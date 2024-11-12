// parser.h
#ifndef PARSER_H
#define PARSER_H

#include "tokens.h"
#include "symtable.h"
#include "ast.h"
#include "scanner.h"
#include "string.h"

// Initializes the parser
void parser_init(Scanner *scanner);

// Starts parsing the input program
ASTNode* parse_program(Scanner *scanner);

// Parses a single function
ASTNode* parse_function(Scanner *scanner, bool is_definition);

// Parses a statement (expression, if, while, return, etc.)
ASTNode* parse_statement(Scanner *scanner);

ASTNode* parse_parameter(Scanner *scanner);

ASTNode* parse_expression(Scanner *scanner);

typedef struct {
    const char *name;             // Имя функции
    DataType return_type;         // Тип возвращаемого значения
    DataType param_types[3];      // Массив типов параметров (максимум 3 для примера)
    size_t param_count;           // Количество параметров
} BuiltinFunctionInfo;

// Объявляем переменную как extern
extern BuiltinFunctionInfo builtin_functions[];

// Объявляем функцию
bool is_builtin_function(const char *identifier, Scanner *scanner);

#endif // PARSER_H
