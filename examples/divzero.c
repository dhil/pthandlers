#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "../lib/pthandlers.h"

enum { DIV_ZERO = 200, READ_INT, BAD_INPUT };

static int _abort = 0;

int read_int(void) {
  int *result = (int*)pthandlers_perform(READ_INT, NULL);
  int i = *result;
  free(result);
  return i;
}

void* hread_static_int_op(const pthandlers_op_t *op, pthandlers_resumption_t r, int *ptr) {
  static int inputs[] = {8, 0, 8, 0};
  int *integer = NULL;
  switch (op->tag) {
  case READ_INT:
    integer = (int*)malloc(sizeof(int));
    *integer = inputs[*ptr];
    *ptr += 1;
    printf("%d\n", *integer);
    return pthandlers_resume_with(r, integer, ptr);
  default:
    return pthandlers_reperform(op);
  }
}

void* hread_int_op(const pthandlers_op_t *op, pthandlers_resumption_t r, void *param) {
  int *integer = NULL;
  switch (op->tag) {
  case READ_INT:
    integer = (int*)malloc(sizeof(int));
    if (scanf("%d", integer) == 1)
      return pthandlers_resume(r, integer);
    else
      return pthandlers_abort(r, BAD_INPUT, NULL);
  default:
    return pthandlers_reperform(op);
  }
}

void* hread_bad_input_exn(const pthandlers_exn_t *op, void *param) {
  switch (op->tag) {
  case BAD_INPUT:
    fprintf(stderr, "error: bad input\n");
    break;
  default:
    pthandlers_rethrow(op);
  }
  return NULL;
}

void* hdiv_zero_op(const pthandlers_op_t *op, pthandlers_resumption_t r, void *param) {
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
  switch (exn->tag) {
  case DIV_ZERO:
    fprintf(stderr, "Division by zero!\n");
    break;
  default:
    pthandlers_rethrow(exn);
  }
  return (void*)0;
}

int safe_div(int n, int d) {
  if (d == 0) return (long)pthandlers_perform(DIV_ZERO, NULL);
  else return n / d;
}

void* interactive_div(void) {
  printf("Enter nominator> ");
  int n = read_int();
  printf("Enter denominator> ");
  int d = read_int();
  printf("%d / %d = %d\n", n, d, safe_div(n, d));
  return (void*)42;
}

void* handle_div(void) {
  pthandlers_t hdiv;
  pthandlers_init(&hdiv, NULL, hdiv_zero_op, hdiv_zero_exn);

  printf("Running under a non-aboertive handler.\n");
  long result = (long)pthandlers_handle(interactive_div, &hdiv, NULL);
  printf("Return value: %ld\n", result);

  _abort = 1;
  printf("Running under an abortive handler.\n");
  result = (long)pthandlers_handle(interactive_div, &hdiv, NULL);
  printf("Return value: %ld\n", result);
  return NULL;
}

int main(int argc, char *argv[]) {
  pthandlers_t hread;
  if (argc == 1) {
    pthandlers_init(&hread, NULL, (pthandlers_op_handler_t)hread_static_int_op, NULL);
    int inp_ptr = 0;
    pthandlers_handle(handle_div, &hread, &inp_ptr);
  } else if (argc == 2 && strcmp(argv[1], "-interact") == 0) {
    pthandlers_init(&hread, NULL, hread_int_op, hread_bad_input_exn);
    pthandlers_handle(handle_div, &hread, NULL);
  } else {
    printf("usage: %s [-interact]\n", argv[0]);
  }

  return 0;
}
