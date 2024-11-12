/*
 * Project Name: Implementation of the IFJ24 Imperative Language Compiler
 * Authors: [List the names and logins of team members who worked on this file]
 */

#include <stdio.h>
#include <stdlib.h>
#include "scanner.h"
#include "parser.h"
#include "error.h"
#include "symtable.h"
#include "ast.h" // Подключаем заголовочный файл для работы с AST
#include "codegen.h" // Подключаем заголовочный файл для генератора кода

int main(int argc, char *argv[]) {
    // Проверяем количество аргументов командной строки
   if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_file> <output_file>\n", argv[0]);
        return ERR_INTERNAL;  // Код 99 согласно спецификации для внутренних ошибок
    }

    // Открываем исходный файл для чтения
    FILE *source_file = fopen(argv[1], "r");
    if (!source_file) {
        fprintf(stderr, "Error opening file: %s\n", argv[1]);
        return ERR_INTERNAL;  // Код 99 согласно спецификации для внутренних ошибок
    }

    // Создаем и инициализируем сканер
    Scanner scanner;
    scanner_init(source_file, &scanner);

    // Инициализируем парсер с указанием сканера
    parser_init(&scanner);

    // Запускаем синтаксический анализатор и получаем корневой узел AST
    ASTNode* ast_root = parse_program(&scanner);

    // Печать AST для отладки
    print_ast(ast_root, 0);

    // Инициализация генератора кода с указанием выходного файла
    codegen_init(argv[2]);

    // Генерация кода на основе AST
    codegen_generate_program(ast_root);

    // Завершаем работу генератора кода и закрываем выходной файл
    codegen_finalize();

    // Освобождаем ресурсы сканера
    scanner_free(&scanner);

    // Освобождаем память AST
    free_ast_node(ast_root);

    // Закрываем исходный файл
    fclose(source_file);

    return ERR_OK;  // Код 0 согласно спецификации для успешного выполнения
}