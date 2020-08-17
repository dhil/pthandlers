#include<stdio.h>
#include<stdlib.h>
#include<limits.h>
#include<pthread.h>
#include "pthreadhandlers.h"

const int EMPTY  = INT_MIN;
const int RETURN = -1;

typedef struct op_t {
  int tag;
  const void *value;
  resumption_t *resumption;
} op_t;

struct handler_t {
  ret_handler_t ret;
  op_handler_t op;
  size_t num_opcodes;
  int* opcodes;
};

typedef struct handler_t* handler_t;

typedef enum { NORMAL, ABORT } stack_status_t;

typedef struct stack_repr_t {
  // Communication.
  stack_status_t status;
  pthread_mutex_t *mut;
  pthread_cond_t *cond;
  void *value;

  // Operation package pointer.
  op_t *op;

  // Handler pointer.
  handler_t handler;

  // Parent stack pointer.
  struct stack_repr_t *parent;
} stack_repr_t;


typedef struct {
  computation_t comp;
  stack_repr_t *repr;
} stack_data_t;

void *forward(const op_t *op);

// Thread-local "stack" pointer.
static _Thread_local stack_repr_t *sp = NULL;

/* Example */
enum { DO = 1 };
void* simple_loop(void) {
  printf("Loop start\n");
  int n = 1000000;
  /* n = 74798; */
  for (int i = 0; i < n; i++) {
    fprintf(stdout, "do ");
    fflush(stdout);
    perform(DO, NULL);
  }
  return NULL;
}

void* h0_ret(const void *value) {
  return NULL;
}

void* h0_op(const op_t *op) {
  switch (op->tag) {
  case DO:
    fprintf(stdout, "be ");
    fflush(stdout);
    return resume(op->resumption, NULL);
    break;
  default:
    return forward(op);
    /* fprintf(stderr, "error: forwarding not yet implemented.\n"); */
    /* exit(-1); */
    break;
  }
}

void* h1_ret(const void *value) {
  return NULL;
}

void* h1_op(const op_t *op) {
  return forward(op);
}

void* wrapped_simple_loop(void) {
  handler_t h1;
  int opcodes[] = {1};
  init_handler(&h1, h1_ret, h1_op, 0, opcodes);
  void *result = handle(simple_loop, h1);
  fprintf(stdout, "\n");
  destroy_handler(&h1);
  return result;
}

int main(void) {
  init_handler_runtime();
  handler_t h0;
  int opcodes[] = {1};
  init_handler(&h0, h0_ret, h0_op, 0, opcodes);
  handle(wrapped_simple_loop, h0);
  destroy_handler(&h0);
  destroy_handler_runtime();
  return 0;
}

/* Runtime implementation. */
void* init_stack(void *arg) {
  stack_data_t *data = (stack_data_t*)arg;

  // Setup local stack structure.
  sp = data->repr;

  // Run computation
  void *result = data->comp();

  // Initiate return protocol.
  // Acquire parent stack lock.
  pthread_mutex_lock(sp->parent->mut);

  // Publish return package.
  sp->parent->op->tag = RETURN;
  sp->parent->op->value = result;
  sp->parent->op->resumption = NULL;

  // Notify parent stack and release its lock.
  pthread_cond_signal(sp->parent->cond);
  pthread_mutex_unlock(sp->parent->mut);

  return NULL;
}

void* await(void) {
  //printf("Awaiting\n");
  // Await an event.
  /* void *result = NULL; */
  pthread_cond_wait(sp->cond, sp->mut);
  //printf("Received event\n");
  if (sp->op->tag == RETURN) return sp->handler->ret(sp->op->value);
  else return sp->handler->op(sp->op);
}

op_t* alloc_op(void) {
  op_t *op = (op_t*)malloc(sizeof(op_t));
  op->tag = EMPTY;
  op->value = NULL;
  op->resumption = NULL;
  return op;
}

int destroy_op(op_t *op) {
  free(op);
  return 0;
}

stack_repr_t* alloc_stack_repr(void) {
  stack_repr_t *stack_repr = (stack_repr_t*)malloc(sizeof(stack_repr_t));
  stack_repr->mut = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(stack_repr->mut, NULL); // TODO check return value.
  stack_repr->cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
  pthread_cond_init(stack_repr->cond, NULL); // TODO check return value.
  stack_repr->value = NULL;
  stack_repr->op = alloc_op();
  stack_repr->handler = NULL;
  stack_repr->parent = NULL;
  stack_repr->status = NORMAL;
  return stack_repr;
}

int destroy_stack_repr(stack_repr_t *stack_repr) {
  pthread_mutex_destroy(stack_repr->mut);
  pthread_cond_destroy(stack_repr->cond);
  destroy_op(stack_repr->op);
  free(stack_repr->mut);
  free(stack_repr->cond);
  free(stack_repr);
  stack_repr = NULL;
  return 0;
}

/* int handles(const handler_t handler, const op_t *op) { */
/*   // Conservatively return true if the handler's operation interface */
/*   // has not been published. */
/*   if (handler->num_opcodes <= 0) return 1; */

/*   for (int i = 0; i < handler->num_opcodes; i++) { */
/*     if (handler->opcodes[i] == op->tag) return 1; */
/*   } */
/*   return 0; */
/* } */

/* int dispatch(stack_repr_t *sp, const op_t *op) { */
/*   if (sp == NULL || sp->handler == NULL) return -1; // TODO FIXME. */
/*   if (handles(sp->handler, op)) { */
    
/*   } else { */
/*     return dispatch(sp->parent, op); */
/*   } */
/* } */

void *forward(const op_t *op) {
  // Acquire current and parent stack lock.  Lock for current stack
  // should already be acquired prior to invoking forward.
  /* pthread_mutex_lock(sp->mut); */
  pthread_mutex_lock(sp->parent->mut);

  // Publish operation package.
  sp->parent->op->tag = op->tag;
  sp->parent->op->value = op->value;
  sp->parent->op->resumption = sp;

  // Release parent stack lock.
  pthread_mutex_unlock(sp->parent->mut);

  // Signal parent stack.
  pthread_cond_signal(sp->parent->cond);

  // Await continuation signal.
  pthread_cond_wait(sp->cond, sp->mut);
  /* void *result = sp->value; */
  /* pthread_mutex_unlock(sp->mut); */

  /* return result; */
  return resume(op->resumption, sp->value);
}

void abort_stack(void) {
  fprintf(stderr, "error: abort stack not yet implemented.\n");
  exit(-1);
}

/* Implementation of public interface. */
int init_handler_runtime(void) {
  sp = alloc_stack_repr();
  return 0;
}

int destroy_handler_runtime(void) {
  destroy_stack_repr(sp);
  sp = NULL;
  return 0;
}

int init_handler(handler_t *handler, ret_handler_t ret_handler, op_handler_t op_handler, size_t num_opcodes, int* opcodes) {
  handler_t new_handler = (handler_t)malloc(sizeof(handler_t));
  new_handler->ret = ret_handler;
  new_handler->op  = op_handler;
  new_handler->num_opcodes = num_opcodes;
  new_handler->opcodes = opcodes;
  *handler = new_handler;
  return 0;
}

int destroy_handler(handler_t *handler) {
  free(*handler);
  handler = NULL;
  return 0;
}

void* handle(computation_t comp, handler_t handler) {
  // Prepare a new child stack.
  pthread_t th;
  pthread_attr_t attr;

  if (pthread_attr_init(&attr) != 0) {
    fprintf(stderr, "error: new stack attribute initialisation failed.\n");
    exit(-1);
  }
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  // Allocate stack structure representation.
  stack_repr_t *child_stack_repr = alloc_stack_repr();
  child_stack_repr->parent = sp;

  // Prepare stack initialisation payload.
  stack_data_t stack_data = {comp, child_stack_repr};

  // Acquire current stack lock.
  pthread_mutex_lock(sp->mut);

  // Install handler on the current stack.
  sp->handler = handler;

  // Initialise the child stack.
  if (pthread_create(&th, &attr, init_stack, &stack_data) != 0) {
    fprintf(stderr, "error: new stack creation failed.\n");
    exit(-1);
  }

  // Enter handling loop.
  void *result = await();

  // Finalise the child stack
  pthread_join(th, NULL);
  pthread_attr_destroy(&attr);

  destroy_stack_repr(child_stack_repr);

  // Uninstall current handler.
  sp->handler = NULL;

  // Release current stack lock.
  pthread_mutex_unlock(sp->mut);
  return result;
}

void* perform(int tag, const void *payload) {
  //printf("Performing event %d\n", tag);

  // Acquire current and parent stack lock.
  pthread_mutex_lock(sp->mut);
  pthread_mutex_lock(sp->parent->mut);

  // Publish operation package.
  sp->parent->op->tag = tag;
  sp->parent->op->value = payload;
  sp->parent->op->resumption = sp;

  // Release parent stack lock.
  pthread_mutex_unlock(sp->parent->mut);

  // Signal parent stack.
  pthread_cond_signal(sp->parent->cond);

  // Await continuation signal.
  pthread_cond_wait(sp->cond, sp->mut);
  // Check for abortive status.
  if (sp->status == ABORT) abort_stack();
  void *result = sp->value; // TODO FIXME: copy pointer.
  pthread_mutex_unlock(sp->mut);

  return result;
}

void* resume(resumption_t *r, void *arg) {
  stack_repr_t* target = r;

  // Acquire target stack lock.
  pthread_mutex_lock(target->mut);

  // Publish the argument.
  target->value = arg;

  // Signal readiness.
  pthread_cond_signal(target->cond);

  // Release target stack lock.
  pthread_mutex_unlock(target->mut);

  // Await next event.
  return await();
}

void* abort_(resumption_t *r, exn_t *e) {
  stack_repr_t *target = r;
  // Acquire target stack lock.
  pthread_mutex_lock(target->mut);

  // Publish exception.
  target->status = ABORT;
  target->value = e;

  // Signal target stack.
  pthread_cond_signal(target->cond);

  // Release target stack lock.
  pthread_mutex_unlock(target->mut);

  // Await next event.
  return await();
}
