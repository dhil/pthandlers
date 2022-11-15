#include<stdio.h>
#include<stdlib.h>
#include<limits.h>
#include<pthread.h>
#include "ptfx.h"

typedef enum event_type { NONE, PERFORMING, RETURNING, ABORTING } event_type_t;

// Stack representation.
typedef struct stack_repr {
  // Control mechanisms for *this* stack.
  pthread_t th;
  pthread_attr_t attr;
  pthread_mutex_t lock;
  pthread_cond_t signal;

  // Storage.
  void *argument;
  event_type_t event_type;

  // Pointer to parent stack.
  struct stack_repr *parent;

  // Pointer to the handler for *child* stacks.
  ptfx_handler_t *handler;
  void *parameter;
} stack_repr_t;

/* // Continuation representation. */
/* struct ptfx_cont { */
/*   stack_repr_t *top; */
/*   stack_repr_t *target; */
/* }; */

/* // Operation package representation. */
/* struct ptfx_op { */
/*   struct ptfx_op_spec *tag; */
/*   void *arg; */
/*   ptfx_cont_t resume; */
/* }; */

/* static struct ptfx_op_spec RETURN_SPEC = {sizeof(void*), 0, "RETURN"}; */
/* static struct ptfx_op RETURN = {&RETURN_SPEC, NULL, NULL}; */

/* Runtime implementation. */
// Thread-local "stack" pointer.
static __thread stack_repr_t *sp = NULL;

static stack_repr_t* alloc_stack_repr(stack_repr_t *parent) {
  stack_repr_t *stack_repr = (stack_repr_t*)malloc(sizeof(stack_repr_t));

  /* stack_repr->attr = (pthread_attr_t*)malloc(sizeof(pthread_attr_t)); */
  pthread_attr_init(&stack_repr->attr); // TODO check return value

  /* stack_repr->lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)); */
  pthread_mutex_init(&stack_repr->lock, NULL); // TODO check return value

  /* stack_repr->signal = (pthread_cond_t*)malloc(sizeof(pthread_cond_t)); */
  pthread_cond_init(&stack_repr->signal, NULL); // TODO check return value

  stack_repr->argument = NULL;
  stack_repr->event_type = NONE;

  stack_repr->parent  = parent;
  stack_repr->handler = NULL;

  return stack_repr;
}

static void free_stack_repr(stack_repr_t *stack_repr) {
  /* pthread_mutex_destroy(stack_repr->lock); */
  /* pthread_cond_destroy(stack_repr->signal); */
  /* pthread_attr_destroy(stack_repr->attr); */
  /* free(stack_repr->lock); */
  /* free(stack_repr->signal); */
  /* free(stack_repr->attr); */
  free(stack_repr);
  stack_repr = NULL;
  return;
}

struct stack_data {
  void *(*fun)(void*);
  stack_repr_t *stack_repr;
};

static void* runstack(void *stkdata) {
  struct stack_data* data = (struct stack_data*)stkdata;
  sp = data->stack_repr;

  // Acquire own lock.
  pthread_mutex_lock(&sp->lock);

  // Acquire parent's lock
  pthread_mutex_lock(&sp->parent->lock);

  // Extract the stack routine.
  void *(*fun)(void*) = data->fun;

  // Signal the parent that this stack is ready to run.
  pthread_cond_signal(&sp->parent->signal);
  pthread_mutex_unlock(&sp->parent->lock);

  // Wait on parent stack to hand over control.
  pthread_cond_wait(&sp->signal, &sp->lock);

  // Extract routine argument.
  void *arg = sp->argument;

  // Force the application.
  void *result = fun(arg);

  /* Return protocol */
  // Acquire parent's lock.
  pthread_mutex_lock(&sp->parent->lock);

  // The parent must initiate the clean up protocol.
  sp->parent->argument = sp;
  sp->parent->event_type = RETURNING;

  // Signal parent
  pthread_cond_signal(&sp->parent->signal);

  // Release parent's lock.
  pthread_mutex_unlock(&sp->parent->lock);

  // Release own lock.
  pthread_mutex_unlock(&sp->lock);

  // Return the result.
  pthread_exit(result);
  return NULL;
}

static ptfx_hop heff_default(const struct ptfx_op_spec *spec) {
  return NULL;
}

static void* hret_default(void *x, void *p) { return x; }

static ptfx_handler_t hdefault = { .ret = hret_default
                                 , .eff = heff_default };

static void initialise_main_stack(void) {
  // Allocate the meta stack.
  sp = alloc_stack_repr(NULL);

  // Install the identity handler.
  sp->handler = &hdefault;

  // Acquire own lock.
  pthread_mutex_lock(&sp->lock);
  return;
}

static stack_repr_t* create_stack(void *(*routine)(void*)) {
  // Initialise the current stack, if need be.
  if (sp == NULL) initialise_main_stack();

  // Allocate a new stack structure.
  stack_repr_t *stack_repr = alloc_stack_repr(sp);
  struct stack_data data = {routine, stack_repr};

  // Initiate the new child stack.
  if (pthread_create(  &stack_repr->th
                     , &stack_repr->attr
                     , runstack
                     , &data) != 0) {
    fprintf(stderr, "error: new stack creation failed.\n");
    exit(-1);
  }

  // Wait on child to signal ready.
  pthread_cond_wait(&sp->signal, &sp->lock);

  return stack_repr;
}

ptfx_cont_t ptfx_new_cont(void *(*routine)(void*)) {
  /* struct ptfx_cont *r = malloc(sizeof(struct ptfx_cont)); */
  /* r->target = create_stack(routine); */
  /* r->top    = r->target; */
  stack_repr_t *metastack = create_stack(routine);
  ptfx_cont_t r = { .top = metastack, .target = metastack };
  return r;
}

void ptfx_free_op(ptfx_op_t *op) {
  free(op);
  op = NULL;
  return;
}

static void* await(void) {
  // Await an event.
  pthread_cond_wait(&sp->signal, &sp->lock);

  ptfx_handler_t *handler = NULL;
  event_type_t event_type = sp->event_type;
  sp->event_type = NONE;

  switch (event_type) {
    case NONE:
      fprintf(stderr, "Read NONE\n");
      return NULL;
    case ABORTING:
      // TODO
      return NULL;
    case RETURNING: {
      // Extract the meta stack.
      stack_repr_t *child = (stack_repr_t*)sp->argument;
      // Join with the child.
      pthread_join(child->th, sp->argument);

      // Clean up the meta stack.
      free_stack_repr(child);

      // Fetch the current handler.
      handler = sp->handler;
      // Invoke the return case.
      return handler->ret(sp->argument, sp->parameter);
    }
    case PERFORMING: {
      // Fetch the operation package.
      ptfx_op_t *op = (ptfx_op_t*)sp->argument;

      // Fetch the current handler.
      ptfx_handler_t *handler = sp->handler;

      /* // Uninstall the current handler. */
      /* sp->handler = NULL; */

      // Run the case computation.
      return handler->eff(op->spec)(op, op->resume, sp->parameter);
    }
  }
  return NULL; // Impossible
}

void* ptfx_resume(  ptfx_cont_t resume
                  , void *arg
                  , ptfx_handler_t *h
                  , void *param ) {
  // Retrieve target stack.
  stack_repr_t *target = resume.target;

  // Acquire target's lock.
  pthread_mutex_lock(&target->lock);

  // Attach the top segment in the stack chain to this stack.
  resume.top->parent = sp;


  // Clean up continuation package.
  /* free(resume); */
  /* resume = NULL; */

  // Copy the argument onto the target stack.
  target->argument = arg;

  // Install the new handler.
  sp->handler = h;

  // Update parameter.
  sp->parameter = param;

  // Signal target stack.
  pthread_cond_signal(&target->signal);

  // Release target stack
  pthread_mutex_unlock(&target->lock);

  // Await next event.
  return await();
}

void* ptfx_resume_deep( ptfx_cont_t resume
                      , void *arg
                      , void *param ) {
  return ptfx_resume(resume, arg, sp->handler, param);
}

void* ptfx_resume_shallow( ptfx_cont_t resume
                         , void *arg
                         , void *param ) {
  return ptfx_resume(resume, arg, &hdefault, param);
}

void* _ptfx_perform(  const char *eff
                    , struct ptfx_op_spec *spec
                    , void *arg ) {
  /* printf("Performing %s.%s with sizes (%d, %d)\n" */
  /*        , eff, spec->name, spec->arg_size, spec->res_size); */

  // Walk the stack chain to find a suitable handler.
  stack_repr_t *prev = sp;
  stack_repr_t *target = sp->parent;

  while (target != NULL) {
    if (target->handler->eff(spec) != NULL) break;
    prev = target;
    target = target->parent;
  }

  if (target == NULL) {
    fprintf(stderr, "Unhandled operation %s.%s", eff, spec->name);
    exit(-1);
  }


  // Construct the continuation
  /* ptfx_cont_t cont = (ptfx_cont_t)malloc(sizeof(struct ptfx_cont)); */
  /* cont->top = prev; */
  /* cont->target = sp; */
  ptfx_cont_t cont = { .top = prev, .target = sp };

  // Setup operation package.
  ptfx_op_t *op = (ptfx_op_t*)malloc(sizeof(ptfx_op_t));
  op->spec = spec;
  op->payload = arg;
  op->resume = cont;

  // Acquire target lock.
  pthread_mutex_lock(&target->lock);

  // Copy operation package.
  target->argument = op;
  target->event_type = PERFORMING;

  // Signal target
  pthread_cond_signal(&target->signal);

  // Release target lock.
  pthread_mutex_unlock(&target->lock);

  // Await response
  pthread_cond_wait(&sp->signal, &sp->lock);

  // TODO check whether to unwind

  return sp->argument;
}
