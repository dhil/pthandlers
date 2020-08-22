#include<stdio.h>
#include<stdlib.h>
#include "../lib/pthandlers.h"

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

queue_t* queue_push(queue_t *q, void *elem) {
  q->rear = list_cons(elem, q->rear);
  return q;
}

void* queue_pop(queue_t *q) {
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
  return elem;
}

int queue_isempty(const queue_t *q) {
  return q->front == NIL && q->rear == NIL;
}

/* Roundrobin scheduler. */
pthandlers_t hrr;

void* run_next(queue_t *q) {
  if (queue_isempty(q)) return NULL;
  else return pthandlers_resume_with(queue_pop(q), NULL, q);
}

enum { FORK = 300, YIELD };

void gfork(void *(*f)(void)) {
  pthandlers_perform(FORK, (void*)f);
}

void gyield(void) {
  pthandlers_perform(YIELD, NULL);
}

void* rr_ret(void *result, queue_t *q) {
  return run_next(q);
}

void* rr_ops(const pthandlers_op_t *op, pthandlers_resumption_t r, queue_t *q) {
  switch (op->tag) {
  case FORK:
    return pthandlers_handle((pthandlers_thunk_t)op->value, &hrr, queue_push(q, (void*)r));
  case YIELD:
    return run_next(queue_push(q, r));
  default:
    return pthandlers_reperform(op);
  }
}

void* a2(void) {
  printf("A2 ");
  gyield();
  printf("A2 ");
  return NULL;
}

void* a1(void) {
  printf("A1 ");
  gfork(a2);
  gyield();
  printf("A1 ");
  return NULL;
}

void* b1(void) {
  printf("B1 ");
  gyield();
  printf("B1 ");
  return NULL;
}

void* coop_comp(void) {
  printf("main ");
  gfork(a1);
  gfork(b1);
  printf("main ");
  return NULL;
}

int main(void) {
  pthandlers_init(&hrr, (pthandlers_ret_handler_t)rr_ret, (pthandlers_op_handler_t)rr_ops, NULL);
  queue_t *q = queue_create();
  pthandlers_handle(coop_comp, &hrr, q);
  printf("\n");
  queue_destroy(q, NULL);
  return 0;
}
