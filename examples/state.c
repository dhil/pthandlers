/*
 * Countdown: repeatedly decrements some ambient state until the
 * stateful value is zero.
 **/

#include<stdio.h>
#include<stdlib.h>
#include "../lib/ptfx.h"

PTFX_DECLARE_EFFECT(state, get, put) {
  PTFX_OP_VOID_PAYLOAD(get, const int*),
  PTFX_OP_VOID_RESULT(put, const int*)
};

int countdown(void) {
  int n;
  while ((n = *((const int*)ptfx_perform0(state, get))) > 0) {
    n = n - 1;
    ptfx_perform(state, put, &n);
  }

  return n;
}

void* runner(void *arg) {
  printf("Countdown from n = %d\n", *((const int*)ptfx_perform0(state, get)));
  printf("Result = %d\n", countdown());
  return NULL;
}

// (Eval)State handler definition.
// Get () r -> r s s
void* hstate_get(ptfx_op_t *op, ptfx_cont_t r, void *param) {
  int *state = (int*)param;
  ptfx_free_op(op);
  return ptfx_resume_deep(r, state, state);
}

// Put s' r -> r () s'
void* hstate_put(ptfx_op_t *op, ptfx_cont_t r, void *param) {
  int *state = (int*)param;
  *state = *((int*)op->payload);
  ptfx_free_op(op);
  return ptfx_resume_deep(r, NULL, state);
}

// Return x -> x
void* hstate_return(void *result, void *param) {
  return result;
}

ptfx_hop hstate_selector(const struct ptfx_op_spec *spec) {
  if (spec == &ptfx_effect_state.get) return hstate_get;
  else if (spec == &ptfx_effect_state.put) return hstate_put;
  else return NULL;
}

int main(void) {
  ptfx_handler_t hstate = { .ret = hstate_return
                          , .eff = hstate_selector };
  int state = 1000000;

  ptfx_cont_t cont = ptfx_new_cont(runner);
  ptfx_resume(cont, NULL, &hstate, &state);

  return 0;
}
