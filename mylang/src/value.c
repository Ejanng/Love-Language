#include "value.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "vm/chunk.h"

static Obj* all_objects = NULL;

static void* allocate_object(size_t size, ObjType type) {
    Obj* o = calloc(1, size);
    if (!o) {
        fputs("Out of memory.\n", stderr);
        exit(EXIT_FAILURE);
    }
    o->type = type;
    o->next = all_objects;
    all_objects = o;
    return o;
}

void objects_free_all(void) {
    while (all_objects) {
        Obj* next = all_objects->next;
        switch (all_objects->type) {
            case OBJ_ENV: table_free(&((ObjEnv*)all_objects)->vars); break;
            case OBJ_CLOSURE: free(((ObjClosure*)all_objects)->upvalues); break;
            default: break;
        }
        free(all_objects);
        all_objects = next;
    }
}

// ---- strings ----

static ObjString* string_alloc(size_t length) {
    ObjString* s = allocate_object(sizeof(ObjString) + length + 1, OBJ_STRING);
    s->length = length;
    s->chars[length] = '\0';
    return s;
}

ObjString* string_copy(const char* chars, size_t length) {
    ObjString* s = string_alloc(length);
    memcpy(s->chars, chars, length);
    return s;
}

ObjString* string_concat(const ObjString* a, const ObjString* b) {
    ObjString* s = string_alloc(a->length + b->length);
    memcpy(s->chars, a->chars, a->length);
    memcpy(s->chars + a->length, b->chars, b->length);
    return s;
}

// ---- table ----

static uint32_t hash_string(const char* key) {  // FNV-1a
    uint32_t h = 2166136261u;
    for (; *key; key++) {
        h ^= (uint8_t)*key;
        h *= 16777619u;
    }
    return h;
}

static TableEntry* find_entry(TableEntry* entries, int cap, const char* key, uint32_t hash) {
    uint32_t i = hash & (uint32_t)(cap - 1);  // cap is a power of two
    for (;;) {
        TableEntry* e = &entries[i];
        if (!e->key || (e->hash == hash && strcmp(e->key, key) == 0)) return e;
        i = (i + 1) & (uint32_t)(cap - 1);
    }
}

void table_free(Table* t) {
    free(t->entries);
    t->entries = NULL;
    t->count = t->cap = 0;
}

static void table_grow(Table* t) {
    int newCap = t->cap < 8 ? 8 : t->cap * 2;
    TableEntry* entries = calloc((size_t)newCap, sizeof(TableEntry));
    if (!entries) {
        fputs("Out of memory.\n", stderr);
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < t->cap; i++) {
        if (!t->entries[i].key) continue;
        *find_entry(entries, newCap, t->entries[i].key, t->entries[i].hash) = t->entries[i];
    }
    free(t->entries);
    t->entries = entries;
    t->cap = newCap;
}

void table_set(Table* t, const char* key, Value value) {
    if (t->count + 1 > t->cap * 3 / 4) table_grow(t);
    uint32_t hash = hash_string(key);
    TableEntry* e = find_entry(t->entries, t->cap, key, hash);
    if (!e->key) t->count++;
    e->key = key;
    e->hash = hash;
    e->value = value;
}

Value* table_get(const Table* t, const char* key) {
    if (t->count == 0) return NULL;
    TableEntry* e = find_entry(t->entries, t->cap, key, hash_string(key));
    return e->key ? &e->value : NULL;
}

// ---- environments and functions (tree-walker) ----

ObjEnv* env_new(ObjEnv* parent) {
    ObjEnv* env = calloc(1, sizeof(ObjEnv));
    if (!env) {
        fputs("Out of memory.\n", stderr);
        exit(EXIT_FAILURE);
    }
    env->obj.type = OBJ_ENV;
    env->parent = parent;
    return env;
}

void env_release(ObjEnv* env) {
    if (env->captured) return;
    table_free(&env->vars);
    free(env);
}

// Captured scopes join the global object list. A captured scope's parents are always captured too.
void env_capture(ObjEnv* env) {
    for (; env && !env->captured; env = env->parent) {
        env->captured = true;
        env->obj.next = all_objects;
        all_objects = &env->obj;
    }
}

void env_define(ObjEnv* env, const char* name, Value value) { table_set(&env->vars, name, value); }

bool env_assign(ObjEnv* env, const char* name, Value value) {
    Value* slot = env_lookup(env, name);
    if (!slot) return false;
    *slot = value;
    return true;
}

Value* env_lookup(const ObjEnv* env, const char* name) {
    for (; env; env = env->parent) {
        Value* v = table_get(&env->vars, name);
        if (v) return v;
    }
    return NULL;
}

ObjFunction* function_new(const Stmt* decl, ObjEnv* closure) {
    ObjFunction* fn = allocate_object(sizeof(ObjFunction), OBJ_FUNCTION);
    fn->decl = decl;
    fn->closure = closure;
    env_capture(closure);
    return fn;
}

// ---- closures and upvalues (VM) ----

ObjClosure* closure_new(Proto* proto) {
    ObjClosure* c = allocate_object(sizeof(ObjClosure), OBJ_CLOSURE);
    c->proto = proto;
    c->upvalueCount = proto->upvalueCount;
    if (c->upvalueCount > 0) c->upvalues = calloc((size_t)c->upvalueCount, sizeof(ObjUpvalue*));
    return c;
}

ObjUpvalue* upvalue_new(size_t slot) {
    ObjUpvalue* up = allocate_object(sizeof(ObjUpvalue), OBJ_UPVALUE);
    up->slot = slot;
    up->open = true;
    return up;
}

// ---- operations on values ----

bool is_truthy(Value v) {
    if (v.type == VAL_NIL) return false;
    if (v.type == VAL_BOOL) return v.as.boolean;
    return true;  // 0 and "" are truthy
}

bool values_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_NIL: return true;
        case VAL_NUMBER: return a.as.number == b.as.number;
        case VAL_BOOL: return a.as.boolean == b.as.boolean;
        case VAL_OBJ:
            if (a.as.obj == b.as.obj) return true;
            if (a.as.obj->type == OBJ_STRING && b.as.obj->type == OBJ_STRING) {
                const ObjString* x = (const ObjString*)a.as.obj;
                const ObjString* y = (const ObjString*)b.as.obj;
                return x->length == y->length && memcmp(x->chars, y->chars, x->length) == 0;
            }
            return false;  // functions are equal only if they are the same function
    }
    return false;
}

int format_number(char* buf, size_t cap, double d) {
    if (isnan(d)) return snprintf(buf, cap, "nan");
    if (isinf(d)) return snprintf(buf, cap, "%s", d < 0 ? "-inf" : "inf");
    return snprintf(buf, cap, "%.15g", d);
}

static ObjString* string_from_cstr(const char* s) { return string_copy(s, strlen(s)); }

ObjString* value_to_string(Value v) {
    switch (v.type) {
        case VAL_NIL: return string_from_cstr("nil");
        case VAL_BOOL: return string_from_cstr(v.as.boolean ? "true" : "false");
        case VAL_NUMBER: {
            char buf[64];
            format_number(buf, sizeof buf, v.as.number);
            return string_from_cstr(buf);
        }
        case VAL_OBJ: break;
    }
    switch (v.as.obj->type) {
        case OBJ_STRING: return AS_STRING(v);
        case OBJ_FUNCTION: {
            const char* name = ((ObjFunction*)v.as.obj)->decl->name;
            ObjString* s = string_alloc(strlen(name) + 5);
            snprintf(s->chars, s->length + 1, "<fn %s>", name);
            return s;
        }
        case OBJ_CLOSURE: {
            const char* name = ((ObjClosure*)v.as.obj)->proto->name;
            ObjString* s = string_alloc(strlen(name) + 5);
            snprintf(s->chars, s->length + 1, "<fn %s>", name);
            return s;
        }
        default: return string_from_cstr("<object>");
    }
}
