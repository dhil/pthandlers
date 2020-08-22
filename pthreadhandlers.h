typedef struct pthandler_stack_repr_t* pthandler_resumption_t;
typedef struct pthandler_op_t {
  int tag;
  void *value;
} pthandler_op_t;
typedef pthandler_op_t pthandler_exn_t;
typedef struct pthandler_t pthandler_t;

extern const int PTHANDLER_EUNHANDLED;

typedef void *(*pthandler_thunk_t)(void);
typedef void *(*pthandler_op_handler_t)(const pthandler_op_t*, pthandler_resumption_t, void*);
typedef void *(*pthandler_ret_handler_t)(void*, void*);

typedef struct {
  int tag;
  pthandler_op_handler_t fn;
} pthandler_op_clause_t;

// Initialise a handler structure.
void pthandler_init(  pthandler_t *handler
                    , pthandler_ret_handler_t ret_handler
                    , pthandler_op_handler_t op_handler );

// Install a handler.
void* pthandler_handle(  pthandler_thunk_t comp
                       , pthandler_t *handler, void *handler_param  );

// Perform an operation.
void* pthandler_perform(int tag, void *payload);
void pthandler_throw(int tag, void *payload);
// Invoke a resumption.
void* pthandler_resume(pthandler_resumption_t r, void *arg);
void* pthandler_resume_with(pthandler_resumption_t r, void *arg, void *handler_param);
// Abort a resumption.
void* pthandler_abort(pthandler_resumption_t r, int tag, void *payload);
// Forward an operation.
/* void* pthandler_forward(pthandler_exn_t op); */
void* pthandler_reperform(const pthandler_op_t *op);

