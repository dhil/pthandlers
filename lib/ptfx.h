#ifndef PTFX_H
#define PTFX_H

struct ptfx_op_spec {
  unsigned int arg_size;
  unsigned int res_size;
  const char *name;
};

/* Internals */
void* _ptfx_perform(  const char *eff
                    , struct ptfx_op_spec *spec
                    , void *arg );

#define PTFX_OP(OP, S, T) .OP = {sizeof(S), sizeof(T), #OP}
#define PTFX_OP_VOID(OP) .OP = {0, 0, #OP}
#define PTFX_OP_VOID_PAYLOAD(OP, T) .OP = {0, sizeof(T), #OP}
#define PTFX_OP_VOID_RESULT(OP, S) .OP = {sizeof(S), 0, #OP}

#define PTFX_DECLARE_EFFECT(eff, ...) \
  struct ptfx_effect_##eff { \
    struct ptfx_op_spec __VA_ARGS__; \
  } ptfx_effect_##eff = \

typedef struct ptfx_cont {
  struct stack_repr *top;
  struct stack_repr *target;
} ptfx_cont_t;

typedef struct ptfx_op {
  struct ptfx_op_spec *spec;
  void *payload;
  ptfx_cont_t resume;
} ptfx_op_t;

void ptfx_free_op(ptfx_op_t *op);


typedef void* (*ptfx_hret)(void *result, void *param);
typedef void* (*ptfx_hop)(ptfx_op_t *op, ptfx_cont_t cont, void *param);
typedef ptfx_hop (*ptfx_heff)(const struct ptfx_op_spec *op);

typedef struct {
  ptfx_hret ret;
  ptfx_heff eff;
} ptfx_handler_t;


ptfx_cont_t ptfx_new_cont(void *(*f)(void *arg));
void* ptfx_resume(ptfx_cont_t resume, void *arg, ptfx_handler_t *h, void *param);
void* ptfx_resume_deep(ptfx_cont_t resume, void *arg, void *param);
void* ptfx_resume_shallow(ptfx_cont_t resume, void *arg, void *param);
#define ptfx_perform(EFF, OP, ARG) _ptfx_perform(#EFF, &ptfx_effect_##EFF . OP, ARG)
#define ptfx_perform0(EFF, OP) ptfx_perform(EFF, OP, NULL)

#endif
