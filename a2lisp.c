#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define LISP_NIL     ((Value*)1)
#define LISP_NILP(v) ((Value*)v == LISP_NIL)

#define CAR(v)   ((v)->pair.car)
#define CDR(v)   ((v)->pair.cdr)
#define CAAR(v)  ((v)->pair.car->pair.car)
#define CADR(v)  ((v)->pair.cdr->pair.car)
#define CDAR(v)  ((v)->pair.car->pair.cdr)
#define CDDR(v)  ((v)->pair.cdr->pair.cdr)
#define CAAAR(v) ((v)->pair.car->pair.car->pair.car)
#define CAADR(v) ((v)->pair.cdr->pair.car->pair.car)
#define CADAR(v) ((v)->pair.car->pair.cdr->pair.car)
#define CADDR(v) ((v)->pair.cdr->pair.cdr->pair.car)
#define CDAAR(v) ((v)->pair.car->pair.car->pair.cdr)
#define CDADR(v) ((v)->pair.cdr->pair.car->pair.cdr)
#define CDDAR(v) ((v)->pair.car->pair.cdr->pair.cdr)
#define CDDDR(v) ((v)->Pair.cdr->pair.cdr->pair.cdr)

typedef struct value {
    uint8_t type;
    union {
        int int_;
        char sym[1];
        struct value* (*fn)(struct value*);
        struct {
            struct value *args;
            struct value *body;
            struct value *env;
        } lambda;
        struct {
          struct value *car;
          struct value *cdr;
        } pair;
    };
} Value;

typedef enum type {
    T_INT,
    T_SYM,
    T_PAIR,
    T_NATIVE,
    T_LAMBDA
} Type;

char *heap; // grows up
char *heap_end;

#define SYMBOL_TABLE_SIZE 255
Value *syms[SYMBOL_TABLE_SIZE];

// Symbols for primitives. Initialized in init().
Value *quote_sym = NULL, *lambda_sym = NULL, *define_sym = NULL;

// Global environment.
Value *global_env;

Value *mksym(const char *);
Value *mkpair(Value *, Value *);
void init()
{
    int i;
    size_t s = 16384;

    heap = malloc(s);
    heap_end = heap + s;

    for (i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        syms[i] = LISP_NIL;
    }

    quote_sym = mksym("QUOTE");
    lambda_sym = mksym("LAMBDA");
    define_sym = mksym("DEFINE");

    // Set up the global environment as a single, "empty" binding.
    // This is done so that we can "splice" global definitions into
    // the global environment rather than "extending" the global
    // environment in the regular fashion. Otherwise, global mutual
    // recursion would not be possible.
    global_env = mkpair(mkpair(LISP_NIL, LISP_NIL), LISP_NIL);
}

void gc()
{
    printf("Running gc\n");
}

void maybe_gc(size_t nalloc)
{
    if (heap + nalloc >= heap_end) {
        gc();
    }
}

Value *mkpair(Value *car, Value *cdr)
{
    Value *p;
    const size_t nalloc = sizeof(Value);

    maybe_gc(nalloc);
    p = (Value *) heap;
    p->type = T_PAIR;
    p->pair.car = car;
    p->pair.cdr = cdr;
    heap += nalloc;
    return p;
}

Value *mkint(int v)
{
    Value *p;
    const size_t nalloc = sizeof(Value);

    maybe_gc(nalloc);
    p = (Value *) heap;
    p->type = T_INT;
    p->int_ = v;
    heap += nalloc;
    return p;
}

Value *mknative(Value* (*fn)(Value *))
{
    Value *p;
    const size_t nalloc = sizeof(Value);

    maybe_gc(nalloc);
    p = (Value *) heap;
    p->type = T_NATIVE;
    p->fn = fn;
    heap += nalloc;
    return p;
}

Value *mklambda(Value *args, Value *body, Value *env)
{
    Value *p;
    const size_t nalloc = sizeof(Value);

    maybe_gc(nalloc);
    p = (Value *) heap;
    p->type = T_LAMBDA;
    p->lambda.args = args;
    p->lambda.body = body;
    p->lambda.env = env;
    heap += nalloc;
    return p;
}

uint8_t gethash(const char *);
Value *mksym(const char *sym)
{
    uint8_t hash = gethash(sym);
    const size_t length = strlen(sym);
    const size_t nalloc = sizeof(Value) + length + 1;
    Value *pair, *prim;

    pair = syms[hash];
    for (; !LISP_NILP(pair); pair = CDR(pair)) {
        Value *prim = CAR(pair);
        if (strcasecmp(prim->sym, sym) == 0) {
            return prim;
        }
    }

    maybe_gc(nalloc);
    prim = (Value *) heap;
    prim->type = T_SYM;
    strcpy(prim->sym, sym);
    heap += nalloc;
    syms[hash] = mkpair(prim, syms[hash]);
    return prim;
}

uint8_t gethash(const char *sym)
{
    uint8_t hash = 0;
    const size_t length = strlen(sym);
    size_t i;

    for (i = 0; i < length; i++) {
        hash ^= tolower(sym[i]);
    }
    // XXX: Alex says this blows. I think he's optimizing prematurely.
    return hash;
}

Type gettype(Value *ptr)
{
    return ptr->type;
}

char peekchar()
{
    char ch = getchar();
    ungetc(ch, stdin);
    return ch;
}

Value *lreadsym()
{
    char buf[32];
    char *p = buf;
    char ch;
    while (isalpha((ch = getchar()))) {
        *p++ = ch;
    }
    ungetc(ch, stdin);
    *p = '\0';
    return mksym(buf);
}

Value *lreadint()
{
    int v = 0;
    char ch;
    while (isdigit((ch = getchar()))) {
        v = v*10 + (ch - '0');
    }
    ungetc(ch, stdin);
    return mkint(v);
}

Value *lread();
Value *lreadlist()
{
    Value *car, *cdr;

    if (peekchar() == ')') {
        getchar(); // eat )
        return LISP_NIL;
    }
    car = lread();
    cdr = lreadlist();
    return mkpair(car, cdr);
}

Value *lread()
{
    char ch;
 again:
    ch = getchar();
    if (isspace(ch)) goto again;

    ungetc(ch, stdin);
    if (isalpha(ch)) return lreadsym();
    else if (isdigit(ch)) return lreadint();
    else if (ch == '(') { getchar(); return lreadlist(); }
    else if (ch == '\'')  {
        getchar();
        return mkpair(quote_sym, mkpair(lread(), LISP_NIL));
    } else {
        printf("*** Unrecognized token '%c'.\n", ch);
        exit(1);
    }
}

void lwriteint(Value *ptr)
{
    printf("%d", ptr->int_);
}

void lwritesym(Value *ptr)
{
    printf("%s", ptr->sym);
}

void lwritenative(Value *ptr)
{
    printf("#<NATIVE>");
}

void lwritelambda(Value *ptr)
{
    printf("#<LAMBDA>");
}

void lwrite(Value *);
void lwritepair(Value *pair)
{
    printf("(");
    for (; !LISP_NILP(pair); pair = CDR(pair)) {
        lwrite(CAR(pair));
        if (!LISP_NILP(CDR(pair))) {
            if (gettype(CDR(pair)) == T_PAIR) {
                printf(" ");
            } else {
                // Handle improper lists
                printf(" . ");
                lwrite(CDR(pair));
                break;
            }
        }
    }
    printf(")");
}

void lwrite(Value *ptr)
{
    if (ptr == LISP_NIL) {
        printf("NIL");
        return;
    }

    switch (gettype(ptr)) {
    case T_INT: lwriteint(ptr); break;
    case T_SYM: lwritesym(ptr); break;
    case T_NATIVE: lwritenative(ptr); break;
    case T_LAMBDA: lwritelambda(ptr); break;
    case T_PAIR: lwritepair(ptr); break;
    }
}

Value *eval(Value *, Value *);
Value *mapeval(Value *list, Value *env)
{
    if (list == LISP_NIL)
        return LISP_NIL;
    return mkpair(eval(CAR(list), env), mapeval(CDR(list), env));
}

Value *bind(Value *name, Value *value, Value *env)
{
    Value *binding = mkpair(name, value);
    return mkpair(binding, env);
}

Value *lookup(Value *name, Value *env)
{
    assert(gettype(name) == T_SYM);
    for (; !LISP_NILP(env); env = CDR(env)) {
        // Pointer comparison is OK for interned symbols.
        Value *binding = CAR(env);
        if (CAR(binding) == name)
            return CDR(binding);
    }
    return NULL;
}

Value *apply(Value *proc, Value *args)
{
    switch (gettype(proc)) {
    case T_NATIVE:
        return proc->fn(args);
    case T_LAMBDA:
        {
            Value *call_env = proc->lambda.env;
            Value *formal = proc->lambda.args;
            Value *actual = args;
            while (!LISP_NILP(formal) && !LISP_NILP(actual)) {
                call_env = bind(CAR(formal), CAR(actual), call_env);
                formal = CDR(formal);
                actual = CDR(actual);
            }

            // Argument count mismatch?
            if (formal != actual) {
                printf("*** Argument count mismatch.\n");
                exit(1);
            }

            return eval(proc->lambda.body, call_env);
        } break;
    default:
        printf("*** Type is not callable.\n");
        exit(1);
    }
}

void defglobal(Value *, Value *);
Value *eval(Value *form, Value *env)
{
    switch (gettype(form)) {
    case T_INT: return form;
    case T_SYM:
        {
            Value *value = lookup(form, env);
            if (value == NULL) {
                printf("*** %s is undefined.\n", ((Value *)form)->sym);
                exit(1);
            }
            return value;
        } break;
    case T_PAIR:
        {
            Value *verb = CAR(form);
            Value *args = CDR(form);

            if (verb == quote_sym) {
                return CAR(args);
            } else if (verb == lambda_sym) {
                Value *lambda_args = CADR(form);
                Value *lambda_body = CADDR(form);
                return mklambda(lambda_args, lambda_body, env);
            } else if (verb == define_sym) {
                Value *name = CAR(args);
                Value *value = eval(CADR(args), env);
                defglobal(name, value);
                return name;
            } else {
                return apply(eval(verb, env), mapeval(args, env));
            }
        } break;
    }
}

void defglobal(Value *name, Value *value)
{
    global_env->pair.cdr = bind(name, value, global_env->pair.cdr);    
}

void defnative(Value *name, Value* (*fn)(Value *))
{
    defglobal(name, mknative(fn));
}

Value *native_cons(Value *args)
{
    Value *car = CAR(args);
    Value *cdr = CADR(args);
    return mkpair(car, cdr);
}

Value *native_car(Value *args)
{
    return CAAR(args);
}

Value *native_cdr(Value *args)
{
    return CDAR(args);
}

Value *native_eval(Value *args)
{
    return eval(CAR(args), global_env);
}

int main()
{
    Value *result;

    init();
    defglobal(mksym("NIL"), LISP_NIL);
    defnative(mksym("CONS"), native_cons);
    defnative(mksym("CAR"), native_car);
    defnative(mksym("CDR"), native_cdr);
    defnative(mksym("EVAL"), native_eval);

    while (!feof(stdin)) {
        printf("> ");
        result = eval(lread(), global_env);
        printf("\n");
        lwrite(result);
        printf("\n");
    }

    return 0;
}
