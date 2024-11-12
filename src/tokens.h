#ifndef TOKENS_H
#define TOKENS_H

// Enumeration of all possible token types
typedef enum {
    // Keywords
    TOKEN_CONST,
    TOKEN_VAR,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_RETURN,
    TOKEN_FN,
    TOKEN_PUB,
    TOKEN_VOID,
    TOKEN_NULL,
    TOKEN_I32,
    TOKEN_F64,
    TOKEN_U8,
    TOKEN_IMPORT,

    // Identifiers and literals
    TOKEN_IDENTIFIER,
    TOKEN_INT_LITERAL,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING_LITERAL,

    // Operators
    TOKEN_PLUS,          // +
    TOKEN_MINUS,         // -
    TOKEN_MULTIPLY,      // *
    TOKEN_DIVIDE,        // /
    TOKEN_ASSIGN,        // =
    TOKEN_EQUAL,         // ==
    TOKEN_NOT_EQUAL,     // !=
    TOKEN_LESS,          // <
    TOKEN_GREATER,       // >
    TOKEN_LESS_EQUAL,    // <=
    TOKEN_GREATER_EQUAL, // >=

    // Delimiters
    TOKEN_LEFT_PAREN,    // (
    TOKEN_RIGHT_PAREN,   // )
    TOKEN_LEFT_BRACE,    // {
    TOKEN_RIGHT_BRACE,   // }
    TOKEN_COMMA,         // ,
    TOKEN_SEMICOLON,     // ;
    TOKEN_COLON,         // :
    TOKEN_LEFT_BRACKET,  // [
    TOKEN_RIGHT_BRACKET, // ]
    TOKEN_PIPE,          // |
    TOKEN_DOT,

    // Special tokens
    TOKEN_EOF,
    TOKEN_UNKNOWN
} TokenType;


typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

#endif // TOKENS_H
