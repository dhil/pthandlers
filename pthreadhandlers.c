#include<stdio.h>
#include<stdlib.h>
#include<limits.h>
#include<pthread.h>
#include "pthreadhandlers.h"

static const int NOOP  = INT_MIN;
static const int RETURN = -1;
const int PTHANDLER_EUNHANDLED = -2;

typedef pthandler_op_t pthandler_exn_t;

struct pthandler_t {
  pthandler_ret_handler_t ret;
  pthandler_op_handler_t op;
  void *param;
};

typedef enum { NORMAL, ABORT } _stack_status_t;

typedef struct pthandler_stack_repr_t {
  // Communication.
  _stack_status_t status;
  pthread_mutex_t *mut;
  pthread_cond_t *cond;
  void *value;

  // Operation package pointer.
  pthandler_op_t *op;
  pthandler_resumption_t backlink;

  // Handler pointer.
  pthandler_t *handler;

  // Parent stack pointer.
  struct pthandler_stack_repr_t *parent;
} stack_repr_t;


typedef struct {
  pthandler_thunk_t comp;
  stack_repr_t *parent_repr;
} _stack_data_t;

/* Example */
enum { DO = 1, GET, PUT };
void* simple_loop(void) {
  printf("Loop start\n");
  int n = 1000000;
  /* n = 74798; */
  for (int i = 0; i < n; i++) {
    fprintf(stdout, "do ");
    fflush(stdout);
    pthandler_perform(DO, NULL);
  }
  return NULL;
}

void* h0_ret(void *value, void *param) {
  return NULL;
}

void* h0_op_do(const pthandler_op_t *op, pthandler_resumption_t r, void *param) {
  /* assert(op.tag == DO); */
  fprintf(stdout, "be ");
  fflush(stdout);
  /* printf("Before first resume\n"); */
  /* pthandler_resume(r, NULL); */
  /* printf("Before second resume\n"); */
  return pthandler_resume(r, NULL);
}

void* h1_ret(void *value, void *param) {
  return NULL;
}

/* void* h1_op(const pthandler_op_t *op, void *param) { */
/*   return pthandler_forward(op); */
/* } */

void* wrapped_simple_loop(void) {
  pthandler_t h1;
  pthandler_init(&h1, h1_ret, NULL);
  void *result = pthandler_handle(simple_loop, &h1, NULL);
  fprintf(stdout, "\n");
  return result;
}

void* countdown(void) {
  long st = (long)pthandler_perform(GET, NULL);
  if (st == 0) return (void*)0;
  else {
    pthandler_perform(PUT, (void *)(st - 1));
    return countdown();
  }
}

void* eval_state_ret(void *value, void *param) {
  return (void*)value;
}

void* state_ops(const pthandler_op_t *op, pthandler_resumption_t r, void *st) {
  switch (op->tag) {
  case GET:
    return pthandler_resume_with(r, st, st);
  case PUT:
    return pthandler_resume_with(r, NULL, op->value);
  default:
    return pthandler_reperform(op);
  }
}

typedef struct pair_t {
  void *fst;
  void *snd;
} pair_t;

pair_t* alloc_pair(void) {
  return (pair_t*)malloc(sizeof(pair_t));
}

int destroy_pair(pair_t *pair) {
  free(pair);
  return 0;
}

pair_t *make_pair(void *fst, void *snd) {
  pair_t *pair = alloc_pair();
  pair->fst = fst;
  pair->snd = snd;
  return pair;
}

void *run_state_ret(void *value, void *param) {
  pair_t *pair = make_pair((void*)value, param);
  return pair;
}

/* void *run_state_op(const pthandler_op_t *op, void *param) { */
/*   return eval_state_op(op, param); */
/* } */

/* long countdown_direct(const void* n) { */
/*   if ((long)n == 0) return 0; */
/*   else return countdown_direct((void*)(((long)n) - 1)); */
/* } */

/* long countdown_loop(const void* n) { */
/*   long m = (long)n; */
/*   while(m != 0) m--; */
/*   return m; */
/* } */

int main(void) {
  /* pthandler_perform(123, NULL); */
  pthandler_t h0;
  pthandler_init(&h0, h0_ret, h0_op_do);
  pthandler_handle(wrapped_simple_loop, &h0, NULL);
  /* destroy_handler(&h0); */

  long n = 100000;

  /* printf("Countdown direct: %ld\n", countdown_direct((void*)n)); */
  /* printf("Countdown loop: %ld\n", countdown_loop((void*)n)); */

  pthandler_t eval_state;
  pthandler_init(&eval_state, eval_state_ret, state_ops);
  long result = (long)pthandler_handle(countdown, &eval_state, (void*)n);
  printf("Countdown effectful: %ld\n", result);

  pthandler_t run_state;
  pthandler_init(&run_state, run_state_ret, state_ops);
  pair_t *p = (pair_t*)pthandler_handle(countdown, &run_state, (void*)n);
  printf("Countdown with run_state: v = %ld, st = %ld\n", (long)(p->fst), (long)(p->snd));
  destroy_pair(p);

  return 0;
}

static void* generic_handler_return(void *value, void *param) {
  return value;
}

static void* generic_handler_op(const pthandler_op_t *op, pthandler_resumption_t r, void *param) {
  return pthandler_reperform(op);
}

void pthandler_init(  pthandler_t *handler
                    , pthandler_ret_handler_t ret_handler
                    , pthandler_op_handler_t op_handler ) {
  handler->ret = ret_handler == NULL ? generic_handler_return : ret_handler;
  handler->op  = op_handler == NULL ? generic_handler_op : op_handler;
}

/* Runtime implementation. */
// Thread-local "stack" pointer.
static _Thread_local stack_repr_t *sp = NULL;

static void init_stack_repr(  stack_repr_t *repr
                            , pthread_mutex_t *mut, pthread_cond_t *cond
                            , pthandler_op_t *op
                            , stack_repr_t *parent) {
  repr->status  = NORMAL;
  repr->mut     = mut;
  repr->cond    = cond;
  repr->value   = NULL;
  repr->op      = op;
  repr->handler = NULL;
  repr->parent  = parent;
}

static stack_repr_t* alloc_stack_repr(void) {
  stack_repr_t *stack_repr = (stack_repr_t*)malloc(sizeof(stack_repr_t));
  pthread_mutex_t *mut = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(mut, NULL); // TODO check return value.
  pthread_cond_t *cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
  pthread_cond_init(cond, NULL); // TODO check return value.
  pthandler_op_t *op = (pthandler_op_t*)malloc(sizeof(pthandler_op_t));
  init_stack_repr(stack_repr, mut, cond, op, NULL);
  return stack_repr;
}

static void destroy_stack_repr(stack_repr_t *stack_repr) {
  pthread_mutex_destroy(stack_repr->mut);
  pthread_cond_destroy(stack_repr->cond);
  /* destroy_op(stack_repr->op); */
  free(stack_repr->mut);
  free(stack_repr->cond);
  free(stack_repr->op);
  free(stack_repr);
  stack_repr = NULL;
}

static void finalise_stack(int tag, void *value, int exitcode) {
  // Initiate return protocol.
  // Acquire parent stack lock.
  pthread_mutex_lock(sp->parent->mut);

  // Publish return package.
  sp->parent->op->tag = tag;
  sp->parent->op->value = value;

  // Notify parent stack and release its lock.
  pthread_cond_signal(sp->parent->cond);
  pthread_mutex_unlock(sp->parent->mut);

  // Destroy stack representation.
  sp = NULL;

  // Prepare stack completion signal.
  int *status = (int*)malloc(sizeof(int));
  *status = exitcode;

  pthread_exit((void*)status);
}

static void* init_stack(void *arg) {
  _stack_data_t *data = (_stack_data_t*)arg;

  // Create and initialise stack structure representation.
  pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
  pthandler_op_t op = { NOOP, NULL };
  stack_repr_t stack_repr;
  init_stack_repr(&stack_repr, &mut, &cond, &op, data->parent_repr);
  sp = &stack_repr;

  // Run computation
  void *result = data->comp();

  // Finalise stack.
  finalise_stack(RETURN, result, 0);

  return NULL;
}

static void* await(void) {
  // Await an event.
  pthread_cond_wait(sp->cond, sp->mut);

  if (sp->op->tag == RETURN) return sp->handler->ret(sp->op->value, sp->handler->param);
  else return sp->handler->op(sp->op, sp->backlink, sp->handler->param);
}

void* pthandler_handle(pthandler_thunk_t comp, pthandler_t *handler, void *handler_param) {
  // Check whether the current context has been initialised.
  int cleanup_context = 0;
  if (sp == NULL) {
    sp = alloc_stack_repr();
    cleanup_context = 1;
  }

  // Prepare a new child stack.
  pthread_t th;
  pthread_attr_t attr;

  if (pthread_attr_init(&attr) != 0) {
    fprintf(stderr, "error: new stack attribute initialisation failed.\n");
    exit(-1);
  }
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  // Prepare stack initialisation payload.
  _stack_data_t stack_data = {comp, sp};

  // Acquire current stack lock.
  pthread_mutex_lock(sp->mut);

  // Install handler on the current stack.
  sp->handler = handler;
  sp->handler->param = handler_param;

  // Initialise the child stack.
  if (pthread_create(&th, &attr, init_stack, &stack_data) != 0) {
    fprintf(stderr, "error: new stack creation failed.\n");
    exit(-1);
  }

  // Enter handling loop.
  void *result = await();

  // Finalise the child stack
  void *status;
  pthread_join(th, &status);
  pthread_attr_destroy(&attr);
  free(status); // TODO FIXME inspect status.

  // Uninstall current handler.
  sp->handler = NULL;

  // Release current stack lock.
  pthread_mutex_unlock(sp->mut);

  // Clean-up this context, if needed.
  if (cleanup_context) {
    destroy_stack_repr(sp);
    sp = NULL;
  }

  return result;
}

/* static stack_repr_t* locate_handler(int tag, stack_repr_t *sp) { */
/*   // An uninitialised context implies that there is no suitable */
/*   // handler for `tag` installed. */
/*   if (sp == NULL) return NULL; */

/*   if (sp->handler->default_op_handler != NULL) return sp; */

/*   for (int i = 0; i < sp->handler->num_op_clauses; i++) { */
/*     if (sp->handler->op_clauses[i].tag == tag) return sp; */
/*   } */

/*   return locate_handler(tag, sp->parent); */
/* } */

void* pthandler_perform(int tag, void *payload) {
  // An uninitialised context implies that no handler has been
  // installed.
  if (sp == NULL || sp->parent == NULL) abort();

  // Acquire current and target stack locks.
  pthread_mutex_lock(sp->mut);
  pthread_mutex_lock(sp->parent->mut);

  // Publish operation package.
  sp->parent->op->tag = tag;
  sp->parent->op->value = payload;
  sp->parent->backlink = sp;

  // Release target stack lock.
  pthread_mutex_unlock(sp->parent->mut);

  // Signal target stack.
  pthread_cond_signal(sp->parent->cond);

  // Await continuation signal.
  pthread_cond_wait(sp->cond, sp->mut);

  // Check for abortive status.
  if (sp->status == ABORT) pthandler_throw(sp->op->tag, sp->op->value);

  void *result = sp->value; // TODO FIXME: copy pointer.

  pthread_mutex_unlock(sp->mut);

  return result;
}

void* pthandler_resume(pthandler_resumption_t r, void *arg) {
  stack_repr_t *target = r;

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

void* pthandler_resume_with(pthandler_resumption_t r, void *arg, void *handler_param) {
  stack_repr_t *target = r;

  // Acquire target stack lock.
  pthread_mutex_lock(target->mut);

  // Publish the argument.
  target->value = arg;

  // Update own handler parameter.
  sp->handler->param = handler_param;

  // Signal readiness.
  pthread_cond_signal(target->cond);

  // Release target stack lock.
  pthread_mutex_unlock(target->mut);

  // Await next event.
  return await();
}

void* pthandler_abort(pthandler_resumption_t r, int tag, void *payload) {
  stack_repr_t *target = r;

  // Acquire target stack lock.
  pthread_mutex_lock(target->mut);

  // Publish exception.
  target->status = ABORT;
  target->op->tag = tag;
  target->op->value = payload;

  // Signal target stack.
  pthread_cond_signal(target->cond);

  // Release target stack lock.
  pthread_mutex_unlock(target->mut);

  // Await next event.
  return await();
}

/* void* pthandler_forward(pthandler_op_t op) { */
/*   // Acquire parent stack lock.  Lock for current stack should already */
/*   // be acquired prior to invoking forward. */
/*   pthread_mutex_lock(sp->parent->mut); */

/*   // Publish operation package. */
/*   sp->parent->op->tag        = op->tag; */
/*   sp->parent->op->value      = op->value; */
/*   sp->parent->op->resumption = op->resumption; */

/*   // Release parent stack lock. */
/*   pthread_mutex_unlock(sp->parent->mut); */

/*   // Signal parent stack. */
/*   pthread_cond_signal(sp->parent->cond); */

/*   // Await continuation signal. */
/*   pthread_cond_wait(sp->cond, sp->mut); */

/*   return pthandler_resume(r, sp->value); */
/* } */

/* pthandler_exn_t* pthandler_exn_create(int tag, void *payload) { */
/*   pthandler_op_t *exn = alloc_op(); */
/*   exn->tag = tag; */
/*   exn->value = payload; */
/*   return (pthandler_exn_t*)exn; */
/* } */

/* void pthandler_exn_destroy(pthandler_exn_t *exn) { */
/*   destroy_op((pthandler_op_t*)exn); */
/* } */

void* pthandler_reperform(const pthandler_op_t *op) {
  // Acquire parent stack lock.  Lock for current stack should already
  // be acquired prior to invoking forward.
  pthread_mutex_lock(sp->parent->mut);

  // Publish operation package.
  sp->parent->op->tag        = op->tag;
  sp->parent->op->value      = op->value;
  sp->parent->backlink       = sp->backlink;
  sp->backlink = NULL;

  // Release parent stack lock.
  pthread_mutex_unlock(sp->parent->mut);

  // Signal parent stack.
  pthread_cond_signal(sp->parent->cond);

  return await();
}

void pthandler_throw(int tag, void *payload) {
  finalise_stack(tag, payload, 0);
}
