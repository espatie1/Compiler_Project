#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h" // Подключаем заголовочный файл с определением структуры AST
#include <stdio.h>

// Инициализация генератора кода (например, открытие файлов вывода)
void codegen_init(const char *filename);

// Завершение работы генератора кода (например, закрытие файлов)
void codegen_finalize();

// Функция для генерации кода для всей программы
void codegen_generate_program(ASTNode *program_node);

// Функция для генерации кода для отдельной функции
void codegen_generate_function(ASTNode *function_node);

// Функция для генерации блока операторов
void codegen_generate_block(FILE *output, ASTNode *block_node);

// Функция для генерации кода для отдельного оператора
void codegen_generate_statement(FILE *output, ASTNode *statement_node);

// Функция для генерации кода для выражения
void codegen_generate_expression(FILE *output, ASTNode *expression_node);

// Функция для генерации вызова функции
void codegen_generate_function_call(FILE *output, ASTNode *node);

// Функция для генерации объявления переменной
void codegen_generate_variable_declaration(FILE *output, ASTNode *declaration_node);

// Функция для генерации присваивания
void codegen_generate_assignment(FILE *output, ASTNode *assignment_node);

// Функция для генерации оператора return
void codegen_generate_return(FILE *output, ASTNode *return_node);

// Функция для генерации оператора if
void codegen_generate_if(FILE *output, ASTNode *if_node);

// Функция для генерации оператора while
void codegen_generate_while(FILE *output, ASTNode *while_node);

// Вспомогательная функция для получения уникальной метки (если требуется)
int generate_unique_label();

#endif // CODEGEN_H
