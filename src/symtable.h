#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>

// Symbol types
typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_PARAMETER
} SymbolType;

// Data types
typedef enum {
    TYPE_NULL,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_U8,
    TYPE_UNKNOWN
} DataType;

// Symbol structure
typedef struct Symbol {
    char *name;
    SymbolType symbol_type;
    DataType data_type;
    bool is_defined;
    bool is_used;
    bool is_constant;
    struct Symbol *next; // Linked list for collision resolution
} Symbol;

// Symbol table structure
typedef struct {
    Symbol **table;
    int size;        // Current size of the hash table
    int count;       // Number of symbols in the table
} SymTable;

// Function prototypes
void symtable_init(SymTable *symtable);
void symtable_free(SymTable *symtable);
Symbol *symtable_insert(SymTable *symtable, char *key, Symbol *symbol);
Symbol *symtable_search(SymTable *symtable, char *key);
void symtable_remove(SymTable *symtable, char *key);
bool is_symtable_all_used(SymTable *symtable);
// Hash function
unsigned int symtable_hash(char *key, int size);

#endif // SYMTABLE_H
