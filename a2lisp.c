#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define LISP_NIL     ((void*)1)
#define LISP_NILP(v) ((void*)v == LISP_NIL)

void *lstack[255];
unsigned lsp = 0;

void lpush(void *v)
{
    lstack[lsp++] = v;
}

void *lpop()
{
    return lstack[--lsp];
}

void lpopn(int n)
{
    for (; n > 0; n--)
        lpop();
}

typedef struct pair {
    void *car;
    void *cdr;
} Pair;

typedef struct prim {
    uint8_t type;
    union {
        int int_;
        char sym[0];
        void* (*fn)(void*);
        struct {
            void *args;
            void *body;
            void *env;
        } lambda;
    };
} Prim;

typedef enum type {
    T_INT,
    T_SYM,
    T_PAIR,
    T_NATIVE,
    T_LAMBDA
} Type;

char *pair_heap; // grows down
char *prim_heap; // grows up

Pair *syms[255];

// Symbols for primitives. Initialized in init().
void *quote_sym = NULL, *lambda_sym = NULL, *define_sym = NULL;

// Global environment.
Pair *global_env;

void *mksym(const char *);
void *mkpair(void *, void *);
void init()
{
    size_t s = 16384;
    char *heap = malloc(s);
    pair_heap = heap + s - sizeof(Pair);
    prim_heap = heap;

    for (int i = 0; i < sizeof(syms)/sizeof(Pair*); i++) {
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
    if (prim_heap + nalloc >= pair_heap) {
        gc();
    }
}

void *mkpair(void *car, void *cdr)
{
    const size_t nalloc = sizeof(Prim);
    lpush(car); lpush(cdr);
    maybe_gc(nalloc);
    lpopn(2);
    Pair *p = (Pair *) pair_heap;
    p->car = car;
    p->cdr = cdr;
    pair_heap -= nalloc;
    return (void *)p;
}

void *mkint(int v)
{
    const size_t nalloc = sizeof(Prim);
    maybe_gc(nalloc);
    Prim *p = (Prim *) prim_heap;
    p->type = T_INT;
    p->int_ = v;
    prim_heap += nalloc;
    return (void *)p;
}

void *mknative(void* (*fn)(void *))
{
    const size_t nalloc = sizeof(Prim);
    maybe_gc(nalloc);
    Prim *p = (Prim *) prim_heap;
    p->type = T_NATIVE;
    p->fn = fn;
    prim_heap += nalloc;
    return (void *)p;
}

void *mklambda(void *args, void *body, void *env)
{
    const size_t nalloc = sizeof(Prim);
    maybe_gc(nalloc);
    Prim *p = (Prim *) prim_heap;
    p->type = T_LAMBDA;
    p->lambda.args = args;
    p->lambda.body = body;
    p->lambda.env = env;
    prim_heap += nalloc;
    return (void *)p;
}

uint8_t gethash(const char *);
void *mksym(const char *sym)
{
    uint8_t hash = gethash(sym);
    size_t length = strlen(sym);

    Pair *pair = syms[hash];
    for (; !LISP_NILP(pair); pair = (Pair *)pair->cdr) {
        Prim *prim = (Prim *)pair->car;
        if (strcmp(prim->sym, sym) == 0) {
            return (void *) prim;
        }
    }

    const size_t nalloc = sizeof(Prim) + length + 1;
    maybe_gc(nalloc);
    Prim *prim = (Prim *) prim_heap;
    prim->type = T_SYM;
    strcpy(prim->sym, sym);
    prim_heap += nalloc;
    syms[hash] = mkpair((void *)prim, (void *)syms[hash]);
    return prim;
}

uint8_t gethash(const char *sym)
{
    uint8_t hash = 0;
    size_t len = strlen(sym);
    for (size_t i = 0; i < len; i++) {
        hash ^= sym[i];
    }
    // XXX: Alex says this blows. I think he's optimizing prematurely.
    return hash;
}

Type gettype(void *ptr)
{
    if ((char *) ptr >= pair_heap) {
        return T_PAIR;
    } else {
        Prim *p = (Prim *) ptr;
        return p->type;
    }
}

char peekchar()
{
    char ch = getchar();
    ungetc(ch, stdin);
}

void *lreadsym()
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

void *lreadint()
{
    int v = 0;
    char ch;
    while (isdigit((ch = getchar()))) {
        v = v*10 + (ch - '0');
    }
    ungetc(ch, stdin);
    return mkint(v);
}

void *lread();
void *lreadlist()
{
    if (peekchar() == ')') {
        getchar(); // eat )
        return LISP_NIL;
    }
    void *car = lread();
    void *cdr = lreadlist();
    return mkpair(car, cdr);
}

void *lread()
{
    char ch;
 again:
    ch = getchar();
    if (isspace(ch)) goto again;

    ungetc(ch, stdin);
    if (isalpha(ch)) return lreadsym();
    if (isdigit(ch)) return lreadint();
    if (ch == '(') { getchar(); return lreadlist(); }
    else {
        printf("*** Unrecognized token '%c'.\n", ch);
        exit(1);
    }
}

void lwriteint(void *ptr)
{
    printf("%d", ((Prim *)ptr)->int_);
}

void lwritesym(void *ptr)
{
    printf("%s", ((Prim *)ptr)->sym);
}

void lwritenative(void *ptr)
{
    printf("#<NATIVE>");
}

void lwritelambda(void *ptr)
{
    printf("#<LAMBDA>");
}

void lwrite(void *);
void lwritepair(void *ptr)
{
    Pair *pair = (Pair *) ptr;
    printf("(");
    for (; !LISP_NILP(pair); pair = (Pair *) pair->cdr) {
        lwrite(pair->car);
        if (!LISP_NILP(pair->cdr)) {
            if (gettype(pair->cdr) == T_PAIR) {
                printf(" ");
            } else {
                // Handle improper lists
                printf(" . ");
                lwrite(pair->cdr);
                break;
            }
        }
    }
    printf(")");
}

void lwrite(void *ptr)
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

void *eval(void *, Pair *);
Pair *mapeval(Pair *list, Pair *env)
{
    if (list == LISP_NIL)
        return LISP_NIL;
    return mkpair(eval(list->car, env), mapeval((Pair *)list->cdr, env));
}

Pair *bind(Prim *name, void *value, Pair *env)
{
    Pair *binding = (Pair *)mkpair(name, value);
    return (Pair *)mkpair(binding, env);
}

void *lookup(Prim *name, Pair *env)
{
    assert(gettype(name) == T_SYM);
    for (; !LISP_NILP(env); env = (Pair *)env->cdr) {
        // Pointer comparison is OK for interned symbols.
        Pair *binding = (Pair *)env->car;
        if (binding->car == name)
            return binding->cdr;
    }
    return NULL;
}

void *apply(void *proc, void *args)
{
    switch (gettype(proc)) {
    case T_NATIVE:
        return ((Prim *)proc)->fn(args);
    case T_LAMBDA:
        {
            Prim *lambda = (Prim *)proc;
            Pair *call_env = lambda->lambda.env;
            Pair *formal = lambda->lambda.args;
            Pair *actual = args;
            while (!LISP_NILP(formal) && !LISP_NILP(actual)) {
                call_env = bind(formal->car, actual->car, call_env);
                formal = (Pair *) formal->cdr;
                actual = (Pair *) actual->cdr;
            }

            // Argument count mismatch?
            if (formal != actual) {
                printf("*** Argument count mismatch.\n");
                exit(1);
            }

            return eval(lambda->lambda.body, call_env);
        } break;
    default:
        printf("*** Type is not callable.\n");
        exit(1);
    }
}

void defglobal(Prim *, void *);
void *eval(void *form, Pair *env)
{
    switch (gettype(form)) {
    case T_INT: return form;
    case T_SYM:
        {
            void *value = lookup(form, env);
            if (value == NULL) {
                printf("*** %s is undefined.\n", ((Prim *)form)->sym);
                exit(1);
            }
            return value;
        } break;
    case T_PAIR:
        {
            Pair *p = (Pair *) form;
            void *verb = p->car;
            Pair *args = (Pair *)p->cdr;

            if (verb == quote_sym) {
                return args->car;
            } else if (verb == lambda_sym) {
                void *args = ((Pair *)p->cdr)->car;
                void *body = ((Pair *)((Pair *)p->cdr)->cdr)->car;
                return mklambda(args, body, env);
            } else if (verb == define_sym) {
                void *name = ((Pair *)args->car);
                void *value = eval(((Pair *)args->cdr)->car, env);
                defglobal(name, value);
                return name;
            } else {
                return apply(eval(verb, env), mapeval(args, env));
            }
        } break;
    }
}

void defglobal(Prim *name, void *value)
{
    global_env->cdr = bind(name, value, global_env->cdr);    
}

void defnative(Prim *name, void* (*fn)(void *))
{
    defglobal(name, mknative(fn));
}

void *native_cons(void *args)
{
    void *car = ((Pair *)args)->car;
    void *cdr = ((Pair *)((Pair *)args)->cdr)->car;
    return mkpair(car, cdr);
}

void *native_car(void *args)
{
    return ((Pair *)((Pair *)args)->car)->car;
}

void *native_cdr(void *args)
{
    return ((Pair *)((Pair *)args)->car)->cdr;
}

void *native_eval(void *args)
{
    return eval(((Pair *)args)->car, global_env);
}

int main()
{
    init();
    defglobal(mksym("NIL"), LISP_NIL);
    defnative(mksym("CONS"), native_cons);
    defnative(mksym("CAR"), native_car);
    defnative(mksym("CDR"), native_cdr);
    defnative(mksym("EVAL"), native_eval);
    while (!feof(stdin)) {
        printf("> ");
        void *ptr = eval(lread(), global_env);
        lwrite(ptr);
        printf("\n");
    }
}
