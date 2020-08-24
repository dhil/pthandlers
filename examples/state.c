#include<stdio.h>
#include<stdlib.h>
#include "../lib/pthandlers.h"

enum { GET = 100, PUT } STATE_OP_T;

void* countdown(void) {
  long st = (long)pthandlers_perform(GET, NULL);
  if (st == 0) return (void*)0;
  else {
    pthandlers_perform(PUT, (void *)(st - 1));
    return countdown();
  }
}

void* eval_state_ret(void *value, void *param) {
  return (void*)value;
}

void* state_ops(pthandlers_op_t op, pthandlers_resumption_t r, void *st) {
  switch (op->tag) {
  case GET:
    return pthandlers_resume_with(r, st, st);
  case PUT:
    return pthandlers_resume_with(r, NULL, op->value);
  default:
    return pthandlers_reperform(op);
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

int main(void) {
  long n = 100000;
  printf("Countdown from %ld\n", n);
  pthandlers_t eval_state;
  pthandlers_init(&eval_state, eval_state_ret, state_ops, NULL);
  long result = (long)pthandlers_handle(countdown, &eval_state, (void*)n);
  printf("Countdown result with eval_state: %ld\n", result);

  pthandlers_t run_state;
  pthandlers_init(&run_state, run_state_ret, state_ops, NULL);
  pair_t *p = (pair_t*)pthandlers_handle(countdown, &run_state, (void*)n);
  printf("Countdown result with run_state: (%ld, %ld)\n", (long)(p->fst), (long)(p->snd));
  destroy_pair(p);
  return 0;
}
