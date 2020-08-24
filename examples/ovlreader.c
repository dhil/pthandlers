// Overloaded reader example.
#include <stdio.h>
#include <stdlib.h>
#include "../lib/pthandlers.h"

// "Closure" data for the resumptive computations.
pthandlers_resumption_t captured_resumption = NULL;
void *captured_param = NULL;

void* run_resume(void) {
  pthandlers_resumption_t r = captured_resumption;
  captured_resumption = NULL;
  void *param = captured_param;
  captured_param = NULL;

  return pthandlers_resume(r, param);
}

// READ : int
enum { READ = 400, LIFTED_READ };
long read(void) {
  return (long)pthandlers_perform(READ, NULL);
}

// Simulates a "lifted" read.
long liftread(void) {
  return (long)pthandlers_perform(LIFTED_READ, NULL);
}

void* hinner_op(pthandlers_op_t op, pthandlers_resumption_t r, void *param) {
  switch (op->tag) {
  case READ:
    return pthandlers_resume(r, param);
  case LIFTED_READ:
    return pthandlers_resume(r, param);
  default:
    return pthandlers_reperform(op);
  }
}

void* houter_op(pthandlers_op_t op, pthandlers_resumption_t r, void *param) {
  pthandlers_t h;
  switch (op->tag) {
  case READ:
    pthandlers_init(&h, NULL, hinner_op, NULL);
    captured_resumption = r;
    captured_param = param;
    return pthandlers_handle(run_resume, &h, (void*)((long)param+2));
  default:
    return pthandlers_reperform(op);
  }
}

void* reader_comp(void) {
  printf("1st read: %ld\n", read());
  printf("2nd read: %ld\n", read());
  printf("3rd read: %ld\n", read());
  printf("lifted read: %ld\n", liftread());
  return NULL;
}

int main(void) {
   pthandlers_t h;
   pthandlers_init(&h, NULL, houter_op, NULL);
   pthandlers_handle(reader_comp, &h, (void*)40);

   return 0;
}
