#include "codegen.h"
#include "ast.h"
#include "symtable.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> 
#include "scanner.h"

// Глобальная переменная для хранения выходного файла
static FILE *output_file;

void codegen_generate_function(ASTNode *function_node);
void codegen_generate_block(FILE *output, ASTNode *block_node);
void codegen_generate_statement(FILE *output, ASTNode *statement_node);
void codegen_generate_expression(FILE *output, ASTNode *node);
void codegen_generate_function_call(FILE *output, ASTNode *node);
void codegen_generate_variable_declaration(FILE *output, ASTNode *declaration_node);
void codegen_generate_assignment(FILE *output, ASTNode *assignment_node);
void codegen_generate_return(FILE *output, ASTNode *return_node);
void codegen_generate_if(FILE *output, ASTNode *if_node);
void codegen_generate_while(FILE *output, ASTNode *while_node);

char *escape_ifj24_string(const char *input);
static int label_counter = 0;

int generate_unique_label() {
    return label_counter++;
}

// Инициализация генератора кода
void codegen_init(const char *filename) {
    output_file = fopen(filename, "w");
    if (!output_file) {
        fprintf(stderr, "Error: Cannot open output file %s for writing.\n", filename);
        exit(EXIT_FAILURE);
    }
    fprintf(output_file, ".IFJcode24\n"); // Заголовок для выходного файла
}

// Завершение генерации кода
void codegen_finalize() {
    if (output_file) {
        fclose(output_file);
        output_file = NULL;
    }
}

// Функция для генерации кода программы
void codegen_generate_program(ASTNode *program_node) {
    printf("CODEGEN: Generating program code\n");
    if (!program_node || program_node->type != NODE_PROGRAM) {
        fprintf(stderr, "!!!!!! Error: Invalid program node.\n");
        return;
    }

    ASTNode *current_function = program_node->body;
    printf("CODEGEN: Test current_function = %p\n", current_function);
    while (current_function) {
        printf("CODEGEN: Generating function %s\n", current_function->name);
        if (current_function->type == NODE_FUNCTION) {
            codegen_generate_function(current_function);
        }
        current_function = current_function->next;
    }
}

// Функция для генерации кода функции
void codegen_generate_function(ASTNode *function_node) {
    fprintf(output_file, "LABEL %s\n", function_node->name);
    fprintf(output_file, "PUSHFRAME\n");

    // Генерация параметров функции
    for (int i = 0; i < function_node->param_count; ++i) {
        fprintf(output_file, "DEFVAR LF@%s\n", function_node->parameters[i]->name);
    }

    // Генерация тела функции
    codegen_generate_block(output_file, function_node->body);

    fprintf(output_file, "POPFRAME\n");
    fprintf(output_file, "RETURN\n");
}

// Функция для генерации блока операторов
void codegen_generate_block(FILE *output, ASTNode *block_node) {
    ASTNode *current_statement = block_node->body;
    while (current_statement) {
        codegen_generate_statement(output, current_statement);
        current_statement = current_statement->next;
    }
}

void codegen_generate_expression(FILE *output, ASTNode *node) {
    if (node == NULL) {
        return;
    }

    switch (node->type) {
        case NODE_LITERAL:
            // Генерация кода для литерала
            if (node->data_type == TYPE_INT) {
                fprintf(output, "PUSHS int@%s\n", node->value);
            } else if (node->data_type == TYPE_FLOAT) {
                double float_value = atof(node->value); // Преобразуем строку в число
                fprintf(output, "PUSHS float@%.13a\n", float_value);
            } else if (node->data_type == TYPE_STRING) {
                char *escaped_value = escape_ifj24_string(node->value);
                fprintf(output, "PUSHS string@%s\n", escaped_value);
                free(escaped_value); 
            }
            break;

        case NODE_IDENTIFIER:
            // Генерация кода для идентификатора
            fprintf(output, "PUSHS LF@%s\n", node->name);
            break;

        case NODE_BINARY_OPERATION:
            // Рекурсивная генерация кода для левого и правого операндов
            codegen_generate_expression(output, node->left);
            codegen_generate_expression(output, node->right);

            if (node->name == NULL) {
                fprintf(stderr, "Error: node->name is NULL for BINARY_OPERATION\n");
                exit(EXIT_FAILURE);
            }
            // Генерация кода для операции
            if (strcmp(node->name, "+") == 0) {
                fprintf(output, "ADDS\n");
            } else if (strcmp(node->name, "-") == 0) {
                fprintf(output, "SUBS\n");
            } else if (strcmp(node->name, "*") == 0) {
                fprintf(output, "MULS\n");
            } else if (strcmp(node->name, "/") == 0) {
                fprintf(output, "DIVS\n");
            } else if (strcmp(node->name, "<") == 0) {
                fprintf(output, "LTS\n");
            } else if (strcmp(node->name, "<=") == 0) {
                fprintf(output, "LTS\n");
                fprintf(output, "PUSHS bool@true\n");
                fprintf(output, "ORS\n");
            } else if (strcmp(node->name, ">") == 0) {
                fprintf(output, "GTS\n");
            } else if (strcmp(node->name, ">=") == 0) {
                fprintf(output, "GTS\n");
                fprintf(output, "PUSHS bool@true\n");
                fprintf(output, "ORS\n");
            } else if (strcmp(node->name, "==") == 0) {
                fprintf(output, "EQS\n");
            } else if (strcmp(node->name, "!=") == 0) {
                fprintf(output, "EQS\n");
                fprintf(output, "NOTS\n");
            }
            break;

        case NODE_FUNCTION_CALL:
            // Генерация вызова функции
            codegen_generate_function_call(output, node);
            break;

        default:
            fprintf(stderr, "Unsupported expression type for code generation\n");
            exit(1);
    }
}

void codegen_generate_function_call(FILE *output, ASTNode *node) {
    if (node == NULL || node->type != NODE_FUNCTION_CALL) {
        fprintf(stderr, "Invalid function call node for code generation\n");
        exit(1);
    }

    // Генерация кода для аргументов функции (обратный порядок для стека)
    for (int i = node->arg_count - 1; i >= 0; --i) {
        codegen_generate_expression(output, node->arguments[i]);
    }

        if (strcmp(node->name, "ifj.readi32") == 0) {
            fprintf(output, "READS int\n");
        } else if (strcmp(node->name, "ifj.readf64") == 0) {
            fprintf(output, "READS float\n");
        } else if (strcmp(node->name, "ifj.readstr") == 0) {
            fprintf(output, "READS string\n");
        } else if (strcmp(node->name, "ifj.write") == 0) {
            fprintf(output, "WRITE\n");
        } else if (strcmp(node->name, "ifj.i2f") == 0) {
            fprintf(output, "INT2FLOATS\n");
        } else if (strcmp(node->name, "ifj.f2i") == 0) {
            fprintf(output, "FLOAT2INTS\n");
        } else if (strcmp(node->name, "ifj.length") == 0) {
            fprintf(output, "STRLEN\n");
        } else if (strcmp(node->name, "ifj.concat") == 0) {
            fprintf(output, "CONCAT\n");
        } else if (strcmp(node->name, "ifj.substring") == 0) {
            fprintf(output, "SUBSTRING\n");
        } else if (strcmp(node->name, "ifj.strcmp") == 0) {
            fprintf(output, "STRCMP\n");
        } else if (strcmp(node->name, "ifj.ord") == 0) {
            fprintf(output, "ORD\n");
        } else if (strcmp(node->name, "ifj.chr") == 0) {
            fprintf(output, "CHR\n");
        } else if (strcmp(node->name, "ifj.string") == 0) {
            // Обработка функции ifj.string
            fprintf(output, "CREATEFRAME\n"); // Создание нового временного фрейма
            fprintf(output, "MOVE LF@result LF@%s\n", node->arguments[0]->name); // Предположим, что первый аргумент - это имя переменной
            // Здесь вы можете добавить дополнительные инструкции для обработки строки
            // Например, вы можете использовать PUSHS для переменных строк
    } else {
        // Генерация пользовательского вызова функции
        fprintf(output, "CALL %s\n", node->name);
    }
}


void codegen_generate_statement(FILE *output, ASTNode *node) {
    if (node == NULL) {
        fprintf(stderr, "Invalid statement node for code generation\n");
        exit(1);
    }

    switch (node->type) {
        case NODE_VARIABLE_DECLARATION:
            // Генерация кода для объявления переменной
            if (node->left != NULL) {
                // Генерация кода для инициализатора
                codegen_generate_expression(output, node->left);
                fprintf(output, "DEFVAR %s\n", node->name);
                fprintf(output, "POPS %s\n", node->name);
            } else {
                fprintf(output, "DEFVAR %s\n", node->name);
            }
            break;

        case NODE_ASSIGNMENT:
            // Генерация кода для присваивания
            codegen_generate_expression(output, node->left);
            fprintf(output, "POPS %s\n", node->name);
            break;

        case NODE_RETURN:
            // Генерация кода для оператора возврата
            if (node->left != NULL) {
                codegen_generate_expression(output, node->left);
                fprintf(output, "RETURN\n");
            }
            break;

        case NODE_IF:
            // Генерация кода для оператора if
            {
                int label_num = generate_unique_label();
                codegen_generate_expression(output, node->condition);
                fprintf(output, "PUSHS bool@true\n");
                fprintf(output, "JUMPIFNEQS $else_%d\n", label_num);
                codegen_generate_block(output, node->body); // Генерация кода для тела if
                fprintf(output, "JUMP $endif_%d\n", label_num);
                fprintf(output, "LABEL $else_%d\n", label_num);
                if (node->left != NULL) {
                    codegen_generate_block(output, node->left); // Генерация кода для тела else
                }
                fprintf(output, "LABEL $endif_%d\n", label_num);
            }
            break;

        case NODE_WHILE:
            // Генерация кода для оператора while
            {
                int label_num = generate_unique_label();
                fprintf(output, "LABEL $while_start_%d\n", label_num);
                codegen_generate_expression(output, node->condition);
                fprintf(output, "PUSHS bool@true\n");
                fprintf(output, "JUMPIFNEQS $while_end_%d\n", label_num);
                codegen_generate_block(output, node->body);
                fprintf(output, "JUMP $while_start_%d\n", label_num);
                fprintf(output, "LABEL $while_end_%d\n", label_num);
            }
            break;

        case NODE_FUNCTION_CALL:
            // Генерация кода для вызова функции
            codegen_generate_function_call(output, node);
            break;

        default:
            fprintf(stderr, "Unsupported statement type for code generation\n");
            exit(1);
    }
}


void codegen_generate_variable_declaration(FILE *output, ASTNode *declaration_node) {
    fprintf(output, "DEFVAR LF@%s\n", declaration_node->name);
    if (declaration_node->left) { // Если есть начальное значение
        codegen_generate_expression(output, declaration_node->left);
        fprintf(output, "POPS LF@%s\n", declaration_node->name);
    }
}


void codegen_generate_assignment(FILE *output, ASTNode *assignment_node) {
    codegen_generate_expression(output, assignment_node->left);
    fprintf(output, "POPS LF@%s\n", assignment_node->name);
}


void codegen_generate_return(FILE *output, ASTNode *return_node) {
    if (return_node->left) { // Если есть возвращаемое значение
        codegen_generate_expression(output, return_node->left);
    }
    fprintf(output, "RETURN\n");
}


void codegen_generate_if(FILE *output, ASTNode *if_node) {
    static int if_label_count = 0;
    int current_label = if_label_count++;
    
    codegen_generate_expression(output, if_node->condition);
    fprintf(output, "PUSHS bool@true\n");
    fprintf(output, "JUMPIFNEQS $else_%d\n", current_label);
    
    codegen_generate_block(output, if_node->body);
    fprintf(output, "JUMP $endif_%d\n", current_label);
    
    fprintf(output, "LABEL $else_%d\n", current_label);
    if (if_node->left) { // Если есть блок else
        codegen_generate_block(output, if_node->left);
    }
    
    fprintf(output, "LABEL $endif_%d\n", current_label);
}


void codegen_generate_while(FILE *output, ASTNode *while_node) {
    static int while_label_count = 0;
    int current_label = while_label_count++;
    
    fprintf(output, "LABEL $while_start_%d\n", current_label);
    codegen_generate_expression(output, while_node->condition);
    fprintf(output, "PUSHS bool@true\n");
    fprintf(output, "JUMPIFNEQS $while_end_%d\n", current_label);
    
    codegen_generate_block(output, while_node->body);
    fprintf(output, "JUMP $while_start_%d\n", current_label);
    
    fprintf(output, "LABEL $while_end_%d\n", current_label);
}

char *escape_ifj24_string(const char *input) {
    size_t length = strlen(input);
    size_t buffer_size = length * 4 + 1; // запас для экранирования
    char *escaped_string = malloc(buffer_size);
    if (!escaped_string) {
        fprintf(stderr, "Memory allocation failed for escaped string.\n");
        exit(EXIT_FAILURE);
    }

    size_t index = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = input[i];

        // Проверяем, нужно ли экранировать символ
        if (c <= 32 || c == 35 || c == 92) { // ASCII 0-32, 35 (#), 92 (\)
            index += snprintf(escaped_string + index, buffer_size - index, "\\%03d", c);
        } else if (isprint(c)) {
            escaped_string[index++] = c;
        } else {
            fprintf(stderr, "Unsupported character in string literal.\n");
            exit(EXIT_FAILURE);
        }
    }

    escaped_string[index] = '\0';
    return escaped_string;
}