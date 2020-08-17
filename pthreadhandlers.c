#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include "pthreadhandlers.h"

const int RETURN = -1;

typedef struct op_t {
  int tag;
  const void *value;
  resumption_t *resumption;
} op_t;

typedef struct handler_t {
  ret_handler_t ret;
  op_handler_t op;
} handler_t;

typedef struct stack_repr_t {
  // Communication.
  pthread_mutex_t *mut;
  pthread_cond_t *cond;
  void *value;

  // Handler pointer.
  handler_t *handler;

  // Parent pointer.
  struct stack_repr_t *parent;
} stack_repr_t;


typedef struct {
  computation_t comp;
  stack_repr_t *repr;
} stack_data_t;

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
    fprintf(stderr, "error: forwarding not yet implemented.\n");
    exit(-1);
    break;
  }
}

int main(void) {
  init_handler_runtime();
  handler_t h0 = {h0_ret, h0_op};
  handle(simple_loop, &h0);
  fprintf(stdout, "\n");
  destroy_handler_runtime();
  return 0;
}

/* Runtime implementation. */
void* init_stack(void *arg) {
  stack_data_t *data = (stack_data_t*)arg;

  // Setup local stack structure.
  sp = data->repr;

  // Rendezvous with the parent stack.
  /* pthread_mutex_lock(sp->mut); */
  /* pthread_cond_wait(sp->cond, sp->mut); */
  /* pthread_mutex_unlock(sp->mut); */

  // Run computation
  //printf("Running comp.\n");
  void *result = data->comp();

  // Signal return.
  pthread_mutex_lock(sp->parent->mut);
  op_t *ret_op = (op_t *)malloc(sizeof(op_t));
  ret_op->tag = RETURN;
  ret_op->value = result;
  ret_op->resumption = NULL;
  sp->parent->value = ret_op;

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
  if (((op_t*)sp->value)->tag == RETURN) return sp->handler->ret(((op_t*)sp->value)->value);
  else return sp->handler->op((op_t*)sp->value);
}

stack_repr_t* alloc_stack_repr(void) {
  stack_repr_t *stack_repr = (stack_repr_t*)malloc(sizeof(stack_repr_t));
  stack_repr->mut = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(stack_repr->mut, NULL); // TODO check return value.
  stack_repr->cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
  pthread_cond_init(stack_repr->cond, NULL); // TODO check return value.
  stack_repr->value = NULL;
  stack_repr->handler = NULL;
  stack_repr->parent = NULL;
  return stack_repr;
}

int destroy_stack_repr(stack_repr_t *stack_repr) {
  pthread_mutex_destroy(stack_repr->mut);
  pthread_cond_destroy(stack_repr->cond);
  free(stack_repr->mut);
  free(stack_repr->cond);
  free(stack_repr);
  stack_repr = NULL;
  return 0;
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

void* handle(computation_t comp, handler_t *handler) {
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
  stack_data_t *stack_data = (stack_data_t*)malloc(sizeof(stack_data_t));
  stack_data->comp = comp;
  stack_data->repr = child_stack_repr;

  // Acquire current stack lock.
  pthread_mutex_lock(sp->mut);

  // Install handler on the current stack.
  sp->handler = handler;

  // Initialise the child stack.
  if (pthread_create(&th, &attr, init_stack, stack_data) != 0) {
    fprintf(stderr, "error: new stack creation failed.\n");
    exit(-1);
  }

  // Signal child stack.
  /* pthread_cond_signal(child_stack_repr->cond); */

  // Enter handling loop.
  void *result = await();

  // Finalise the child stack
  pthread_join(th, NULL);
  pthread_attr_destroy(&attr);

  destroy_stack_repr(child_stack_repr);
  free(stack_data);

  // Uninstall current handler.
  sp->handler = NULL;
  free(sp->value); // return operation package.

  // Release current stack lock.
  pthread_mutex_unlock(sp->mut);
  return result;
}

void* perform(int tag, const void *payload) {
  //printf("Performing event %d\n", tag);
  // Prepare operation package.
  op_t op = {tag, payload, sp};

  // Acquire current and parent stack lock.
  pthread_mutex_lock(sp->mut);
  pthread_mutex_lock(sp->parent->mut);

  // Publish operation package.
  sp->parent->value = &op;

  // Release parent stack lock.
  pthread_mutex_unlock(sp->parent->mut);

  // Signal parent stack.
  pthread_cond_signal(sp->parent->cond);

  // Await continuation signal.
  pthread_cond_wait(sp->cond, sp->mut);
  void *result = sp->value;
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
