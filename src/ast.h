#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include "symtable.h"

// Типы узлов AST
typedef enum {
    NODE_PROGRAM,
    NODE_FUNCTION,
    NODE_VARIABLE_DECLARATION,
    NODE_ASSIGNMENT,
    NODE_BINARY_OPERATION,
    NODE_LITERAL,
    NODE_IDENTIFIER,
    NODE_IF,
    NODE_WHILE,
    NODE_RETURN,
    NODE_FUNCTION_CALL,
    NODE_BLOCK
} NodeType;

// Узел AST
typedef struct ASTNode {
    NodeType type;  // Тип узла
    DataType data_type;  // Тип данных, если применимо
    struct ASTNode* left;  // Левый потомок (используется для бинарных операций и условий)
    struct ASTNode* right; // Правый потомок (используется для бинарных операций)
    struct ASTNode* next;  // Следующий узел на том же уровне (используется для последовательностей операторов)
    struct ASTNode* condition; // Условие (например, для IF или WHILE)
    struct ASTNode* body;  // Тело цикла или условного оператора

    char* name;  // Имя функции, переменной или идентификатора
    char* value; // Значение для литералов
    struct ASTNode** parameters; // Параметры функции
    int param_count;  // Количество параметров

    struct ASTNode** arguments; // Аргументы функции при вызове
    int arg_count;  // Количество аргументов
} ASTNode;

// Функции для создания различных типов узлов AST
ASTNode* create_program_node();
ASTNode* create_function_node(char* name, DataType return_type, ASTNode** parameters, int param_count, ASTNode* body);
ASTNode* create_variable_declaration_node(char* name, DataType data_type, ASTNode* initializer);
ASTNode* create_assignment_node(char* name, ASTNode* value);
ASTNode* create_binary_operation_node(const char* operator_name, ASTNode* left, ASTNode* right);
ASTNode* create_literal_node(DataType type, char* value);
ASTNode* create_identifier_node(char* name);
ASTNode* create_if_node(ASTNode* condition, ASTNode* true_block, ASTNode* false_block);
ASTNode* create_while_node(ASTNode* condition, ASTNode* body);
ASTNode* create_return_node(ASTNode* value);
ASTNode* create_function_call_node(char* name, ASTNode** arguments, int arg_count);
ASTNode* create_block_node(ASTNode* statements, DataType return_type);

// Функции для освобождения памяти
void free_ast_node(ASTNode* node);

// Функции для отладки
void print_ast(ASTNode* node, int indent);

#endif // AST_H
