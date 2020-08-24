#include<stdio.h>
#include<stdlib.h>
#include<limits.h>
#include<pthread.h>
#include "pthandlers.h"

static const int RETURN = -1;
const int PTHANDLERS_EUNHANDLED = -2;

typedef enum { NORMAL, ABORT } _stack_status_t;

struct pthandlers_resumption_t {
  pthandlers_t *handler;
  struct pthandlers_stack_repr_t *top;
  struct pthandlers_stack_repr_t *target;
};

typedef struct pthandlers_stack_repr_t {
  // Communication.
  _stack_status_t status;
  pthread_mutex_t *mut;
  pthread_cond_t *cond;
  void *value;

  // Operation package pointer.
  pthandlers_op_t op;

  // Handler pointer.
  pthandlers_t *handler;

  // Parent stack pointer.
  struct pthandlers_stack_repr_t *parent;
} stack_repr_t;


typedef struct {
  pthandlers_thunk_t comp;
  stack_repr_t *parent_repr;
} _stack_data_t;

static void* generic_handler_return(void *value, void *param) {
  return value;
}

static void* generic_handler_op(pthandlers_op_t op, pthandlers_resumption_t r, void *param) {
  return pthandlers_reperform(op);
}

static void* generic_handler_exn(pthandlers_exn_t exn, void *param) {
  pthandlers_rethrow(exn);
  return NULL; // unreachable.
}

void pthandlers_init( pthandlers_t *handler
                    , pthandlers_ret_handler_t ret_handler
                    , pthandlers_op_handler_t op_handler
                    , pthandlers_exn_handler_t exn_handler) {
  handler->ret = ret_handler == NULL ? generic_handler_return : ret_handler;
  handler->op  = op_handler == NULL ? generic_handler_op : op_handler;
  handler->exn = exn_handler == NULL ? generic_handler_exn : exn_handler;
}

/* Runtime implementation. */
// Thread-local "stack" pointer.
static _Thread_local stack_repr_t *sp = NULL;

static pthandlers_op_t alloc_op(int tag, void *payload, pthandlers_resumption_t r) {
  pthandlers_op_t op = (pthandlers_op_t)malloc(sizeof(struct pthandlers_op_t));
  op->tag = tag;
  op->value = payload;
  op->resumption = r;
  return op;
}

static void destroy_op(pthandlers_op_t op) {
  free(op);
}

static pthandlers_resumption_t alloc_resumption(stack_repr_t *target, stack_repr_t *top) {
  pthandlers_resumption_t r = (pthandlers_resumption_t)malloc(sizeof(struct pthandlers_resumption_t));
  r->target = target;
  r->top = top;
  return r;
}

static void destroy_resumption(pthandlers_resumption_t r) {
  free(r);
}

static void init_stack_repr(  stack_repr_t *repr
                            , pthread_mutex_t *mut, pthread_cond_t *cond
                            , stack_repr_t *parent) {
  repr->status  = NORMAL;
  repr->mut     = mut;
  repr->cond    = cond;
  repr->value   = NULL;
  repr->op      = NULL;
  repr->handler = NULL;
  repr->parent  = parent;
}

static stack_repr_t* alloc_stack_repr(void) {
  stack_repr_t *stack_repr = (stack_repr_t*)malloc(sizeof(stack_repr_t));
  pthread_mutex_t *mut = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(mut, NULL); // TODO check return value.
  pthread_cond_t *cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
  pthread_cond_init(cond, NULL); // TODO check return value.
  init_stack_repr(stack_repr, mut, cond, NULL);
  return stack_repr;
}

static void destroy_stack_repr(stack_repr_t *stack_repr) {
  pthread_mutex_destroy(stack_repr->mut);
  pthread_cond_destroy(stack_repr->cond);
  free(stack_repr->mut);
  free(stack_repr->cond);
  free(stack_repr);
  stack_repr = NULL;
}

static void finalise_stack(_stack_status_t stack_status, pthandlers_op_t op) {
  // Initiate return protocol.
  // Acquire parent stack lock.
  pthread_mutex_lock(sp->parent->mut);

  // Propogate stack status.
  sp->parent->status = stack_status;

  // Publish return package.
  sp->parent->op = op;

  // Notify parent stack and release its lock.
  pthread_cond_signal(sp->parent->cond);
  pthread_mutex_unlock(sp->parent->mut);

  // Destroy stack representation.
  sp = NULL;

  // Kill this thread.
  pthread_exit(NULL);
}

static void* init_stack(void *arg) {
  _stack_data_t *data = (_stack_data_t*)arg;

  // Create and initialise stack structure representation.
  pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
  stack_repr_t stack_repr;
  init_stack_repr(&stack_repr, &mut, &cond, data->parent_repr);
  sp = &stack_repr;

  // The following seemingly odd sequence serve to sequentialise the
  // observable effects.
  pthread_mutex_lock(sp->parent->mut);
  pthread_mutex_unlock(sp->parent->mut);

  // Run computation
  void *result = data->comp();

  // Finalise stack.
  pthandlers_op_t op = alloc_op(RETURN, result, NULL);
  finalise_stack(ABORT, op);

  return NULL;
}

static void* finally_exn(pthandlers_exn_t exn, pthandlers_exn_handler_t handle, void *handler_param) {
  void *result = handle(exn, handler_param);
  destroy_op(exn);
  return result;
}

static void* finally_ret(pthandlers_exn_t exn, pthandlers_ret_handler_t handle, void *handler_param) {
  void *result = handle(exn->value, handler_param);
  destroy_op(exn);
  return result;
}

static void* await(void) {
  // Await an event.
  pthread_cond_wait(sp->cond, sp->mut);

  // Uninstall the current handler.
  pthandlers_t *handler = sp->handler;
  sp->handler = NULL;

  // Consume operation package.
  pthandlers_op_t op = sp->op;
  sp->op = NULL;

  // Set stack operating status.
  sp->status = NORMAL;

  if (op->tag == RETURN) {
    return finally_ret(op, handler->ret, handler->param);
  } else if (op->resumption == NULL) {
    return finally_exn(op, handler->exn, handler->param);
  } else {
    op->resumption->handler = handler;
    return handler->op(op, op->resumption, handler->param);
  }
}

void* pthandlers_handle(pthandlers_thunk_t comp, pthandlers_t *handler, void *handler_param) {
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

  // Acquire current stack lock (if this application is recursive it
  // will already have been acquired).
  pthread_mutex_trylock(sp->mut);

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
  pthread_join(th, NULL);
  pthread_attr_destroy(&attr);

  // Release current stack lock.
  pthread_mutex_unlock(sp->mut);

  // Clean-up this context, if needed.
  if (cleanup_context) {
    destroy_stack_repr(sp);
    sp = NULL;
  }

  return result;
}

void* pthandlers_perform(int tag, void *payload) {
  // An uninitialised context implies that no handler has been
  // installed.
  if (sp == NULL || sp->parent == NULL) abort();

  // Acquire current and target stack locks.
  pthread_mutex_lock(sp->mut);
  pthread_mutex_lock(sp->parent->mut);

  // Publish operation package.
  pthandlers_resumption_t r = alloc_resumption(sp, sp);
  pthandlers_op_t op = alloc_op(tag, payload, r);
  sp->parent->op     = op;


  // Release target stack lock.
  pthread_mutex_unlock(sp->parent->mut);

  // Signal target stack.
  pthread_cond_signal(sp->parent->cond);

  // Await continuation signal.
  pthread_cond_wait(sp->cond, sp->mut);

  // Reclaim operation and resumption packages.
  destroy_op(op);
  destroy_resumption(r);

  // Check for abortive status.
  if (sp->status == ABORT) pthandlers_rethrow(sp->op);

  void *result = sp->value; // TODO FIXME: copy pointer.

  pthread_mutex_unlock(sp->mut);

  return result;
}

void* pthandlers_resume(pthandlers_resumption_t r, void *arg) {
  stack_repr_t *target = r->target;

  // Reinstall handler.
  r->top->parent = sp;
  sp->handler = r->handler;

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

void* pthandlers_resume_with(pthandlers_resumption_t r, void *arg, void *handler_param) {
  // Update handler parameter.
  r->handler->param = handler_param;
  return pthandlers_resume(r, arg);
}

void* pthandlers_abort(pthandlers_resumption_t r, int tag, void *payload) {
  stack_repr_t *target = r->target;

  // Reinstall handler.
  sp->handler = r->handler;

  // Acquire target stack lock.
  pthread_mutex_lock(target->mut);

  // Publish exception.
  target->status     = ABORT;
  target->op         = alloc_op(tag, payload, NULL);

  // Signal target stack.
  pthread_cond_signal(target->cond);

  // Release target stack lock.
  pthread_mutex_unlock(target->mut);

  // Await next event.
  return await();
}

void* pthandlers_reperform(pthandlers_op_t op) {
  // Reinstall handler.
  sp->handler = op->resumption->handler;
  op->resumption->handler = NULL;

  // Acquire parent stack lock.  Lock for current stack should already
  // be acquired prior to invoking forward.
  pthread_mutex_lock(sp->parent->mut);

  // Publish operation package.
  op->resumption->top = sp;
  sp->parent->op = op;

  // Release parent stack lock.
  pthread_mutex_unlock(sp->parent->mut);

  // Signal parent stack.
  pthread_cond_signal(sp->parent->cond);

  return await();
}

void pthandlers_throw(int tag, void *payload) {
  pthandlers_exn_t exn = alloc_op(tag, payload, NULL);
  finalise_stack(ABORT, exn);
}

void pthandlers_rethrow(pthandlers_exn_t exn) {
  finalise_stack(ABORT, exn);
}
