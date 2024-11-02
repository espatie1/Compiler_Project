#include "parser.h"
#include "error.h"
#include "scanner.h"
#include "string.h"
#include "symtable.h"
#include "tokens.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG_PARSER
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

// Forward declarations of helper functions
static void expect_token(TokenType expected_type, Scanner *scanner);
static void parse_block(Scanner *scanner);
static void parse_variable_declaration(Scanner *scanner);
static void parse_variable_assigning(Scanner *scanner);
static void parse_if_statement(Scanner *scanner);
static void parse_while_statement(Scanner *scanner);
static void parse_return_statement(Scanner *scanner);
static DataType parse_primary_expression(Scanner *scanner);
static DataType parse_binary_operation(Scanner *scanner);
DataType parse_type(Scanner *scanner);        // Объявляем функцию заранее
DataType parse_return_type(Scanner *scanner); // Объявляем функцию заранее

// Global token storage
static Token current_token;
static SymTable symtable; // Global symbol table for the program

void parser_init(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Initializing parser...\n");
    // Initialize the symbol table
    symtable_init(&symtable);
    // Get the first token to start parsing
    current_token = get_next_token(scanner);
}

// Function to start parsing the program
void parse_program(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing program\n");
    while (current_token.type != TOKEN_EOF)
    {
        // print_token(current_token); // Debugging token output
        if (current_token.type == TOKEN_PUB)
        {
            parse_function(scanner);
        }
        else
        {
            error_exit(ERR_SYNTAX, "Expected function definition.");
        }
    }
}

// Function to parse a function definition
// Function to parse a function definition
int parse_function(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing function\n");
    expect_token(TOKEN_PUB, scanner); // Ожидаем ключевое слово 'pub'
    expect_token(TOKEN_FN, scanner);  // Ожидаем ключевое слово 'fn'

    if (current_token.type != TOKEN_IDENTIFIER)
    {
        error_exit(ERR_SYNTAX, "Expected function name.");
    }

    if (current_token.lexeme == NULL)
    {
        error_exit(ERR_INTERNAL, "Lexeme is NULL before strdup.");
    }
    char *function_name = string_duplicate(current_token.lexeme); // strdup(current_token.lexeme);
    if (function_name == NULL)
    {
        error_exit(ERR_INTERNAL, "Memory allocation failed for function name.");
    }
    current_token = get_next_token(scanner);

    expect_token(TOKEN_LEFT_PAREN, scanner); // '('

    // Parse function parameters
    if (current_token.type != TOKEN_RIGHT_PAREN)
    {
        parse_parameter(scanner); // Разбираем параметры функции
        while (current_token.type == TOKEN_COMMA)
        {
            current_token = get_next_token(scanner);
            parse_parameter(scanner);
        }
    }
    expect_token(TOKEN_RIGHT_PAREN, scanner); // ')'

    // Ожидаем возвращаемый тип
    DataType return_type = parse_return_type(scanner); // Функция, разбирающая возвращаемый тип

    // Добавляем функцию в таблицу символов
    Symbol *function_symbol = symtable_search(&symtable, function_name);
    if (function_symbol != NULL)
    {
        error_exit(ERR_SEMANTIC, "Function already defined.");
    }
    Symbol *new_function = (Symbol *)malloc(sizeof(Symbol)); // Выделяем память для нового символа
    if (new_function == NULL)
    {
        // Обработка ошибки выделения памяти
        error_exit(ERR_INTERNAL, "Memory allocation failed for new function symbol");
    }

    // Копируем строку имени функции
    new_function->name = string_duplicate(function_name); // Используйте strdup или свою реализацию string_duplicate
    if (new_function->name == NULL)
    {
        free(new_function); // Если копирование имени не удалось, освобождаем символ
        error_exit(ERR_INTERNAL, "Memory allocation failed for function name");
    }

    new_function->symbol_type = SYMBOL_FUNCTION;
    new_function->data_type = return_type;
    new_function->is_defined = true;
    new_function->is_used = false;
    new_function->next = NULL; // Инициализируем указатель на следующий элемент

    // Вставляем новый символ в таблицу
    symtable_insert(&symtable, function_name, new_function);

    // Parse function body (block)
    parse_block(scanner);
    return 0;
}

void parse_parameter(Scanner *scanner)
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

    // Не обрабатываем запятую и не вызываем parse_parameter рекурсивно
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
    else if (current_token.type == TOKEN_VOID)
    {
        current_token = get_next_token(scanner); // Обновляем токен после разбора типа
        return TYPE_VOID;
    }
    else if (current_token.type == TOKEN_ASSIGN)
    {
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
static void parse_block(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing block\n");
    expect_token(TOKEN_LEFT_BRACE, scanner); // Ожидаем '{'

    // Парсим операторы в теле функции до тех пор, пока не встретим '}'
    while (current_token.type != TOKEN_RIGHT_BRACE)
    {
        parse_statement(scanner);
    }

    // После завершения тела функции ожидаем закрывающую скобку '}'
    expect_token(TOKEN_RIGHT_BRACE, scanner);
}

// Function to parse a statement
void parse_statement(Scanner *scanner)
{
    if (current_token.type == TOKEN_VAR || current_token.type == TOKEN_CONST)
    {
        parse_variable_declaration(scanner);
    }
    else if (current_token.type == TOKEN_IF)
    {
        parse_if_statement(scanner);
    }
    else if (current_token.type == TOKEN_WHILE)
    {
        parse_while_statement(scanner);
    }
    else if (current_token.type == TOKEN_RETURN)
    {
        parse_return_statement(scanner); // Обрабатываем return
    }
    else if (current_token.type == TOKEN_IDENTIFIER)
    {
        // Parse as an expression statement
        parse_variable_assigning(scanner);
    }
}

void parse_variable_assigning(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing variable assigning\n");
    Symbol *symbol = symtable_search(&symtable, current_token.lexeme);
    if (symbol == NULL)
    {
        error_exit(ERR_SYNTAX, "Variable is not defined.");
    }
    current_token = get_next_token(scanner);
    expect_token(TOKEN_ASSIGN, scanner);
    parse_expression(scanner);

    //current_token = get_next_token(scanner);
    expect_token(TOKEN_SEMICOLON, scanner);
}

// Function to parse a variable declaration
void parse_variable_declaration(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing variable declaration\n");
    TokenType var_type = current_token.type; // 'var' or 'const'
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

    if (current_token.type == TOKEN_COLON)
    {
        current_token = get_next_token(scanner);
        declaration_type = parse_type(scanner);
    }
    expect_token(TOKEN_ASSIGN, scanner); // '='

    // Parse the expression being assigned
    DataType expr_type = parse_expression(scanner);

    if (declaration_type != TYPE_UNKNOWN && expr_type != declaration_type)
    {
        error_exit(ERR_SEMANTIC, "Declarated type of variable is not matching.");
    }
    else if (declaration_type == TYPE_UNKNOWN)
    {
        declaration_type = expr_type;
    }

    // Semantically check if the types are compatible
    if (var_type == TOKEN_VAR && expr_type == TYPE_UNKNOWN) // че бля?
    {
        error_exit(ERR_SEMANTIC, "Unknown data type assignment.");
    }

    expect_token(TOKEN_SEMICOLON, scanner); // Variable declaration ends with a semicolon

    // Add the variable to the symbol table
    Symbol *symbol = symtable_search(&symtable, variable_name);
    if (symbol != NULL)
    {
        error_exit(ERR_SEMANTIC, "Variable already defined.");
    }

    Symbol *new_var = (Symbol *)malloc(sizeof(Symbol));
    if (new_var == NULL)
    {
        free(new_var);
        error_exit(ERR_INTERNAL, "Memory allocation failed for parameter symbol.");
    }
    new_var->name = variable_name;
    new_var->symbol_type = SYMBOL_VARIABLE;
    new_var->data_type = expr_type;
    new_var->is_defined = true;
    new_var->next = NULL;

    symtable_insert(&symtable, variable_name, new_var);
}

// Function to parse an if statement
void parse_if_statement(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing if statement\n");
    expect_token(TOKEN_IF, scanner);         // 'if'
    expect_token(TOKEN_LEFT_PAREN, scanner); // '('

    DataType cond_type = parse_expression(scanner); // Condition expression

    // Semantic check for boolean condition
    if (cond_type != TYPE_BOOL)
    {
        error_exit(ERR_SEMANTIC, "Condition in if statement must be boolean.");
    }

    expect_token(TOKEN_RIGHT_PAREN, scanner); // ')'

    parse_block(scanner); // 'if' body

    // Optional 'else' part
    if (current_token.type == TOKEN_ELSE)
    {
        current_token = get_next_token(scanner);
        parse_block(scanner); // 'else' body
    }
}

// Function to parse a while statement
void parse_while_statement(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing while statement\n");
    expect_token(TOKEN_WHILE, scanner);      // 'while'
    expect_token(TOKEN_LEFT_PAREN, scanner); // '('

    DataType cond_type = parse_expression(scanner); // Condition expression

    // Semantic check for boolean condition
    if (cond_type != TYPE_BOOL)
    {
        error_exit(ERR_SEMANTIC, "Condition in while statement must be boolean.");
    }

    expect_token(TOKEN_RIGHT_PAREN, scanner); // ')'

    parse_block(scanner); // 'while' body
}

// Function to parse a return statement
// Function to parse a return statement
void parse_return_statement(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing return statement\n"); // Debugging output
    expect_token(TOKEN_RETURN, scanner);

    if (current_token.type != TOKEN_SEMICOLON)
    {
        parse_expression(scanner);
    }

    expect_token(TOKEN_SEMICOLON, scanner);
    LOG("DEBUG_PARSER: Finished parsing return statement\n"); // Debugging output
}

// Function to parse an expression and return its data type
DataType parse_expression(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing expression\n");

    DataType expression_type = parse_primary_expression(scanner); // Parse the first operand or literal


    if(current_token.type != TOKEN_SEMICOLON && current_token.type != TOKEN_RIGHT_PAREN)
    {
    current_token = get_next_token(scanner);
    }
    // If there is a binary operation, check its type
    if (current_token.type == TOKEN_PLUS || current_token.type == TOKEN_MINUS ||
        current_token.type == TOKEN_MULTIPLY || current_token.type == TOKEN_DIVIDE)
    {
        DataType binary_operation_type = parse_binary_operation(scanner);
        if (expression_type != binary_operation_type)
        {
            error_exit(ERR_SEMANTIC, "Conflicting types of expression");
        }
    }
    if (current_token.type != TOKEN_SEMICOLON && current_token.type != TOKEN_RIGHT_PAREN)
    {
        DataType next_expression_type = parse_expression(scanner);
        if (next_expression_type != expression_type)
        {
            error_exit(ERR_SEMANTIC, "Conflicting types of expression");
        }
    }
    return expression_type; // Default case, if no specific type was identified
}

// Parses a primary expression (literal, identifier, or parenthesized expression)
static DataType parse_primary_expression(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing parsing primary expression\n");
    if (current_token.type == TOKEN_INT_LITERAL)
    {
        // current_token = get_next_token(scanner);
        return TYPE_INT;
    }
    else if (current_token.type == TOKEN_FLOAT_LITERAL)
    {
        // current_token = get_next_token(scanner);
        return TYPE_FLOAT;
    }
    else if (current_token.type == TOKEN_STRING_LITERAL)
    {
        // current_token = get_next_token(scanner);
        return TYPE_STRING;
    }
    else if (current_token.type == TOKEN_IDENTIFIER)
    {
        // Check if the identifier is in the symbol table
        char *identifier_name = string_duplicate(current_token.lexeme); // Почему не string_duplicate?
        Symbol *symbol = symtable_search(&symtable, identifier_name);
        if (symbol == NULL)
        {
            error_exit(ERR_SEMANTIC, "Undefined variable or function.");
        }

        current_token = get_next_token(scanner);

        // If it's a function call, handle it
        if (current_token.type == TOKEN_LEFT_PAREN)
        {
            current_token = get_next_token(scanner); // Skip '('
            if (current_token.type != TOKEN_RIGHT_PAREN)
            {
                // Parse arguments if any
                parse_expression(scanner);
                while (current_token.type == TOKEN_COMMA)
                {
                    current_token = get_next_token(scanner);
                    parse_expression(scanner);
                }
            }
            expect_token(TOKEN_RIGHT_PAREN, scanner); // Expect ')'
        }

        return symbol->data_type; // Return the type of the variable or function
    }
    else if (current_token.type == TOKEN_LEFT_PAREN)
    {
        // Parenthesized expression
        current_token = get_next_token(scanner);        // Skip '('
        DataType expr_type = parse_expression(scanner); // Parse the expression inside parentheses
        expect_token(TOKEN_RIGHT_PAREN, scanner);       // Expect ')'
        return expr_type;
    }
    else 
    {
        error_exit(ERR_SYNTAX, "Expected literal, identifier, or '(' for expression.");
        return TYPE_UNKNOWN;
    }
}

// Parses a binary operation and returns the resulting type
static DataType parse_binary_operation(Scanner *scanner)
{
    LOG("DEBUG_PARSER: Parsing binary operation\n");
    TokenType operator_type = current_token.type;
    current_token = get_next_token(scanner); // Move to the next token after operator

    DataType rhs_type = parse_primary_expression(scanner); // Parse the right-hand side of the operation

    // Check the compatibility of types for binary operations
    if (operator_type == TOKEN_PLUS || operator_type == TOKEN_MINUS ||
        operator_type == TOKEN_MULTIPLY || operator_type == TOKEN_DIVIDE)
    {
        // For now, assuming operations between INT and FLOAT are allowed
        // current_token = get_next_token(scanner);
        return rhs_type;
    }

    return TYPE_UNKNOWN;
}

// Function to expect a specific token type
static void expect_token(TokenType expected_type, Scanner *scanner)
{
    LOG("DEBUG_PARSER: Expected token: %d, got token: %d\n", expected_type, current_token.type);
    if (current_token.type != expected_type)
    {
        error_exit(ERR_SYNTAX, "Unexpected token.");
    }
    current_token = get_next_token(scanner);
}
