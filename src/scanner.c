/*
 * Scanner for IFJ24 language
 * Responsible for lexical analysis of the input source code.
 */

#include "scanner.h"
#include "error.h"
#include "tokens.h"
#include "utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


#ifdef DEBUG_SCANNER
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif

// Maximum length for a lexeme
#define MAX_LEXEME_LENGTH 256

// Scanner state and current character storage

// Function prototypes
static void skip_whitespace_and_comments(Scanner *scanner);
static Token scan_identifier_or_keyword(Scanner *scanner);
static Token scan_number(Scanner *scanner);
static Token scan_string(Scanner *scanner);
static Token get_operator_or_delimiter(Scanner *scanner);
static Token get_next_token_internal(Scanner *scanner);

void print_token(Token token)
{
    if (token.lexeme == NULL)
    {
        LOG("DEBUG_SCANNER: Token type: %d, lexeme: (null), line: %d, column: %d\n",
               token.type, token.line, token.column);
    }
    else
    {
        LOG("DEBUG_SCANNER: Token type: %d, lexeme: %s, line: %d, column: %d\n",
               token.type, token.lexeme, token.line, token.column);
    }
}

// Helper function to skip whitespace and comments
static void skip_whitespace_and_comments(Scanner *scanner)
{
    while (1)
    {
        // Skip whitespace
        while (isspace(scanner->current_char))
        {
            LOG("DEBUG_SCANNER: Skipping whitespace: %c\n", scanner->current_char);
            if (scanner->current_char == '\n')
            {
                scanner->line++;
                scanner->column = 0;
            }
            else
            {
                scanner->column++;
            }
            scanner->current_char = fgetc(scanner->input);
        }

        // Check for comments
        if (scanner->current_char == '/')
        {
            char next_char = fgetc(scanner->input);
            if (next_char == '/')
            { // Line comment
                // Skip until end of line or EOF
                while (scanner->current_char != '\n' && scanner->current_char != EOF)
                {
                    scanner->current_char = fgetc(scanner->input);
                    scanner->column++;
                }
                // Continue to skip whitespace and comments
                continue;
            }
            else
            {
                // Not a comment, put back the character
                ungetc(next_char, scanner->input);
                break;
            }
        }
        else
        {
            break;
        }
    }
}

// Function to recognize keywords and identifiers
static Token recognize_keyword_or_identifier(char *lexeme, Scanner *scanner)
{
    LOG("DEBUG_SCANNER: Lexeme in the start recognize is: %s\n", lexeme);
    Token token;
    LOG("DEBUG_SCANNER: token.Lexeme in the start recognize is: %s\n", token.lexeme);
    token.lexeme = string_duplicate(lexeme);
    if (token.lexeme == NULL)
    {
        error_exit(ERR_INTERNAL, "Memory allocation failed for token lexeme.");
    }
    LOG("DEBUG_SCANNER: Lexeme in the middle recognize is: %s\n", lexeme);
    LOG("DEBUG_SCANNER: token.Lexeme in the middle recognize is: %s\n", token.lexeme);
    token.line = scanner->line;
    token.column = scanner->column - strlen(lexeme);

    LOG("DEBUG_SCANNER: Recognizing identifier or keyword: %s\n", lexeme);

    if (strcmp(lexeme, "const") == 0)
        token.type = TOKEN_CONST;
    else if (strcmp(lexeme, "var") == 0)
        token.type = TOKEN_VAR;
    else if (strcmp(lexeme, "if") == 0)
        token.type = TOKEN_IF;
    else if (strcmp(lexeme, "else") == 0)
        token.type = TOKEN_ELSE;
    else if (strcmp(lexeme, "while") == 0)
        token.type = TOKEN_WHILE;
    else if (strcmp(lexeme, "return") == 0)
        token.type = TOKEN_RETURN;
    else if (strcmp(lexeme, "fn") == 0)
        token.type = TOKEN_FN;
    else if (strcmp(lexeme, "pub") == 0)
    {
        LOG("DEBUG_SCANNER: Recognized 'pub' keyword\n");
        token.type = TOKEN_PUB;
    }
    else if (strcmp(lexeme, "void") == 0)
        token.type = TOKEN_VOID;
    else if (strcmp(lexeme, "null") == 0)
        token.type = TOKEN_NULL;
    else if (strcmp(lexeme, "i32") == 0)
        token.type = TOKEN_I32;
    else if (strcmp(lexeme, "f64") == 0)
        token.type = TOKEN_F64;
    else if (strcmp(lexeme, "[]u8") == 0)
        token.type = TOKEN_U8;
    else if (strcmp(lexeme, "@import") == 0)
        token.type = TOKEN_IMPORT;
    else
    {
        token.type = TOKEN_IDENTIFIER;
        LOG("DEBUG_SCANNER: Recognized as identifier\n");
    }
    LOG("DEBUG_SCANNER: Lexeme in the end recognize is: %s\n", token.lexeme);
    return token;
}

static Token scan_identifier_or_keyword(Scanner *scanner)
{
    char lexeme_buffer[MAX_LEXEME_LENGTH];
    int index = 0;

    LOG("DEBUG_SCANNER: Starting to scan identifier or keyword\n");
    // Разрешаем буквы, цифры, '_', '@' и '[' ']'
    while (isalnum(scanner->current_char) || scanner->current_char == '_' || scanner->current_char == '@' || scanner->current_char == '[' 
           || scanner->current_char == ']')
    {
        // Проверка, что точка возможна только после ifj
        if (index < MAX_LEXEME_LENGTH - 1)
        {
            lexeme_buffer[index++] = scanner->current_char;
            LOG("DEBUG_SCANNER: Lexeme_Buffer is: %s\n", lexeme_buffer);
        }
        else
        {
            error_exit(ERR_LEXICAL, "Identifier too long.");
        }
        
        // Считываем следующий символ
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
    }

    // Проверка на пустоту буфера, что свидетельствует об ошибке
    if (index == 0)
    {
        error_exit(ERR_LEXICAL, "Lexeme buffer is empty.");
    }

    // Завершаем строку лексемы
    lexeme_buffer[index] = '\0';
    LOG("DEBUG_SCANNER: Finished scanning identifier: %s\n", lexeme_buffer);

    // Резолвим идентификатор или ключевое слово
    Token t = recognize_keyword_or_identifier(lexeme_buffer, scanner);
    LOG("DEBUG_SCANNER: Token type: %d, lexeme: %s, line: %d, column: %d\n",
           t.type, t.lexeme, t.line, t.column);
    
    return t;
}



// Function to scan numeric literals (int and float)
static Token scan_number(Scanner *scanner)
{
    char number_buffer[MAX_LEXEME_LENGTH];
    int index = 0;
    int is_float = 0;

    // Handle integer part
    while (isdigit(scanner->current_char))
    {
        if (index < MAX_LEXEME_LENGTH - 1)
        {
            number_buffer[index++] = scanner->current_char;
        }
        else
        {
            error_exit(ERR_LEXICAL, "Number literal too long.");
        }
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
    }

    // Handle decimal point (float) or exponent
    if (scanner->current_char == '.')
    {
        is_float = 1;
        number_buffer[index++] = scanner->current_char;
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;

        // At least one digit after decimal point
        if (!isdigit(scanner->current_char))
        {
            error_exit(ERR_LEXICAL, "Invalid float literal.");
        }

        while (isdigit(scanner->current_char))
        {
            if (index < MAX_LEXEME_LENGTH - 1)
            {
                number_buffer[index++] = scanner->current_char;
            }
            else
            {
                error_exit(ERR_LEXICAL, "Float literal too long.");
            }
            scanner->current_char = fgetc(scanner->input);
            scanner->column++;
        }
    }

    // Handle exponent part
    if (scanner->current_char == 'e' || scanner->current_char == 'E')
    {
        is_float = 1;
        number_buffer[index++] = scanner->current_char;
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;

        // Optional '+' or '-'
        if (scanner->current_char == '+' || scanner->current_char == '-')
        {
            if (index < MAX_LEXEME_LENGTH - 1)
            {
                number_buffer[index++] = scanner->current_char;
            }
            else
            {
                error_exit(ERR_LEXICAL, "Float literal too long in exponent.");
            }
            scanner->current_char = fgetc(scanner->input);
            scanner->column++;
        }

        // At least one digit in exponent
        if (!isdigit(scanner->current_char))
        {
            error_exit(ERR_LEXICAL, "Invalid float literal exponent.");
        }

        while (isdigit(scanner->current_char))
        {
            if (index < MAX_LEXEME_LENGTH - 1)
            {
                number_buffer[index++] = scanner->current_char;
            }
            else
            {
                error_exit(ERR_LEXICAL, "Float literal exponent too long.");
            }
            scanner->current_char = fgetc(scanner->input);
            scanner->column++;
        }
    }

    number_buffer[index] = '\0';

    // Validate integer literals: non-zero numbers should not start with '0'
    if (!is_float && strlen(number_buffer) > 1 && number_buffer[0] == '0')
    {
        error_exit(ERR_LEXICAL, "Invalid integer literal with leading zero.");
    }

    Token token;
    token.lexeme = string_duplicate(number_buffer);
    token.line = scanner->line;
    token.column = scanner->column - strlen(number_buffer);

    if (is_float)
    {
        token.type = TOKEN_FLOAT_LITERAL;
    }
    else
    {
        token.type = TOKEN_INT_LITERAL;
    }

    return token;
}

// Function to scan string literals
static Token scan_string(Scanner *scanner)
{
    char string_buffer[MAX_LEXEME_LENGTH];
    int index = 0;

    scanner->current_char = fgetc(scanner->input); // Skip the opening quote
    scanner->column++;

    while (scanner->current_char != '"' && scanner->current_char != EOF)
    {
        if (scanner->current_char == '\\')
        { // Handle escape sequences
            scanner->current_char = fgetc(scanner->input);
            scanner->column++;

            if (scanner->current_char == 'n')
            {
                if (index < MAX_LEXEME_LENGTH - 1)
                    string_buffer[index++] = '\n';
                else
                    error_exit(ERR_LEXICAL, "String literal too long.");
            }
            else if (scanner->current_char == 't')
            {
                if (index < MAX_LEXEME_LENGTH - 1)
                    string_buffer[index++] = '\t';
                else
                    error_exit(ERR_LEXICAL, "String literal too long.");
            }
            else if (scanner->current_char == 'r')
            {
                if (index < MAX_LEXEME_LENGTH - 1)
                    string_buffer[index++] = '\r';
                else
                    error_exit(ERR_LEXICAL, "String literal too long.");
            }
            else if (scanner->current_char == '"')
            {
                if (index < MAX_LEXEME_LENGTH - 1)
                    string_buffer[index++] = '"';
                else
                    error_exit(ERR_LEXICAL, "String literal too long.");
            }
            else if (scanner->current_char == '\\')
            {
                if (index < MAX_LEXEME_LENGTH - 1)
                    string_buffer[index++] = '\\';
                else
                    error_exit(ERR_LEXICAL, "String literal too long.");
            }
            else if (scanner->current_char == 'x')
            {
                // Обработка \xdd, где dd — две шестнадцатеричные цифры
                char hex_digits[3] = {0};
                scanner->current_char = fgetc(scanner->input);
                scanner->column++;

                for (int i = 0; i < 2; i++)
                {
                    if (!isxdigit(scanner->current_char))
                    {
                        error_exit(ERR_LEXICAL, "Invalid escape sequence in string literal.");
                    }
                    hex_digits[i] = scanner->current_char;
                    scanner->current_char = fgetc(scanner->input);
                    scanner->column++;
                }

                int char_code = (int)strtol(hex_digits, NULL, 16);
                if (index < MAX_LEXEME_LENGTH - 1)
                {
                    string_buffer[index++] = (char)char_code;
                }
                else
                {
                    error_exit(ERR_LEXICAL, "String literal too long.");
                }
            }
            else
            {
                // Invalid escape sequence
                error_exit(ERR_LEXICAL, "Invalid escape sequence in string literal.");
            }
        }
        else if (scanner->current_char == '\n')
        {
            // Strings cannot contain newlines
            error_exit(ERR_LEXICAL, "Unterminated string literal.");
        }
        else
        {
            // Regular character
            if (scanner->current_char < 32 || scanner->current_char == 35 || scanner->current_char == 92)
            { // ASCII > 32, not '#', not '\\'
                error_exit(ERR_LEXICAL, "Invalid character in string literal.");
            }
            if (index < MAX_LEXEME_LENGTH - 1)
            {
                string_buffer[index++] = scanner->current_char;
            }
            else
            {
                error_exit(ERR_LEXICAL, "String literal too long.");
            }
        }

        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
    }

    if (scanner->current_char != '"')
    {
        error_exit(ERR_LEXICAL, "Unterminated string literal.");
    }

    scanner->current_char = fgetc(scanner->input); // Skip the closing quote
    scanner->column++;

    string_buffer[index] = '\0';

    Token token;
    token.type = TOKEN_STRING_LITERAL;
    token.lexeme = string_duplicate(string_buffer);
    if (token.lexeme == NULL)
    {
        error_exit(ERR_INTERNAL, "Memory allocation failed for string literal.");
    }
    token.line = scanner->line;
    token.column = scanner->column - strlen(string_buffer) - 2; // Approximation

    return token;
}

// Function to get operator or delimiter tokens
static Token get_operator_or_delimiter(Scanner *scanner)
{
    Token token;
    token.line = scanner->line;
    token.column = scanner->column;

    // Allocate memory for lexeme
    token.lexeme = malloc(2);
    if (token.lexeme == NULL)
    {
        error_exit(ERR_INTERNAL, "Memory allocation failed for token lexeme.");
    }
    token.lexeme = malloc(2); // Выделяем память для одного символа + '\0'
    token.line = scanner->line;
    token.column = scanner->column;

    switch (scanner->current_char)
    {
    case '+':
        token.type = TOKEN_PLUS;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case '-':
        token.type = TOKEN_MINUS;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case '*':
        token.type = TOKEN_MULTIPLY;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case '/':
        token.type = TOKEN_DIVIDE;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case '=':
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        if (scanner->current_char == '=')
        {
            token.type = TOKEN_EQUAL;
            char *temp = realloc(token.lexeme, 3);
            if (temp == NULL)
            {
                free(token.lexeme); // Освобождаем старую память
                error_exit(ERR_INTERNAL, "Memory reallocation failed.");
            }
            token.lexeme = temp;
            token.lexeme[0] = '=';
            token.lexeme[1] = '=';
            token.lexeme[2] = '\0';
            scanner->current_char = fgetc(scanner->input);
            scanner->column++;
        }
        else
        {
            token.type = TOKEN_ASSIGN;
            token.lexeme[0] = '=';
            token.lexeme[1] = '\0';
        }
        break;
    case '(':
        token.type = TOKEN_LEFT_PAREN;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case ')':
        token.type = TOKEN_RIGHT_PAREN;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case '{':
        token.type = TOKEN_LEFT_BRACE;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case '}':
        token.type = TOKEN_RIGHT_BRACE;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case '|':
        token.type = TOKEN_PIPE;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case ':': // Добавляем поддержку двоеточия
        token.type = TOKEN_COLON;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case ';': // Добавляем поддержку semicolon
        token.type = TOKEN_SEMICOLON;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case '<':
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        if (scanner->current_char == '=')
        {
            token.type = TOKEN_LESS_EQUAL;
            char *temp = realloc(token.lexeme, 3);
            if (temp == NULL)
            {
                free(token.lexeme); // Освобождаем старую память
                error_exit(ERR_INTERNAL, "Memory reallocation failed.");
            }
            token.lexeme = temp;

            strcpy(token.lexeme, "<=");
            scanner->current_char = fgetc(scanner->input);
            scanner->column++;
        }
        else
        {
            token.type = TOKEN_LESS;
            token.lexeme[0] = '<';
            token.lexeme[1] = '\0';
        }
        break;
    case '>':
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        if (scanner->current_char == '=')
        {
            token.type = TOKEN_GREATER_EQUAL;
            char *temp = realloc(token.lexeme, 3);
            if (temp == NULL)
            {
                free(token.lexeme); // Освобождаем старую память
                error_exit(ERR_INTERNAL, "Memory reallocation failed.");
            }
            token.lexeme = temp;

            strcpy(token.lexeme, ">=");
            scanner->current_char = fgetc(scanner->input);
            scanner->column++;
        }
        else
        {
            token.type = TOKEN_GREATER;
            token.lexeme[0] = '>';
            token.lexeme[1] = '\0';
        }
        break;
    case ',': // Добавляем поддержку запятой
        token.type = TOKEN_COMMA;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case '.':
        token.type = TOKEN_DOT;
        token.lexeme[0] = scanner->current_char;
        token.lexeme[1] = '\0';
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        break;
    case '!':
        scanner->current_char = fgetc(scanner->input);
        scanner->column++;
        if (scanner->current_char == '=')
        {
            token.type = TOKEN_NOT_EQUAL;
            char *temp = realloc(token.lexeme, 3);
            if (temp == NULL)
            {
                free(token.lexeme); // Освобождаем старую память
                error_exit(ERR_INTERNAL, "Memory reallocation failed.");
            }
            token.lexeme = temp;

            token.lexeme[0] = '!';
            token.lexeme[1] = '=';
            token.lexeme[2] = '\0';
            scanner->current_char = fgetc(scanner->input);
            scanner->column++;
        }
        else
        {
            error_exit(ERR_LEXICAL, "Unknown operator '!' without '='.");
        }
        break;
    default:
        free(token.lexeme);
        error_exit(ERR_LEXICAL, "Unknown character encountered. character: %c", scanner->current_char);
        break;
    }

    return token;
}

static Token get_next_token_internal(Scanner *scanner)
{
    skip_whitespace_and_comments(scanner);

    LOG("DEBUG_SCANNER: Processing character: '%c' (ASCII: %d)\n", scanner->current_char, (int)scanner->current_char);

    Token token;
    token.lexeme = NULL;
    token.line = scanner->line;
    token.column = scanner->column;

    if (scanner->current_char == EOF)
    {
        LOG("DEBUG_SCANNER: Reached EOF\n");
        token.type = TOKEN_EOF;
        token.lexeme = string_duplicate("EOF");
        LOG("DEBUG_SCANNER: Returning EOF token\n");
        return token;
    }

    if (isalpha(scanner->current_char) || scanner->current_char == '_' || scanner->current_char == '@'
        || scanner->current_char == '[' || scanner->current_char == ']')
    {
        LOG("DEBUG_SCANNER: Detected identifier or keyword\n");
        Token t = scan_identifier_or_keyword(scanner);
        print_token(t); // Отладочная печать токена
        return t;
    }
    else if (isdigit(scanner->current_char))
    {
        LOG("DEBUG_SCANNER: Detected number\n");
        Token t = scan_number(scanner);
        print_token(t); // Отладочная печать токена
        return t;
    }
    else if (scanner->current_char == '"')
    {
        LOG("DEBUG_SCANNER: Detected string\n");
        Token t = scan_string(scanner);
        print_token(t); // Отладочная печать токена
        return t;
    }
    else
    {
        LOG("DEBUG_SCANNER: Detected operator or delimiter\n");
        Token t = get_operator_or_delimiter(scanner);
        print_token(t); // Отладочная печать токена
        return t;
    }
}

// Public function to get the next token
Token get_next_token(Scanner *scanner)
{
    return get_next_token_internal(scanner);
}

// Function to initialize scanner
void scanner_init(FILE *input_file, Scanner *scanner)
{
    scanner->input = input_file;
    scanner->current_char = fgetc(scanner->input);
    scanner->column = 1;
    scanner->line = 1;
    LOG("DEBUG_SCANNER: Initial character: '%c' (ASCII: %d)\n", scanner->current_char, scanner->current_char); // Отладка
    while ((scanner->current_char = fgetc(scanner->input)) != EOF)
    {
        LOG("DEBUG_SCANNER: Reading character: '%c' (ASCII: %d)\n", scanner->current_char, scanner->current_char);
    }

    // Возвращаемся к началу файла, чтобы начать нормальный процесс анализа
    rewind(scanner->input);
    scanner->current_char = fgetc(scanner->input);
}

// Function to free token lexeme
void free_token(Token *token)
{
    if (token->lexeme != NULL)
    {
        free(token->lexeme);
        token->lexeme = NULL;
    }
}

void scanner_free(Scanner *scanner)
{
    // Если в будущем вы добавите динамические ресурсы, освобождайте их здесь
}