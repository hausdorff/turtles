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
    };
} Prim;

typedef enum type {
    T_INT,
    T_SYM,
    T_PAIR,
    T_NATIVE
} Type;

char *pair_heap; // grows down
char *prim_heap; // grows up

Pair *syms[255];

// Symbols for primitives. Initialized in init().
void *quote_sym = NULL;

// Global environment.
Pair *global_env = LISP_NIL;

void *mksym(const char *);
void init()
{
    size_t s = 1024;
    char *heap = malloc(s);
    pair_heap = heap + s - sizeof(Pair);
    prim_heap = heap;

    for (int i = 0; i < sizeof(syms)/sizeof(Pair*); i++) {
        syms[i] = LISP_NIL;
    }

    quote_sym = mksym("QUOTE");
}

void checkoverflow()
{
    if (prim_heap >= pair_heap) {
        printf("*** Heap overflow. Quitting.\n");
        exit(1);
    }
}

void *mkpair(void *car, void *cdr)
{
    // XXX: CHECK OVERFLOW UP HERE INSTEAD
    Pair *p = (Pair *) pair_heap;
    p->car = car;
    p->cdr = cdr;
    pair_heap -= sizeof(*p);
    checkoverflow();
    return (void *)p;
}

void *mkint(int v)
{
    // XXX: CHECK OVERFLOW UP HERE INSTEAD
    Prim *p = (Prim *) prim_heap;
    p->type = T_INT;
    p->int_ = v;
    prim_heap += sizeof(*p);
    checkoverflow();
    return (void *)p;
}

void *mknative(void* (*fn)(void *))
{
    // XXX: CHECK OVERFLOW UP HERE INSTEAD
    Prim *p = (Prim *) prim_heap;
    p->type = T_NATIVE;
    p->fn = fn;
    prim_heap += sizeof(*p);
    checkoverflow();
    return (void *)p;
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

void *mksym(const char *sym)
{
    uint8_t hash = gethash(sym);

    Pair *pair = syms[hash];
    for (; !LISP_NILP(pair); pair = (Pair *)pair->cdr) {
        Prim *prim = (Prim *)pair->car;
        if (strcmp(prim->sym, sym) == 0) {
            return (void *) prim;
        }
    }
    // XXX: CHECK OVERFLOW UP HERE INSTEAD
    Prim *prim = (Prim *) prim_heap;
    prim->type = T_SYM;
    strcpy(prim->sym, sym);
    prim_heap += sizeof(*prim) + strlen(sym) + 1;
    checkoverflow();
    syms[hash] = mkpair((void *)prim, (void *)syms[hash]);
    return prim;
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

void lwrite(void *ptr)
{
    if (ptr == LISP_NIL) {
        printf("NIL");
        return;
    }

    switch (gettype(ptr)) {
    case T_INT: printf("%d", ((Prim *)ptr)->int_); break;
    case T_SYM: printf("%s", ((Prim *)ptr)->sym); break;
    case T_PAIR:
        {
            Pair *p = (Pair *) ptr;
            printf("(");
            for (; !LISP_NILP(p); p = (Pair *) p->cdr) {
                lwrite(p->car);
                if (!LISP_NILP(p->cdr)) {
                    printf(" ");
                }
            }
            printf(")");
        } break;
    }
}

void *eval(void *, Pair *);
Pair *mapeval(Pair *list, Pair *env)
{
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
        if (env->car == name)
            return env->cdr;
    }
    return NULL;
}

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
            } else {
                verb = eval(verb, env);
                args = mapeval(args, env);

                switch (gettype(verb)) {
                case T_NATIVE:
                    return ((Prim *)verb)->fn(args);
                default:
                    printf("*** Type is not callable.\n");
                    exit(1);
                }
            }
        } break;
    }
}

void defglobal(const char *name, void *value)
{
    global_env = bind(mksym(name), value, global_env);    
}

void defnative(const char *name, void* (*fn)(void *))
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

int main()
{
    init();
    defglobal("NIL", LISP_NIL);
    defnative("CONS", native_cons);
    defnative("CAR", native_car);
    defnative("CDR", native_cdr);
    while (!feof(stdin)) {
        printf("> ");
        void *ptr = eval(lread(), global_env);
        lwrite(ptr);
        printf("\n");
    }
}
