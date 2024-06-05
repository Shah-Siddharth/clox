#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct
{
    const char *start;   // beginning of the lexeme being scanned
    const char *current; // current character
    int line;
} Scanner;

Scanner scanner;

void initScanner(const char *sourceCode)
{
    scanner.start = sourceCode;
    scanner.current = sourceCode;
    scanner.line = 1;
}

static char advance()
{
    scanner.current++;
    return scanner.current[-1];
}

static bool isAtEnd()
{
    return *scanner.current == '\0';
}

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool match(char expected)
{
    if (isAtEnd())
        return false;

    if (*scanner.current != expected)
        return false;

    scanner.current++;
    return true;
}

static char peek()
{
    return *scanner.current;
}

static char peekNext()
{
    if (isAtEnd())
        return '\0';
    return scanner.current[1];
}

// advance the scanner past any leading whitespace or comments
static void skipWhiteSpace()
{
    for (;;)
    {
        char c = peek();
        switch (c)
        {
        case ' ':
        case '\r':
        case '\t':
            advance();
            break;
        case '\n':
            scanner.line++;
            advance();
            break;
        case '/':
            if (peekNext() == '/')
            {
                // A comment goes until the end of the line
                while (peek() != '\n' && !isAtEnd())
                    advance();
            }
            else
                return;
            break;
        default:
            return;
        }
    }
}

static Token createToken(TokenType type)
{
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

static Token createErrorToken(const char *message)
{
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

static Token createNumberToken()
{
    while (isDigit(peek()))
        advance();

    // see if there is a fractional part
    if (peek() == '.' && isDigit(peekNext()))
    {
        // consume the '.'
        advance();

        while (isDigit(peek()))
            advance();
    }

    return createToken(TOKEN_NUMBER);
}

static Token createStringToken()
{
    while (peek() != '"' && !isAtEnd())
    {
        if (peek() == '\n')
            scanner.line++;
        advance();
    }

    if (isAtEnd())
        return createErrorToken("Unterminated string.");

    advance();
    return createToken(TOKEN_STRING);
}

/*
each time this function is called, a complete token is scanned.
so, whenever function is executed, the scanner is always at the beginning of a new token.
thus, scanner.start can point to current character so we can remember the start of the token
*/
Token scanToken()
{
    skipWhiteSpace();

    scanner.start = scanner.current;

    if (isAtEnd())
        return createToken(TOKEN_EOF);

    char c = advance();
    if (isDigit(c))
        return createNumberToken();

    switch (c)
    {

    // single character tokens
    case '(':
        return createToken(TOKEN_LEFT_PAREN);
    case ')':
        return createToken(TOKEN_RIGHT_PAREN);
    case '{':
        return createToken(TOKEN_LEFT_BRACE);
    case '}':
        return createToken(TOKEN_RIGHT_BRACE);
    case ';':
        return createToken(TOKEN_SEMICOLON);
    case ',':
        return createToken(TOKEN_COMMA);
    case '.':
        return createToken(TOKEN_DOT);
    case '-':
        return createToken(TOKEN_MINUS);
    case '+':
        return createToken(TOKEN_PLUS);
    case '/':
        return createToken(TOKEN_SLASH);
    case '*':
        return createToken(TOKEN_STAR);

        // single or double character tokens
    case '!':
        return createToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
        return createToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<':
        return createToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>':
        return createToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);

    // strings
    case '"':
        return createStringToken();
    }
    return createErrorToken("Unexpected character.");
}