#include<stdio.h>
#include "../lib/pthandlers.h"

enum { DO = 20 };

void* simple_loop(void) {
  int n = 1000000;
  for (int i = 0; i < n; i++) {
    fprintf(stdout, "do ");
    fflush(stdout);
    pthandlers_perform(DO, NULL);
  }
  return NULL;
}

void* h0_ret(void *value, void *param) {
  return NULL;
}

void* h0_op_do(pthandlers_op_t op, pthandlers_resumption_t r, void *param) {
  fprintf(stdout, "be ");
  fflush(stdout);
  return pthandlers_resume(r, NULL);
}

void* wrapped_simple_loop(void) {
  pthandlers_t h1;
  pthandlers_init(&h1, NULL, NULL, NULL);
  void *result = pthandlers_handle(simple_loop, &h1, NULL);
  fprintf(stdout, "\n");
  return result;
}

int main(void) {
  pthandlers_t h0;
  pthandlers_init(&h0, h0_ret, h0_op_do, NULL);
  pthandlers_handle(wrapped_simple_loop, &h0, NULL);
  return 0;
}
