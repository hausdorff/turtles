#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define LISP_NIL     ((Prim*)1)
#define LISP_NILP(v) ((Prim*)v == LISP_NIL)


typedef struct prim {
    uint8_t type;
    union {
        int int_;
        char sym[0];
        struct prim* (*fn)(struct prim*);
        struct {
            struct prim *args;
            struct prim *body;
            struct prim *env;
        } lambda;
        struct {
          struct prim *car;
          struct prim *cdr;
        } pair;
    };
} Prim;

typedef enum type {
    T_INT,
    T_SYM,
    T_PAIR,
    T_NATIVE,
    T_LAMBDA
} Type;

char *heap; // grows up
char *heap_end;

Prim *syms[255];

// Symbols for primitives. Initialized in init().
Prim *quote_sym = NULL, *lambda_sym = NULL, *define_sym = NULL;

// Global environment.
Prim *global_env;

Prim *mksym(const char *);
Prim *mkpair(Prim *, Prim *);
void init()
{
    size_t s = 16384;
    heap = malloc(s);
    heap_end = heap + s;

    for (int i = 0; i < sizeof(syms)/sizeof(Prim*); i++) {
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

Prim *mkpair(Prim *car, Prim *cdr)
{
    const size_t nalloc = sizeof(Prim);
    maybe_gc(nalloc);
    Prim *p = (Prim *) heap;
    p->type = T_PAIR;
    p->pair.car = car;
    p->pair.cdr = cdr;
    heap += nalloc;
    return p;
}

Prim *mkint(int v)
{
    const size_t nalloc = sizeof(Prim);
    maybe_gc(nalloc);
    Prim *p = (Prim *) heap;
    p->type = T_INT;
    p->int_ = v;
    heap += nalloc;
    return p;
}

Prim *mknative(Prim* (*fn)(Prim *))
{
    const size_t nalloc = sizeof(Prim);
    maybe_gc(nalloc);
    Prim *p = (Prim *) heap;
    p->type = T_NATIVE;
    p->fn = fn;
    heap += nalloc;
    return p;
}

Prim *mklambda(Prim *args, Prim *body, Prim *env)
{
    const size_t nalloc = sizeof(Prim);
    maybe_gc(nalloc);
    Prim *p = (Prim *) heap;
    p->type = T_LAMBDA;
    p->lambda.args = args;
    p->lambda.body = body;
    p->lambda.env = env;
    heap += nalloc;
    return p;
}

uint8_t gethash(const char *);
Prim *mksym(const char *sym)
{
    uint8_t hash = gethash(sym);
    size_t length = strlen(sym);

    Prim *pair = syms[hash];
    for (; !LISP_NILP(pair); pair = pair->pair.cdr) {
        Prim *prim = pair->pair.car;
        if (strcmp(prim->sym, sym) == 0) {
            return prim;
        }
    }

    const size_t nalloc = sizeof(Prim) + length + 1;
    maybe_gc(nalloc);
    Prim *prim = (Prim *) heap;
    prim->type = T_SYM;
    strcpy(prim->sym, sym);
    heap += nalloc;
    syms[hash] = mkpair(prim, syms[hash]);
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

Type gettype(Prim *ptr)
{
    return ptr->type;
}

char peekchar()
{
    char ch = getchar();
    ungetc(ch, stdin);
    return ch;
}

Prim *lreadsym()
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

Prim *lreadint()
{
    int v = 0;
    char ch;
    while (isdigit((ch = getchar()))) {
        v = v*10 + (ch - '0');
    }
    ungetc(ch, stdin);
    return mkint(v);
}

Prim *lread();
Prim *lreadlist()
{
    if (peekchar() == ')') {
        getchar(); // eat )
        return LISP_NIL;
    }
    Prim *car = lread();
    Prim *cdr = lreadlist();
    return mkpair(car, cdr);
}

Prim *lread()
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

void lwriteint(Prim *ptr)
{
    printf("%d", ptr->int_);
}

void lwritesym(Prim *ptr)
{
    printf("%s", ptr->sym);
}

void lwritenative(Prim *ptr)
{
    printf("#<NATIVE>");
}

void lwritelambda(Prim *ptr)
{
    printf("#<LAMBDA>");
}

void lwrite(Prim *);
void lwritepair(Prim *pair)
{
    printf("(");
    for (; !LISP_NILP(pair); pair = pair->pair.cdr) {
        lwrite(pair->pair.car);
        if (!LISP_NILP(pair->pair.cdr)) {
            if (gettype(pair->pair.cdr) == T_PAIR) {
                printf(" ");
            } else {
                // Handle improper lists
                printf(" . ");
                lwrite(pair->pair.cdr);
                break;
            }
        }
    }
    printf(")");
}

void lwrite(Prim *ptr)
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

Prim *eval(Prim *, Prim *);
Prim *mapeval(Prim *list, Prim *env)
{
    if (list == LISP_NIL)
        return LISP_NIL;
    return mkpair(eval(list->pair.car, env), mapeval(list->pair.cdr, env));
}

Prim *bind(Prim *name, Prim *value, Prim *env)
{
    Prim *binding = mkpair(name, value);
    return mkpair(binding, env);
}

Prim *lookup(Prim *name, Prim *env)
{
    assert(gettype(name) == T_SYM);
    for (; !LISP_NILP(env); env = env->pair.cdr) {
        // Pointer comparison is OK for interned symbols.
        Prim *binding = env->pair.car;
        if (binding->pair.car == name)
            return binding->pair.cdr;
    }
    return NULL;
}

Prim *apply(Prim *proc, Prim *args)
{
    switch (gettype(proc)) {
    case T_NATIVE:
        return ((Prim *)proc)->fn(args);
    case T_LAMBDA:
        {
            Prim *call_env = proc->lambda.env;
            Prim *formal = proc->lambda.args;
            Prim *actual = args;
            while (!LISP_NILP(formal) && !LISP_NILP(actual)) {
                call_env = bind(formal->pair.car, actual->pair.car, call_env);
                formal = formal->pair.cdr;
                actual = actual->pair.cdr;
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

void defglobal(Prim *, Prim *);
Prim *eval(Prim *form, Prim *env)
{
    switch (gettype(form)) {
    case T_INT: return form;
    case T_SYM:
        {
            Prim *value = lookup(form, env);
            if (value == NULL) {
                printf("*** %s is undefined.\n", ((Prim *)form)->sym);
                exit(1);
            }
            return value;
        } break;
    case T_PAIR:
        {
            Prim *verb = form->pair.car;
            Prim *args = form->pair.cdr;

            if (verb == quote_sym) {
                return args->pair.car;
            } else if (verb == lambda_sym) {
                Prim *args = form->pair.cdr->pair.car;
                Prim *body = form->pair.cdr->pair.cdr->pair.car;
                return mklambda(args, body, env);
            } else if (verb == define_sym) {
                Prim *name = args->pair.car;
                Prim *value = eval(args->pair.cdr->pair.car, env);
                defglobal(name, value);
                return name;
            } else {
                return apply(eval(verb, env), mapeval(args, env));
            }
        } break;
    }
}

void defglobal(Prim *name, Prim *value)
{
    global_env->pair.cdr = bind(name, value, global_env->pair.cdr);    
}

void defnative(Prim *name, Prim* (*fn)(Prim *))
{
    defglobal(name, mknative(fn));
}

Prim *native_cons(Prim *args)
{
    Prim *car = args->pair.car;
    Prim *cdr = args->pair.cdr->pair.car;
    return mkpair(car, cdr);
}

Prim *native_car(Prim *args)
{
    return args->pair.car->pair.car;
}

Prim *native_cdr(Prim *args)
{
    return args->pair.car->pair.cdr;
}

Prim *native_eval(Prim *args)
{
    return eval(args->pair.car, global_env);
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
        Prim *ptr = eval(lread(), global_env);
        lwrite(ptr);
        printf("\n");
    }
}
