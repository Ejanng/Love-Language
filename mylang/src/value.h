#pragma once
// Runtime values shared by both backends: nil, number, boolean, or a pointer to a heap object
// (string, function, closure, ...).
//
// Heap objects are never freed while the program runs: each one is linked into a global list and
// everything is released together by objects_free_all() at exit. This stands in for the reference
// counting the language used before, which leaked on cycles anyway. A real garbage collector
// (mark-and-sweep, rooted in the stack, globals and open upvalues) would walk this same list.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct Stmt;
struct Proto;

typedef struct Obj Obj;

typedef enum { VAL_NIL, VAL_NUMBER, VAL_BOOL, VAL_OBJ } ValueType;

typedef struct {
    ValueType type;
    union {
        double number;
        bool boolean;
        Obj* obj;
    } as;
} Value;

static inline Value nil_val(void) { return (Value){VAL_NIL, {.number = 0}}; }
static inline Value number_val(double n) { return (Value){VAL_NUMBER, {.number = n}}; }
static inline Value bool_val(bool b) { return (Value){VAL_BOOL, {.boolean = b}}; }
static inline Value obj_val(void* o) { return (Value){VAL_OBJ, {.obj = (Obj*)o}}; }

typedef enum {
    OBJ_STRING,
    OBJ_ENV,       // tree-walker: a scope
    OBJ_FUNCTION,  // tree-walker: a user-defined function plus the scope it was defined in
    OBJ_CLOSURE,   // VM: compiled code plus the variables it captured
    OBJ_UPVALUE,   // VM: a variable captured by a closure
} ObjType;

struct Obj {
    ObjType type;
    Obj* next;  // the global list of every object
};

static inline bool is_obj_type(Value v, ObjType t) { return v.type == VAL_OBJ && v.as.obj->type == t; }

// ---- strings ----

typedef struct {
    Obj obj;
    size_t length;
    char chars[];  // always '\0'-terminated as well
} ObjString;

ObjString* string_copy(const char* chars, size_t length);
ObjString* string_concat(const ObjString* a, const ObjString* b);
#define AS_STRING(v) ((ObjString*)(v).as.obj)

// ---- hash table (string keys) ----
// Keys are borrowed '\0'-terminated strings that must outlive the table (AST names, string constants).

typedef struct {
    const char* key;  // NULL marks an empty slot
    uint32_t hash;
    Value value;
} TableEntry;

typedef struct {
    TableEntry* entries;
    int count;
    int cap;
} Table;

void table_free(Table* t);
void table_set(Table* t, const char* key, Value value);  // adds the key or replaces its value
Value* table_get(const Table* t, const char* key);       // NULL if absent

// ---- tree-walker objects ----

// A scope. Scopes start out private to the call or block that made them and are freed with
// env_release() when it ends. Creating a function over a scope "captures" it (and its parents),
// which hands it over to the global object list so it outlives the call.
typedef struct ObjEnv {
    Obj obj;
    struct ObjEnv* parent;
    Table vars;
    bool captured;
} ObjEnv;

typedef struct {
    Obj obj;
    const struct Stmt* decl;
    ObjEnv* closure;  // the scope the function was defined in, which is what makes closures work
} ObjFunction;

ObjEnv* env_new(ObjEnv* parent);  // private until captured; the caller must env_release() it
void env_release(ObjEnv* env);    // frees a scope that was not captured; does nothing otherwise
void env_capture(ObjEnv* env);    // keeps `env` and its parents alive until exit
void env_define(ObjEnv* env, const char* name, Value value);
bool env_assign(ObjEnv* env, const char* name, Value value);  // false if no scope declares `name`
Value* env_lookup(const ObjEnv* env, const char* name);       // NULL if undefined
ObjFunction* function_new(const struct Stmt* decl, ObjEnv* closure);

// ---- VM objects ----

// A variable captured by a closure. While the variable's scope is still running it is "open"
// and lives on the stack at `slot`; when the scope ends it is "closed": the value moves into `closed`.
// Using a stack index instead of a pointer keeps it valid when the stack array grows.
typedef struct {
    Obj obj;
    size_t slot;
    bool open;
    Value closed;
} ObjUpvalue;

typedef struct {
    Obj obj;
    struct Proto* proto;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

ObjClosure* closure_new(struct Proto* proto);
ObjUpvalue* upvalue_new(size_t slot);

// ---- operations on values ----

bool is_truthy(Value v);  // only nil and false are falsy; 0 and "" are truthy
bool values_equal(Value a, Value b);
ObjString* value_to_string(Value v);  // how print and string + show a value
int format_number(char* buf, size_t cap, double d);

void objects_free_all(void);
