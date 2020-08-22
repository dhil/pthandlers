#include<stdio.h>
#include "../lib/pthandlers.h"

enum { DIV_ZERO = 200 };

static int _abort = 0;

void* hdiv_zero_op(const pthandlers_exn_t *op, pthandlers_resumption_t r, void *param) {
  switch (op->tag) {
  case DIV_ZERO:
    if (_abort) {
      fprintf(stderr, "Aborting resumption...\n");
      void *result = pthandlers_abort(r, DIV_ZERO, NULL);
      fprintf(stderr, "The resumption was successfully aborted.\n");
      return result;
    } else {
      return pthandlers_resume(r, (void*)0);
    }
  default:
    return pthandlers_reperform(op);
  }
}

void* hdiv_zero_exn(const pthandlers_exn_t *exn, void *param) {
  fprintf(stderr, "Division by zero!\n");
  return (void*)0;
}

int safe_div(int n, int d) {
  if (d == 0) return (long)pthandlers_perform(DIV_ZERO, NULL);
  else return n / d;
}

void* div_zero(void) {
  printf("8 / 0 = %d\n", safe_div(8, 0));
  return (void*)42;
}

int main(void) {
  pthandlers_t hdiv;
  pthandlers_init(&hdiv, NULL, hdiv_zero_op, hdiv_zero_exn);

  long result = (long)pthandlers_handle(div_zero, &hdiv, NULL);
  printf("non-abortive hdiv(div_zero()) = %ld\n", result);

  _abort = 1;
  result = (long)pthandlers_handle(div_zero, &hdiv, NULL);
  printf("abortive hdiv(div_zero()) = %ld\n", result);

  return 0;
}
