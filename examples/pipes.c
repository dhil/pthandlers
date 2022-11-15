/* Pipes example. Adapted from Kammar et al. (2013) */

#include<stdio.h>
#include<stdlib.h>
#include "../lib/ptfx.h"

// Producer & Consumer effect declarations
PTFX_DECLARE_EFFECT(producer, yield) {
  PTFX_OP_VOID_RESULT(yield, const int*)
};

PTFX_DECLARE_EFFECT(consumer, await) {
  PTFX_OP_VOID_PAYLOAD(await, const int*)
};

// Producer and & Consumer handler declarations
ptfx_handler_t *hpipe, *hcopipe;

static ptfx_cont_t update_param(ptfx_cont_t r, void *param) {
  ptfx_cont_t *k = (ptfx_cont_t*)param;
  ptfx_cont_t resume = *k;
  *k = r;
  return resume;
}

// Producer & Consumer implementations
void* hpipe_eff(ptfx_op_t *op, ptfx_cont_t sender, void *param) {
  int *arg = (int*)op->payload;
  ptfx_free_op(op);
  ptfx_cont_t receiver = update_param(sender, param);
  return ptfx_resume(receiver, arg, hcopipe, param);
}

void* hcopipe_eff(ptfx_op_t *op, ptfx_cont_t receiver, void *param) {
  ptfx_free_op(op);
  ptfx_cont_t sender = update_param(receiver, param);
  return ptfx_resume(sender, NULL, hpipe, param);
}

ptfx_hop hpipe_selector(const struct ptfx_op_spec *spec) {
  if (spec == &ptfx_effect_producer.yield) return hpipe_eff;
  else return NULL;
}

ptfx_hop hcopipe_selector(const struct ptfx_op_spec *spec) {
  if (spec == &ptfx_effect_consumer.await) return hcopipe_eff;
  else return NULL;
}

void* hreturn(void *result, void *param) {
  return result;
}

// Producer & Consumer prototypes
void* pipe(void* (*producer)(void *arg), void* (*consumer)(void *arg)) {
  ptfx_cont_t p = ptfx_new_cont(producer), c = ptfx_new_cont(consumer);
  return ptfx_resume(c, NULL, hcopipe, &p);
}


// Concrete producer
void* naturals(void *arg) {
  for (int i = 0; i < 64; i++)
    ptfx_perform(producer, yield, &i);
  ptfx_perform(producer, yield, NULL);
  return NULL;
}

// Concrete consumer
void* evens(void *arg) {
  while (1) {
    const int *j = ptfx_perform0(consumer, await);
    if (j == NULL) break;
    if (*j % 2 == 0) printf("%d ", *j);
  }
  printf("\n");
  return NULL;
}

int main(void) {
  // Initialise hpipe & hcopipe
  ptfx_handler_t hpipe_def   = { .ret = hreturn
                               , .eff = hpipe_selector };
  ptfx_handler_t hcopipe_def = { .ret = hreturn
                               , .eff = hcopipe_selector };
  hpipe = &hpipe_def;
  hcopipe = &hcopipe_def;

  pipe(naturals, evens);

  return 0;
}
