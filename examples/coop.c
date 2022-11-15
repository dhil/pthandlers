#include<stdio.h>
#include<stdlib.h>
#include "../lib/ptfx.h"

/* An implementation of cons lists. */
typedef struct list_t {
  void *value;
  struct list_t *next;
} list_t;

list_t* NIL = NULL;

list_t* list_cons(void *value, list_t *tail) {
  list_t *cell = (list_t*)malloc(sizeof(list_t));
  cell->value = value;
  cell->next = tail;
  return cell;
}

// In-place map.
list_t* list_imap(void *(*f)(void*), list_t *xs) {
  list_t* ptr = xs;
  while (ptr != NIL) {
    ptr->value = f(ptr->value);
    ptr = ptr->next;
  }
  return xs;
}

void* list_destroy(list_t *list, void *(*felem)(void*)) {
  if (felem != NULL) list = list_imap(felem, list);
  while (list != NIL) {
    list_t *cell = list;
    list = list->next;
    free(cell);
  }
  return NULL;
}

// In-place list reversal.
list_t* list_ireverse(list_t *list) {
  list_t *ptr = list;
  list = NIL;
  while (ptr != NIL) {
    list_t *tail = ptr->next;
    ptr->next = list;
    list = ptr;
    ptr = tail;
  }
  return list;
}

void* print_int(void* i) {
  printf("%ld", (long)i);
  return i;
}

/* FIFO Queue implementation. */
typedef struct queue_t {
  list_t *front;
  list_t *rear;
} queue_t;

queue_t* queue_create(void) {
  queue_t* q = (queue_t*)malloc(sizeof(queue_t));
  q->front = NIL;
  q->rear  = NIL;
  return q;
}

void* queue_destroy(queue_t *q, void *(*felem)(void*)) {
  list_destroy(q->front, felem);
  list_destroy(q->rear, felem);
  free(q);
  return NULL;
}

queue_t* queue_push(queue_t *q, ptfx_cont_t *k) {
  q->rear = list_cons((void*)k, q->rear);
  return q;
}

ptfx_cont_t* queue_pop(queue_t *q) {
  if (q->front == NIL) {
    q->front = list_ireverse(q->rear);
    q->rear  = NIL;
  }

  if (q->front == NIL) return NULL;
  void *elem = q->front->value;
  list_t *cell = q->front;
  q->front = q->front->next;
  cell->next = NIL;
  list_destroy(cell, NULL);
  return (ptfx_cont_t*)elem;
}

int queue_isempty(const queue_t *q) {
  return q->front == NIL && q->rear == NIL;
}

/* Roundrobin scheduler. */
static ptfx_handler_t hrr;

void* run_next(queue_t *q) {
  if (queue_isempty(q)) return NULL;
  else {
    ptfx_cont_t *cont = queue_pop(q);
    ptfx_cont_t k = *cont;
    free(cont);
    return ptfx_resume(k, NULL, &hrr, q);
  }
}

struct thunk_t {
  void *(*fn)(void *);
};

typedef struct thunk_t* thunk_t;

PTFX_DECLARE_EFFECT(co, fork, yield) {
  PTFX_OP_VOID_RESULT(fork, const thunk_t),
  PTFX_OP_VOID(yield)
};


void gfork(void *(*f)(void*)) {
  struct thunk_t g = { f };
  ptfx_perform(co, fork, &g);
}

void gyield(void) {
  ptfx_perform0(co, yield);
}

static ptfx_cont_t* heapify(ptfx_cont_t r) {
  ptfx_cont_t *cont = (ptfx_cont_t*)malloc(sizeof(ptfx_cont_t));
  *cont = r;
  return cont;
}

// Handler definition
void* hrr_return(void *result, queue_t *q) {
  return run_next(q);
}

void* hrr_fork(ptfx_op_t *op, ptfx_cont_t r, queue_t *q) {
  ptfx_cont_t cont = ptfx_new_cont(((thunk_t)op->payload)->fn);
  q = queue_push(q, heapify(r));
  return ptfx_resume(cont, NULL, &hrr, (void *)q);
}

void* hrr_yield(ptfx_op_t *op, ptfx_cont_t r, queue_t *q) {
  return run_next(queue_push(q, heapify(r)));
}

ptfx_hop hrr_selector(const struct ptfx_op_spec *spec) {
  if (spec == &ptfx_effect_co.fork) return (ptfx_hop)hrr_fork;
  else if (spec == &ptfx_effect_co.yield) return (ptfx_hop)hrr_yield;
  else return NULL;
}

void* a2(void *arg) {
  printf("A2 ");
  gyield();
  printf("A2 ");
  return NULL;
}

void* a1(void *arg) {
  printf("A1 ");
  gfork(a2);
  gyield();
  printf("A1 ");
  return NULL;
}

void* b1(void *arg) {
  printf("B1 ");
  gyield();
  printf("B1 ");
  return NULL;
}

void* runner(void *arg) {
  printf("main ");
  gfork(a1);
  gfork(b1);
  printf("main ");
  return NULL;
}

int main(void) {
  ptfx_handler_t h = { .ret = (ptfx_hret)hrr_return
                     , .eff = hrr_selector };
  hrr = h;
  ptfx_cont_t cont = ptfx_new_cont(runner);
  queue_t *q = queue_create();
  ptfx_resume(cont, NULL, &hrr, (void*)q);
  printf("\n");
  queue_destroy(q, NULL);
  return 0;
}
