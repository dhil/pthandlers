typedef struct stack_repr_t stack_repr_t;
typedef stack_repr_t resumption_t;
typedef struct op_t op_t;
typedef struct handler_t handler_t;

typedef void *(*computation_t)(void);
typedef void *(*op_handler_t)(const op_t*);
typedef void *(*ret_handler_t)(const void*);

extern const int RETURN;

// Install a handler.
void* handle(computation_t comp, handler_t *handler);
// Perform an operation.
void* perform(int op, const void *payload);
// Invoke a resumption.
void* resume(resumption_t *r, void *arg);
// Initialise the toplevel.
int init_handler_runtime(void);
// Finalise the toplevel.
int destroy_handler_runtime(void);

