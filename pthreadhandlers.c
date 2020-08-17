#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>

int countdown_direct(int n);
int countdown_effectful(void);
void eval_state(int st0, void *(*comp)(void), void **result);
void perform(int op, void *payload, void **result);

int main(void) {
  int n = 65440;
  int ret = countdown_direct(n);
  printf("Values: n = %d, ret = %d\n", n, ret);
  fflush(stdout);

  int *result = NULL;
  eval_state(n, (void *(*)(void))countdown_effectful, (void*)&result);
  printf("Printing results.\n");
  printf("eval_state = %d\n", result);
  return 0;
}

typedef struct {
  void *value;
  pthread_mutex_t *mut;
  pthread_cond_t *cond;
} resumption_t;

typedef struct {
  // Operation data.
  int tag;
  void *payload;
  // Resumption.
  resumption_t *resume;
} op_t;

typedef struct {
  pthread_mutex_t *mut;
  pthread_cond_t *cond;
  op_t *op;
} stack_descr_t;

typedef void *(*op_handler_t)(resumption_t *r, void *arg, stack_descr_t *sp);
typedef void *(*ret_handler_t)(stack_descr_t *sp);

// Parent stack pointer
_Thread_local static stack_descr_t *parent_stack_descr;

void perform(int tag, void *payload, void **result) {
  printf("> Performing %d\n", tag);
  // Construct resumption package.
  pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
  resumption_t resume = {*result, &mut, &cond};

  // Publish the operation package.
  pthread_mutex_lock(parent_stack_descr->mut);
  parent_stack_descr->op->tag = tag;
  parent_stack_descr->op->payload = payload;
  parent_stack_descr->op->resume = &resume;
  pthread_mutex_unlock(parent_stack_descr->mut);

  // Initiate control transfer protocol.
  pthread_mutex_lock(&mut);

  // Notify enclosing handler.
  pthread_cond_signal(parent_stack_descr->cond);

  // Await response.
  pthread_cond_wait(&cond, &mut);
  *result = resume.value;
  pthread_mutex_unlock(&mut);
}

int countdown_direct(int n) {
  if (n == 0) return 0;
  else return countdown_direct(n - 1);
}

enum { RET, GET, PUT } op;

int countdown_effectful(void) {
  //printf("Countdown!\n");
  void *r0 = NULL;
  perform(GET, NULL, (void*)&r0);
  int st0 = *((int *)r0);
  if (st0 == 0) return 0;
  else {
    /* void *r1 = NULL; */
    /* perform(GET, NULL, (void*)&r1); */
    /* int st1 = *((int *)r1); */
    int m = st0 - 1;
    void *r2 = NULL;
    perform(PUT, &m, (void*)&r2);
    return countdown_effectful();
  }
}


typedef struct {
  void *(*comp)(void);
  stack_descr_t *stack_descr;
} comp_data_t;

void* initialise_stack(void *inp) {
  printf("> Initialising stack...\n");
  comp_data_t* data = (comp_data_t*)inp;

  // Initialisation protocol.
  parent_stack_descr = data->stack_descr;
  pthread_mutex_lock(parent_stack_descr->mut);
  //pthread_cond_signal(parent_stack_descr->cond);
  printf("> Stack initialised\n");
  pthread_mutex_unlock(parent_stack_descr->mut);

  // Run computation.
  printf("> Starting computation\n");
  void *result = data->comp();

  // Finalisation protocol.
  pthread_mutex_lock(parent_stack_descr->mut);
  parent_stack_descr->op->tag = RET;
  parent_stack_descr->op->payload = result;
  pthread_mutex_unlock(parent_stack_descr->mut);
  pthread_cond_signal(parent_stack_descr->cond);

  return NULL;
}

void *handle_with(resumption_t *r, void *arg, stack_descr_t *stack_descr, int st0) {
  if (r != NULL) {
    printf("< Invoking resumption\n");
    // Invoke the resumption.
    pthread_mutex_lock(r->mut);
    r->value = arg;
    pthread_mutex_unlock(r->mut);
    pthread_cond_signal(r->cond);
  }

  int st = st0; // TODO.
  // Await an event.
  printf("< Awaiting signal\n");
  pthread_cond_wait(stack_descr->cond, stack_descr->mut);
  printf("< Received signal: %d\n", stack_descr->op->tag);
  // Handle the event.
  switch (stack_descr->op->tag) {
  case RET: // Return signal.
    return stack_descr->op->payload;
    break;
  case GET:
    return handle_with(stack_descr->op->resume, &st, stack_descr, st);
    break;
  case PUT:
    st = *((int *)stack_descr->op->payload);
    return handle_with(stack_descr->op->resume, NULL, stack_descr, st);
    break;
  default: // TODO Forward
    break;
  }
  return NULL;
}


void eval_state(int st0, void *(*comp)(void), void **result) {
  pthread_t th;
  /* pthread_attr_t attr; */

  // Construct stack descriptor.
  pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
  op_t *op = (op_t *)malloc(sizeof(op_t));
  stack_descr_t stack_descr = {&mut, &cond, op};

  // Initialise child stack data.
  comp_data_t data = {comp, &stack_descr};

  // New stack initialisation protocol.
  printf("< Initiating stack initialisation protocol\n");
  pthread_mutex_lock(&mut);
  // Initialise a new stack for the computation.
  if (pthread_create(&th, NULL, initialise_stack, &data) != 0) {
    fprintf(stderr, "error: failed to create new thread.\n");
    exit(-1);
  }

  // Waiting for child stack initialisation to finish.
  //pthread_cond_wait(&cond, &mut);
  printf("< Child stack initialisation done.\n");

  // Initiate handling loop.
  printf("< Initiating handling loop\n");
  *result = handle_with(NULL, NULL, &stack_descr, st0);

  // Finalisation protocol.
  printf("< Initiating finalisation protocol\n");
  pthread_mutex_unlock(&mut);
  // Synchronise with child thread.
  pthread_join(th, NULL);
  free(op);
  printf("< Finalisation done.\n");
}
