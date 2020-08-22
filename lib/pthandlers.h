#ifndef PTHANDLERS_H
#define PTHANDLERS_H
typedef struct pthandlers_stack_repr_t* pthandlers_resumption_t;
typedef struct pthandlers_op_t {
  int tag;
  void *value;
} pthandlers_op_t;
typedef pthandlers_op_t pthandlers_exn_t;

extern const int PTHANDLERS_EUNHANDLED;

typedef void *(*pthandlers_thunk_t)(void);
typedef void *(*pthandlers_op_handler_t)(const pthandlers_op_t*, pthandlers_resumption_t, void*);
typedef void *(*pthandlers_exn_handler_t)(const pthandlers_op_t*, void*);
typedef void *(*pthandlers_ret_handler_t)(void*, void*);

typedef struct pthandlers_t {
  pthandlers_ret_handler_t ret;
  pthandlers_op_handler_t op;
  pthandlers_exn_handler_t exn;
  void *param;
} pthandlers_t;

// Initialise a handler structure.
void pthandlers_init( pthandlers_t *handler
                    , pthandlers_ret_handler_t ret_handler
                    , pthandlers_op_handler_t op_handler
                    , pthandlers_exn_handler_t exn_handler );

// Install a handler.
void* pthandlers_handle( pthandlers_thunk_t comp
                       , pthandlers_t *handler, void *handler_param  );

// Perform an operation.
void* pthandlers_perform(int tag, void *payload);
void  pthandlers_throw(int tag, void *payload);
// Invoke a resumption.
void* pthandlers_resume(pthandlers_resumption_t r, void *arg);
void* pthandlers_resume_with(pthandlers_resumption_t r, void *arg, void *handler_param);
// Abort a resumption.
void* pthandlers_abort(pthandlers_resumption_t r, int tag, void *payload);
// Forward an operation.
void* pthandlers_reperform(const pthandlers_op_t *op);
void  pthandlers_rethrow(const pthandlers_exn_t *exn);
#endif
