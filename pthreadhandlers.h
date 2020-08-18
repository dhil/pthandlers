typedef struct stack_repr_t stack_repr_t;
typedef stack_repr_t resumption_t;
typedef struct op_t op_t;
typedef struct handler_t* handler_t;

typedef void *(*computation_t)(void);
typedef void *(*op_handler_t)(const op_t*, void*);
typedef void *(*ret_handler_t)(void*, void*);
/* typedef int (*handle_predicate_t)(const op_t*); */

typedef struct {
  char *message;
} exn_t;

// Initialise a handler.
int init_handler(handler_t *handler, ret_handler_t ret_handler, op_handler_t op_handler, void *param);
// Finalise a handler.
int destroy_handler(handler_t *handler);

// Install a handler.
void* handle(computation_t comp, handler_t handler);
// Perform an operation.
void* perform(int op, void *payload);
// Invoke a resumption.
void* resume(resumption_t *r, void *arg);
void* resume_with(resumption_t *r, void *arg, void *handler_param);
// Abort a resumption.
void* abort_(resumption_t *r, exn_t *e);
// Forward an operation.
void* forward(const op_t *op);

// Initialise the runtime.
int init_handler_runtime(void);
// Finalise the runtime.
int destroy_handler_runtime(void);

