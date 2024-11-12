#include "parser.h"
#include "error.h"
#include "scanner.h"
#include "string.h"
#include "symtable.h"
#include "tokens.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG_PARSER
#ifdef DEBUG_PARSER
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

// Forward declarations of helper functions
static void expect_token(TokenType expected_type, Scanner *scanner);
static ASTNode *parse_block(Scanner *scanner);
static ASTNode *parse_variable_declaration(Scanner *scanner);
static ASTNode *parse_variable_assigning(Scanner *scanner);
static ASTNode *parse_if_statement(Scanner *scanner);
static ASTNode *parse_while_statement(Scanner *scanner);
static ASTNode *parse_return_statement(Scanner *scanner);
static ASTNode *parse_import(Scanner *scanner);
static ASTNode *parse_primary_expression(Scanner *scanner);
static ASTNode *parse_binary_operation(Scanner *scanner, ASTNode *left_node);
DataType parse_type(Scanner *scanner);        // Объявляем функцию заранее
DataType parse_return_type(Scanner *scanner); // Объявляем функцию заранее
void check_return_types(ASTNode *function_node, DataType return_type, int *block_layer);
bool type_convertion(ASTNode *main_node);
void parse_functions_declaration(Scanner *scanner);


// Global token storage
static Token current_token;
static SymTable symtable; // Global symbol table for the program

BuiltinFunctionInfo builtin_functions[] = {
    {"readstr", TYPE_STRING, {}, 0},
    {"readi32", TYPE_INT, {}, 0},
    {"readf64", TYPE_FLOAT, {}, 0},
    {"write", TYPE_VOID, {TYPE_UNKNOWN}, 1},
    {"i2f", TYPE_FLOAT, {TYPE_INT}, 1},
    {"f2i", TYPE_INT, {TYPE_FLOAT}, 1},
    {"string", TYPE_U8, {TYPE_UNKNOWN}, 1},
    {"length", TYPE_INT, {TYPE_STRING}, 1},
    {"concat", TYPE_U8, {TYPE_U8, TYPE_U8}, 2},
    {"substring", TYPE_STRING, {TYPE_STRING, TYPE_INT, TYPE_INT}, 3},
    {"strcmp", TYPE_INT, {TYPE_U8, TYPE_U8}, 2},
    {"ord", TYPE_INT, {TYPE_STRING, TYPE_INT}, 2},
    {"chr", TYPE_STRING, {TYPE_INT}, 1}
    // Добавьте другие функции по необходимости
};

bool is_builtin_function(const char *identifier, Scanner *scanner)
{
    if (strcmp(identifier, "ifj") != 0){
        return false;
    }
    LOG("!!!!DEBUG_PARSER: Recognized 'ifj' keyword\n");
    current_token = get_next_token(scanner);
    expect_token(TOKEN_DOT, scanner);
    identifier = current_token.lexeme;
    LOG("!!!!DEBUG_PARSER: Recognized identifier: %s\n", identifier);
    for (size_t i = 0; i < sizeof(builtin_functions) / sizeof(builtin_functions[0]); i++)
    {
        if (strcmp(identifier, builtin_functions[i].name) == 0)
        {
            LOG("!!!!DEBUG_PARSER: Recognized builtin function: %s\n", identifier);
            return true;
        }
    }
    return false;
}
// Определение типа данных встроенных функций с использованием словаря
DataType get_builtin_function_type(const char *function_name)
{
    size_t num_functions = sizeof(builtin_functions) / sizeof(builtin_functions[0]);
    for (size_t i = 0; i < num_functions; i++)
    {
        if (strcmp(function_name, builtin_functions[i].name) == 0)
        {
            return builtin_functions[i].return_type;
        }
    }
    return TYPE_UNKNOWN; // Неизвестная функция
}

void parser_init(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Initializing parser...\n");
    // Initialize the symbol table
    symtable_init(&symtable);
    // Get the first token to start parsing
    current_token = get_next_token(scanner);
}

ASTNode *parse_program(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing program\n");

    // Создаем узел программы
    ASTNode *program_node = create_program_node();
    ASTNode *current_function = NULL; // Указатель на текущую функцию

    // Парсим обязательное выражение импорта и добавляем его в дерево
    ASTNode *import_node = parse_import(scanner);
    program_node->next = import_node;

    parse_functions_declaration(scanner);
    // Продолжаем парсить остальную часть программы
    while (current_token.type != TOKEN_EOF)
    {
        if ((current_token.type == TOKEN_PUB) || (current_token.type == TOKEN_FN))
        {
            ASTNode *function_node = parse_function(scanner, true);
            if (program_node->body == NULL)
            {
                program_node->body = function_node; // Устанавливаем первую функцию
            }
            else
            {
                current_function->next = function_node; // Присоединяем к текущему
            }
            current_function = function_node; // Обновляем указатель на текущую функцию
        }
        else
        {
            error_exit(ERR_SYNTAX, "Expected function definition.");
        }
    }

    if (!is_symtable_all_used(&symtable))
    {
        error_exit(ERR_SYNTAX, "Unused variable");
    }
    return program_node;
}

void parse_functions_declaration(Scanner *scanner)
{
    Scanner saved_scanner_state = *scanner;
    FILE saved_input = *scanner->input;
    Token saved_token = current_token;
    
    while (current_token.type != TOKEN_EOF)
    {
        if ((current_token.type == TOKEN_PUB) || (current_token.type == TOKEN_FN))
        {
            parse_function(scanner, false);
        }
        else
        {
            error_exit(ERR_SYNTAX, "Expected function definition.");
        }
    }
    *scanner = saved_scanner_state;
    *scanner->input = saved_input;
    current_token = saved_token;
}

ASTNode *parse_function(Scanner *scanner, bool is_definition)
{
    LOG("DEBUG_PARSER: Parsing function\n");
    expect_token(TOKEN_PUB, scanner); // Ожидаем ключевое слово 'pub'
    expect_token(TOKEN_FN, scanner);  // Ожидаем ключевое слово 'fn'

    if (current_token.type != TOKEN_IDENTIFIER)
    {
        error_exit(ERR_SYNTAX, "Expected function name.");
    }

    char *function_name = string_duplicate(current_token.lexeme);
    if (function_name == NULL)
    {
        error_exit(ERR_INTERNAL, "Memory allocation failed for function name.");
    }
    current_token = get_next_token(scanner);

    expect_token(TOKEN_LEFT_PAREN, scanner); // '('

    // Парсим параметры функции
    ASTNode **parameters = NULL;
    int param_count = 0;
    if (current_token.type != TOKEN_RIGHT_PAREN)
    {
        parameters = (ASTNode **)malloc(sizeof(ASTNode *));
        parameters[param_count++] = parse_parameter(scanner);

        while (current_token.type == TOKEN_COMMA)
        {
            current_token = get_next_token(scanner);
            parameters = (ASTNode **)realloc(parameters, (param_count + 1) * sizeof(ASTNode *));
            parameters[param_count++] = parse_parameter(scanner);
        }
    }
    expect_token(TOKEN_RIGHT_PAREN, scanner); // ')'

    // Ожидаем возвращаемый тип
    DataType return_type = parse_return_type(scanner);
    ASTNode *function_node = NULL;

    if (is_definition)
    {
        // Создаем узел функции
        ASTNode *body_node = parse_block(scanner);
        function_node = create_function_node(function_name, return_type, parameters, param_count, body_node);

        int block_layer = 0;
        check_return_types(function_node->body->body, return_type, &block_layer);
    }
    else
    {
        while(current_token.type != TOKEN_RIGHT_BRACE){
            current_token = get_next_token(scanner);
        }
        current_token = get_next_token(scanner);
    
    // Добавляем функцию в таблицу символов
    Symbol *function_symbol = symtable_search(&symtable, function_name);
    if (function_symbol != NULL)
    {
        error_exit(ERR_SEMANTIC, "Function already defined.");
    }

    Symbol *new_function = (Symbol *)malloc(sizeof(Symbol));
    if (new_function == NULL)
    {
        error_exit(ERR_INTERNAL, "Memory allocation failed for new function symbol");
    }

    new_function->name = string_duplicate(function_name);
    if (new_function->name == NULL)
    {
        free(new_function);
        error_exit(ERR_INTERNAL, "Memory allocation failed for function name");
    }

    new_function->symbol_type = SYMBOL_FUNCTION;
    new_function->data_type = return_type;
    new_function->is_defined = true;
    new_function->is_used = strcmp(new_function->name, "main") == 0 ? true : false;
    new_function->next = NULL;

    symtable_insert(&symtable, function_name, new_function);
    }

    return function_node;
}

ASTNode *parse_parameter(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing parameter\n");

    // Ожидаем имя параметра
    if (current_token.type != TOKEN_IDENTIFIER)
    {
        error_exit(ERR_SYNTAX, "Expected parameter name.");
    }

    LOG("DEBUG_PARSER: current_token.lexeme before param: %s\n", current_token.lexeme);
    char *param_name = string_duplicate(current_token.lexeme);
    if (param_name == NULL)
    {
        error_exit(ERR_INTERNAL, "Memory allocation failed for parameter name.");
    }

    current_token = get_next_token(scanner);

    // Ожидаем двоеточие ':'
    expect_token(TOKEN_COLON, scanner);

    // Ожидаем тип параметра
    DataType param_type = parse_type(scanner);

    // Проверяем существование параметра в таблице символов
    LOG("DEBUG_PARSER: param_name before symtable_search: %s\n", param_name);
    Symbol *param_symbol = symtable_search(&symtable, param_name);
    if (param_symbol != NULL)
    {
        free(param_name);
        error_exit(ERR_SEMANTIC, "Parameter already defined.");
    }

    // Создаем новый символ и добавляем его в таблицу символов
    Symbol *new_param = (Symbol *)malloc(sizeof(Symbol));
    if (new_param == NULL)
    {
        free(param_name);
        error_exit(ERR_INTERNAL, "Memory allocation failed for parameter symbol.");
    }

    new_param->name = param_name;
    new_param->symbol_type = SYMBOL_PARAMETER;
    new_param->data_type = param_type;
    new_param->is_defined = true;
    new_param->next = NULL;

    symtable_insert(&symtable, param_name, new_param);

    // Создаем узел параметра AST
    ASTNode *param_node = create_variable_declaration_node(param_name, param_type, NULL);

    return param_node;
}

DataType parse_type(Scanner *scanner)
{
    if (current_token.type == TOKEN_I32)
    {
        current_token = get_next_token(scanner); // Обновляем токен после разбора типа
        return TYPE_INT;
    }
    else if (current_token.type == TOKEN_F64)
    {
        current_token = get_next_token(scanner); // Обновляем токен после разбора типа
        return TYPE_FLOAT;
    }
    else if (current_token.type == TOKEN_U8)
    {
        current_token = get_next_token(scanner); // Обновляем токен после разбора типа
        return TYPE_U8;
    }
    else if (current_token.type == TOKEN_VOID)
    {
        current_token = get_next_token(scanner); // Обновляем токен после разбора типа
        return TYPE_VOID;
    }
    else if (current_token.type == TOKEN_ASSIGN)
    {
        current_token = get_next_token(scanner);
        return TYPE_UNKNOWN;
    }
    else
    {
        error_exit(ERR_SYNTAX, "Expected type.");
        return TYPE_UNKNOWN;
    }
}
// Function to parse the return type (same as parameter type parsing)
DataType parse_return_type(Scanner *scanner)
{
    if (current_token.type == TOKEN_I32)
    {
        current_token = get_next_token(scanner); // Переходим к следующему токену
        return TYPE_INT;
    }
    else if (current_token.type == TOKEN_F64)
    {
        current_token = get_next_token(scanner);
        return TYPE_FLOAT;
    }
    else if (current_token.type == TOKEN_U8)
    {
        current_token = get_next_token(scanner);
        return TYPE_U8;
    }
    else if (current_token.type == TOKEN_VOID)
    {
        current_token = get_next_token(scanner);
        return TYPE_VOID;
    }
    else
    {
        error_exit(ERR_SYNTAX, "Expected return type.");
        return TYPE_UNKNOWN;
    }
}

// Parses a block of statements enclosed in {}
ASTNode *parse_block(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing block\n");
    expect_token(TOKEN_LEFT_BRACE, scanner); // Ожидаем '{'

    // Создаем узел блока
    ASTNode *block_node = create_block_node(NULL, TYPE_NULL);
    ASTNode *current_statement = NULL;

    // Парсим операторы в теле функции до тех пор, пока не встретим '}'
    while (current_token.type != TOKEN_RIGHT_BRACE)
    {
        ASTNode *statement_node = parse_statement(scanner);

        if (!block_node->body)
        {
            block_node->body = statement_node;
        }
        else
        {
            current_statement->next = statement_node;
        }
        current_statement = statement_node;
    }

    // Ожидаем закрывающую скобку '}'
    expect_token(TOKEN_RIGHT_BRACE, scanner);

    return block_node;
}

ASTNode *parse_statement(Scanner *scanner)
{
    if (current_token.type == TOKEN_VAR || current_token.type == TOKEN_CONST)
    {
        return parse_variable_declaration(scanner);
    }
    else if (current_token.type == TOKEN_IF)
    {
        return parse_if_statement(scanner);
    }
    else if (current_token.type == TOKEN_WHILE)
    {
        return parse_while_statement(scanner);
    }
    else if (current_token.type == TOKEN_RETURN)
    {
        return parse_return_statement(scanner); // Обрабатываем return и возвращаем узел
    }
    else if (current_token.type == TOKEN_IDENTIFIER)
    {
        // Парсим как выражение присваивания и возвращаем узел
        return parse_variable_assigning(scanner);
    }
    else
    {
        error_exit(ERR_SYNTAX, "Invalid statement.");
        return NULL; // На случай ошибки, хотя сюда выполнение не дойдет
    }
}

ASTNode *parse_variable_assigning(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing variable assigning\n");

    // Проверяем существование переменной или функции в таблице символов
    Symbol *symbol = symtable_search(&symtable, current_token.lexeme);
    bool is_builtin = is_builtin_function(current_token.lexeme, scanner);
    if (symbol == NULL && !(is_builtin))
    {
        error_exit(ERR_SYNTAX, "Variable or function %s is not defined.", current_token.lexeme);
    } 

    if (symbol->is_constant)
    {
        error_exit(ERR_SEMANTIC_OTHER, "Constatn variable can't be modified");
    }
    char *name = string_duplicate(current_token.lexeme);
    if (is_builtin) {
        // Adding ifj. to the name of the function
        char *new_name = (char *)malloc(strlen(name) + 5);
        if (new_name == NULL)
        {
            free(name);
            error_exit(ERR_INTERNAL, "Memory allocation failed for new function name.");
        }
        strcpy(new_name, "ifj.");
        strcat(new_name, name);
        free(name);
        name = new_name;
    }

    // Сохраняем имя для создания узла AST

    current_token = get_next_token(scanner);

    // Проверяем, является ли это вызовом функции
    if (current_token.type == TOKEN_LEFT_PAREN)
    {
        current_token = get_next_token(scanner); // Пропускаем '('

        // Парсим аргументы функции
        ASTNode **arguments = NULL;
        int arg_count = 0;

        if (current_token.type != TOKEN_RIGHT_PAREN)
        {
            arguments = (ASTNode **)malloc(sizeof(ASTNode *));
            arguments[arg_count++] = parse_expression(scanner);

            while (current_token.type == TOKEN_COMMA)
            {
                current_token = get_next_token(scanner);
                arguments = (ASTNode **)realloc(arguments, (arg_count + 1) * sizeof(ASTNode *));
                arguments[arg_count++] = parse_expression(scanner);
            }
        }

        expect_token(TOKEN_RIGHT_PAREN, scanner); // Ожидаем ')'
        expect_token(TOKEN_SEMICOLON, scanner);   // Ожидаем ';' после вызова функции

        // Создаем узел для вызова функции и возвращаем его
        return create_function_call_node(name, arguments, arg_count);
    }
    else
    {
        // Логика для присваивания
        expect_token(TOKEN_ASSIGN, scanner);

        // Парсим выражение для присваивания
        ASTNode *value_node = parse_expression(scanner);

        // Ожидаем точку с запятой в конце оператора
        expect_token(TOKEN_SEMICOLON, scanner);

        // Создаем узел присваивания и возвращаем его
        return create_assignment_node(name, value_node);
    }
}

ASTNode *parse_variable_declaration(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing variable declaration\n");
    TokenType var_type = current_token.type; // 'var' или 'const'
    current_token = get_next_token(scanner);

    if (current_token.type != TOKEN_IDENTIFIER)
    {
        error_exit(ERR_SYNTAX, "Expected variable name.");
    }
    if (current_token.lexeme == NULL)
    {
        error_exit(ERR_INTERNAL, "Lexeme is NULL before strdup.");
    }
    char *variable_name = string_duplicate(current_token.lexeme);
    if (variable_name == NULL)
    {
        error_exit(ERR_INTERNAL, "Memory allocation failed for variable name.");
    }
    current_token = get_next_token(scanner);

    DataType declaration_type = TYPE_UNKNOWN;

    // Проверяем наличие типа переменной
    if (current_token.type == TOKEN_COLON)
    {
        current_token = get_next_token(scanner);
        declaration_type = parse_type(scanner);
    }

    expect_token(TOKEN_ASSIGN, scanner); // Ожидаем '='

    // Парсим выражение для присваивания
    ASTNode *initializer_node = parse_expression(scanner);
    DataType expr_type = initializer_node->data_type;

    // Проверка на совпадение типов
    if (declaration_type != TYPE_UNKNOWN && expr_type != declaration_type)
    {
        error_exit(ERR_SEMANTIC, "Declared type of variable does not match the assigned type. %d %d,\n Line and column: %d %d\n", declaration_type, expr_type, current_token.line, current_token.column);
    }
    else if (declaration_type == TYPE_UNKNOWN)
    {
        declaration_type = expr_type;
    }

    // Семантическая проверка типа
    if (var_type == TOKEN_VAR && expr_type == TYPE_UNKNOWN)
    {
        error_exit(ERR_SEMANTIC, "Unknown data type assignment.");
    }

    expect_token(TOKEN_SEMICOLON, scanner); // Ожидаем ';' в конце оператора

    // Проверяем существование переменной в таблице символов
    Symbol *symbol = symtable_search(&symtable, variable_name);
    if (symbol != NULL)
    {
        error_exit(ERR_SEMANTIC, "Variable already defined.");
    }

    // Создаем новый символ и добавляем его в таблицу символов
    Symbol *new_var = (Symbol *)malloc(sizeof(Symbol));
    if (new_var == NULL)
    {
        free(variable_name);
        error_exit(ERR_INTERNAL, "Memory allocation failed for parameter symbol.");
    }
    new_var->name = variable_name;
    new_var->symbol_type = SYMBOL_VARIABLE;
    new_var->data_type = expr_type;
    new_var->is_defined = true;
    new_var->is_constant = (var_type == TOKEN_CONST) ? true : false;
    new_var->next = NULL;

    symtable_insert(&symtable, variable_name, new_var);

    // Создаем узел объявления переменной и возвращаем его
    return create_variable_declaration_node(variable_name, declaration_type, initializer_node);
}

ASTNode *parse_if_statement(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing if statement\n");
    expect_token(TOKEN_IF, scanner);         // 'if'
    expect_token(TOKEN_LEFT_PAREN, scanner); // '('

    // Парсим условие
    ASTNode *condition_node = parse_expression(scanner);

    // Семантическая проверка на тип bool для условия
    if (condition_node->data_type != TYPE_BOOL)
    {
        error_exit(ERR_SEMANTIC, "Condition in if statement must be boolean.");
    }

    expect_token(TOKEN_RIGHT_PAREN, scanner); // ')'

    // Парсим тело 'if'
    ASTNode *true_block = parse_block(scanner);

    // Парсим необязательный блок 'else', если он есть
    ASTNode *false_block = NULL;
    if (current_token.type == TOKEN_ELSE)
    {
        current_token = get_next_token(scanner);
        false_block = parse_block(scanner);
    }

    if (false_block != NULL)
    {
        if (true_block->data_type != false_block->data_type)
        {
            error_exit(ERR_SEMANTIC_PARAMS, "Incompabile type of return expression");
        }
    }
    // Создаем узел 'if' и возвращаем его
    return create_if_node(condition_node, true_block, false_block);
}

// Function to parse a while statement
ASTNode *parse_while_statement(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing while statement\n");
    expect_token(TOKEN_WHILE, scanner);      // 'while'
    expect_token(TOKEN_LEFT_PAREN, scanner); // '('

    // Парсим условие
    ASTNode *condition_node = parse_expression(scanner);

    // Семантическая проверка на тип bool для условия
    if (condition_node->data_type != TYPE_BOOL)
    {
        error_exit(ERR_SEMANTIC, "Condition in while statement must be boolean.");
    }

    expect_token(TOKEN_RIGHT_PAREN, scanner); // ')'

    // Парсим тело 'while'
    ASTNode *body_node = parse_block(scanner);

    // Создаем узел 'while' и возвращаем его
    return create_while_node(condition_node, body_node);
}

ASTNode *parse_return_statement(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing return statement\n");
    expect_token(TOKEN_RETURN, scanner); // Ожидаем ключевое слово 'return'

    ASTNode *return_value_node = NULL;

    // Проверяем, есть ли выражение после 'return'
    if (current_token.type != TOKEN_SEMICOLON)
    {
        return_value_node = parse_expression(scanner);
    }

    expect_token(TOKEN_SEMICOLON, scanner); // Ожидаем ';' после оператора return
    LOG("DEBUG_PARSER: Finished parsing return statement\n");

    // Создаем узел 'return' и возвращаем его
    return create_return_node(return_value_node);
}

// Function to parse an expression and return its data type
ASTNode *parse_expression(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing expression. Current token: %d\n. Line and column: %d %d\n", current_token.type, current_token.line, current_token.column);

    // Разбор первой части выражения и создание узла AST
    ASTNode *left_node = parse_primary_expression(scanner);
    bool is_boolean_expression = false;
    DataType expression_type = left_node->data_type;

    // Проверка наличия бинарного оператора и создание узлов для бинарных операций
    while (current_token.type == TOKEN_PLUS || current_token.type == TOKEN_MINUS ||
           current_token.type == TOKEN_MULTIPLY || current_token.type == TOKEN_DIVIDE ||
           current_token.type == TOKEN_LESS || current_token.type == TOKEN_LESS_EQUAL ||
           current_token.type == TOKEN_GREATER || current_token.type == TOKEN_GREATER_EQUAL ||
           current_token.type == TOKEN_EQUAL || current_token.type == TOKEN_NOT_EQUAL)
    {

        NodeType op_type;

        // Определяем тип узла операции на основе токена
        switch (current_token.type)
        {
        case TOKEN_PLUS:
            op_type = NODE_BINARY_OPERATION;
            break;
        case TOKEN_MINUS:
            op_type = NODE_BINARY_OPERATION;
            break;
        case TOKEN_MULTIPLY:
            op_type = NODE_BINARY_OPERATION;
            break;
        case TOKEN_DIVIDE:
            op_type = NODE_BINARY_OPERATION;
            break;
        case TOKEN_LESS:
            op_type = NODE_BINARY_OPERATION;
            is_boolean_expression = true;
            break;
        case TOKEN_LESS_EQUAL:
            op_type = NODE_BINARY_OPERATION;
            is_boolean_expression = true;
            break;
        case TOKEN_GREATER:
            op_type = NODE_BINARY_OPERATION;
            is_boolean_expression = true;
            break;
        case TOKEN_GREATER_EQUAL:
            op_type = NODE_BINARY_OPERATION;
            is_boolean_expression = true;
            break;
        case TOKEN_EQUAL:
            op_type = NODE_BINARY_OPERATION;
            is_boolean_expression = true;
            break;
        case TOKEN_NOT_EQUAL:
            op_type = NODE_BINARY_OPERATION;
            is_boolean_expression = true;
            break;
        default:
            error_exit(ERR_SYNTAX, "Unknown operator type.");
            break;
        }

        const char *operator_name = current_token.lexeme;

        current_token = get_next_token(scanner); // Пропускаем оператор

        ASTNode *right_node = parse_primary_expression(scanner);

        // Создаем узел для бинарной операции
        left_node = create_binary_operation_node(operator_name, left_node, right_node);

        // Обновляем тип выражения
        if (is_boolean_expression)
        {
            left_node->data_type = TYPE_BOOL;
        }
        else if (left_node->left->data_type != right_node->data_type)
        {
            if (!type_convertion(left_node))
                error_exit(ERR_SEMANTIC, "Conflicting types of expression, line: %d, column: %d", current_token.line, current_token.column);
        }
    }

    // Возвращаем узел AST итогового выражения
    return left_node;
}

// Parses a primary expression (literal, identifier, or parenthesized expression)
ASTNode *parse_primary_expression(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing primary expression. Current token: %d\n Line and column: %d %d\n", current_token.type, current_token.line, current_token.column);

    if (current_token.type == TOKEN_INT_LITERAL)
    {
        // Создаем узел для целочисленного литерала
        char *value = string_duplicate(current_token.lexeme);
        ASTNode *literal_node = create_literal_node(TYPE_INT, value);
        current_token = get_next_token(scanner);
        return literal_node;
    }
    else if (current_token.type == TOKEN_FLOAT_LITERAL)
    {
        // Создаем узел для литерала с плавающей точкой
        char *value = string_duplicate(current_token.lexeme);
        ASTNode *literal_node = create_literal_node(TYPE_FLOAT, value);
        current_token = get_next_token(scanner);
        return literal_node;
    }
    else if (current_token.type == TOKEN_STRING_LITERAL)
    {
        // Создаем узел для строкового литерала
        char *value = string_duplicate(current_token.lexeme);
        ASTNode *literal_node = create_literal_node(TYPE_STRING, value);
        current_token = get_next_token(scanner);
        return literal_node;
    }
    else if (current_token.type == TOKEN_IDENTIFIER)
    {
        // Проверяем, существует ли идентификатор в таблице символов
        char *identifier_name = string_duplicate(current_token.lexeme);
        Symbol *symbol = symtable_search(&symtable, identifier_name);
        DataType data_type = TYPE_UNKNOWN;

        if (symbol == NULL && !(is_builtin_function(identifier_name, scanner)))
        {
            error_exit(ERR_SEMANTIC, "Undefined variable or function. Got lexeme: %s", current_token.lexeme);
        }
        else if (is_builtin_function(identifier_name, scanner))
        {
            data_type = get_builtin_function_type(identifier_name);
            // Adding ifj. to the beginning of the identifier_name. Example - identifier was write, now it is ifj.write. Using identifier_name.
            // Добавляем "ifj." в начало identifier_name
            LOG("DEBUG_PARSER: Builtin function found: %s\n", identifier_name);
            char *new_identifier_name = (char *)malloc(strlen(identifier_name) + 5);
            if (new_identifier_name == NULL) {
                error_exit(ERR_INTERNAL, "Memory allocation failed");
            }

            strcpy(new_identifier_name, "ifj.");
            strcat(new_identifier_name, identifier_name);

            // Освобождаем старую память и переопределяем identifier_name
            free(identifier_name);
            identifier_name = new_identifier_name;

        }
        else
        {
            data_type = symbol->data_type;
        }

        LOG("DEBUG_PARSER: BEFORE Primary parsing got token type: %d\n", current_token.type);
        current_token = get_next_token(scanner);
        LOG("DEBUG_PARSER: Primary parsing got token type: %d\n", current_token.type);

        // Если это вызов функции
        if (current_token.type == TOKEN_LEFT_PAREN)
        {
            current_token = get_next_token(scanner); // Пропускаем '('

            ASTNode **arguments = NULL;
            int arg_count = 0;

            if (current_token.type != TOKEN_RIGHT_PAREN)
            {
                arguments = (ASTNode **)malloc(sizeof(ASTNode *));
                arguments[arg_count++] = parse_expression(scanner);

                while (current_token.type == TOKEN_COMMA)
                {
                    current_token = get_next_token(scanner);
                    arguments = (ASTNode **)realloc(arguments, (arg_count + 1) * sizeof(ASTNode *));
                    arguments[arg_count++] = parse_expression(scanner);
                }
            }
            expect_token(TOKEN_RIGHT_PAREN, scanner); // Ожидаем ')'

            // Создаем узел вызова функции
            ASTNode *func_call_node = create_function_call_node(identifier_name, arguments, arg_count);
            func_call_node->data_type = data_type; // Устанавливаем тип данных на основе встроенной функции или символа
            return func_call_node;
        }

        // Создаем узел идентификатора
        ASTNode *identifier_node = create_identifier_node(identifier_name);
        identifier_node->data_type = data_type; // Устанавливаем тип данных для идентификатора
        return identifier_node;
    }
    else if (current_token.type == TOKEN_LEFT_PAREN)
    {
        // Скобочное выражение
        current_token = get_next_token(scanner);        // Пропускаем '('
        ASTNode *expr_node = parse_expression(scanner); // Парсим выражение в скобках
        expect_token(TOKEN_RIGHT_PAREN, scanner);       // Ожидаем ')'
        return expr_node;
    }
    else
    {
        error_exit(ERR_SYNTAX, "Expected literal, identifier, or '(' for expression.");
        return NULL; // На случай ошибки, хотя сюда выполнение не дойдет
    }
}

// Parses a binary operation and returns the resulting type
ASTNode *parse_binary_operation(Scanner *scanner, ASTNode *left_node)
{
    LOG("DEBUG_PARSER: Parsing binary operation. Current token: %s\n Line and column: %d %d\n", current_token.lexeme, current_token.line, current_token.column);

    // Сохраняем тип оператора и создаем узел операции
    TokenType operator_type = current_token.type;
    NodeType op_node_type;

    switch (operator_type)
    {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_MULTIPLY:
    case TOKEN_DIVIDE:
    case TOKEN_LESS:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER:
    case TOKEN_GREATER_EQUAL:
    case TOKEN_EQUAL:
    case TOKEN_NOT_EQUAL:
        op_node_type = NODE_BINARY_OPERATION;
        break;
    default:
        error_exit(ERR_SYNTAX, "Unknown operator type.");
    }

    current_token = get_next_token(scanner); // Переходим к правой части выражения

    // Парсим правую часть выражения
    ASTNode *right_node = parse_primary_expression(scanner);

    // Проверка совместимости типов для арифметических операций
    if ((operator_type == TOKEN_PLUS || operator_type == TOKEN_MINUS ||
         operator_type == TOKEN_MULTIPLY || operator_type == TOKEN_DIVIDE) &&
        (right_node->data_type != TYPE_INT && right_node->data_type != TYPE_FLOAT))
    {

        error_exit(ERR_SEMANTIC, "Invalid operand type for arithmetic operation, line: %d, column: %d", current_token.line, current_token.column);
    }

    // Проверка и установка типа данных для логических операций
    if (operator_type == TOKEN_LESS || operator_type == TOKEN_LESS_EQUAL ||
        operator_type == TOKEN_GREATER || operator_type == TOKEN_GREATER_EQUAL ||
        operator_type == TOKEN_EQUAL || operator_type == TOKEN_NOT_EQUAL)
    {

        left_node->data_type = TYPE_BOOL; // Логические операции возвращают тип BOOL
    }
    else
    {
        // Устанавливаем тип данных для арифметических операций
        if (left_node->data_type != right_node->data_type)
        {
            error_exit(ERR_SEMANTIC, "Type mismatch in binary operation, line: %d, column: %d", current_token.line, current_token.column);
        }
        left_node->data_type = right_node->data_type;
    }

    // Создаем и возвращаем узел бинарной операции
    return create_binary_operation_node(NULL, left_node, right_node);
}

// Function to parse the import line at the beginning of the program
ASTNode *parse_import(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing import statement\n");

    // Ожидаем ключевое слово 'const'
    expect_token(TOKEN_CONST, scanner);

    // Ожидаем идентификатор 'ifj'
    if (current_token.type != TOKEN_IDENTIFIER || strcmp(current_token.lexeme, "ifj") != 0)
    {
        error_exit(ERR_SYNTAX, "Expected identifier 'ifj'.");
    }
    current_token = get_next_token(scanner);

    // Ожидаем '='
    expect_token(TOKEN_ASSIGN, scanner);

    // Ожидаем '@import'
    if (current_token.type != TOKEN_IMPORT)
    {
        error_exit(ERR_SYNTAX, "Expected '@import'.");
    }
    current_token = get_next_token(scanner);

    // Ожидаем '('
    expect_token(TOKEN_LEFT_PAREN, scanner);

    // Ожидаем строковый литерал "ifj24.zig"
    if (current_token.type != TOKEN_STRING_LITERAL || strcmp(current_token.lexeme, "ifj24.zig") != 0)
    {
        error_exit(ERR_SYNTAX, "Expected string literal \"ifj24.zig\". Got: %s", current_token.lexeme);
    }
    char *import_value = string_duplicate(current_token.lexeme);
    current_token = get_next_token(scanner);

    // Ожидаем ')'
    expect_token(TOKEN_RIGHT_PAREN, scanner);

    // Ожидаем ';'
    expect_token(TOKEN_SEMICOLON, scanner);

    LOG("DEBUG_PARSER: Finished parsing import statement\n");

    // Создаем узел AST для оператора импорта
    return create_literal_node(TYPE_STRING, import_value);
}

/*
A function that recursively traverses an entire AST and:
1. In case of void function looks for any return statement and checks if its type is different from void
2. In case of a function of a type other than void, it checks if all return statements match the given type of the function,
 and looks for a return statement in the main block of the function.
*/
void check_return_types(ASTNode *function_node, DataType return_type, int *block_layer)
{
    if (return_type == TYPE_VOID)
    {
        if (function_node != NULL)
        {
            if (function_node->type == NODE_RETURN)
            {
                if (function_node->data_type != TYPE_VOID)
                {
                    error_exit(ERR_SEMANTIC_RETURN, "Function VOID expects return(void)");
                }
            }
        }
        if (function_node->body != NULL)
        {
            check_return_types(function_node->body, return_type, block_layer);
        }
        if (function_node->left != NULL)
        {
            check_return_types(function_node->left, return_type, block_layer);
        }
        if (function_node->next != NULL)
        {
            check_return_types(function_node->next, return_type, block_layer);
        }
        return;
    }
    else
    {
        if (function_node == NULL && *block_layer == 0)
        {
            error_exit(ERR_SEMANTIC_RETURN, "Missing return statement");
        }
        if (function_node->type == NODE_RETURN)
        {
            if (function_node->data_type != return_type)
            {
                error_exit(ERR_SEMANTIC_PARAMS, "Incompatible types of return statement");
            }
            if (*block_layer == 0) // Здесь проверяем значение на 0
            {
                (*block_layer)--; // Корректно уменьшаем значение
            }
            return;
        }
        if (function_node->body != NULL)
        {
            (*block_layer)++;
            check_return_types(function_node->body, return_type, block_layer);
            (*block_layer)--;
        }
        if (function_node->left != NULL)
        {
            (*block_layer)++;
            check_return_types(function_node->left, return_type, block_layer);
            (*block_layer)--;
        }
        if (function_node->next != NULL)
        {
            check_return_types(function_node->next, return_type, block_layer);
        }
        if (*block_layer == 0) // Здесь используем разыменование указателя
        {
            error_exit(ERR_SEMANTIC_RETURN, "Missing return statement");
        }
    }
}

bool type_convertion(ASTNode *main_node)
{
    if (main_node->left->data_type == TYPE_FLOAT && main_node->right->data_type == TYPE_INT && main_node->right->type == NODE_LITERAL)
    {
        add_decimal(main_node->right->value);
        main_node->right->data_type = TYPE_FLOAT;
        main_node->data_type = TYPE_FLOAT;
        return true;
    }
    else if (main_node->left->data_type == TYPE_INT && main_node->right->data_type == TYPE_FLOAT && main_node->left->type == NODE_LITERAL)
    {
        add_decimal(main_node->left->value);
        main_node->left->data_type = TYPE_FLOAT;
        main_node->data_type = TYPE_FLOAT;
        return true;
    }

    return false;
    /*
    else if(ma)
    remove_decimal(main_node->right->value);
    remove_decimal(main_node->left->value);*/
}


// Function to expect a specific token type
static void expect_token(TokenType expected_type, Scanner *scanner)
{
    LOG("DEBUG_PARSER: Expected token: %d, got token: %d\nLine and column: %d %d\n", expected_type, current_token.type, current_token.line, current_token.column);
    if (current_token.type != expected_type)
    {
        error_exit(ERR_SYNTAX, "Unexpected token.");
    }
    current_token = get_next_token(scanner);
}