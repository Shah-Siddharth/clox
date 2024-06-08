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

static bool isAlpha(char c)
{
    return ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_');
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

static TokenType checkKeyword(int start, int length, const char *rest, TokenType type)
{
    // check if identifier is same length as keyword & if it matches the keyword name
    if ((scanner.current - scanner.start == start + length) &&
        memcmp(scanner.start + start, rest, length) == 0)
        return type;

    return TOKEN_IDENTIFIER;
}

/*
function to check if the identifier is any of the reserved keywords
uses a trie like logic to check character by character if the identifier is a keyword
*/
static TokenType getIdentifierType()
{
    switch (scanner.start[0])
    {
    case 'a':
        return checkKeyword(1, 2, "nd", TOKEN_AND);
    case 'c':
        return checkKeyword(1, 4, "lass", TOKEN_CLASS);
    case 'e':
        return checkKeyword(1, 3, "lse", TOKEN_ELSE);
    case 'f':
        if (scanner.current - scanner.start > 1) // check if there are more chars in the identifier
        {
            switch (scanner.start[1])
            {
            case 'a':
                return checkKeyword(2, 3, "lse", TOKEN_FALSE);
            case 'o':
                return checkKeyword(2, 1, "r", TOKEN_FOR);
            case 'u':
                return checkKeyword(2, 1, "n", TOKEN_FUN);
            }
        }
    case 'i':
        return checkKeyword(1, 1, "f", TOKEN_IF);
    case 'n':
        return checkKeyword(1, 2, "il", TOKEN_NIL);
    case 'o':
        return checkKeyword(1, 1, "r", TOKEN_OR);
    case 'p':
        return checkKeyword(1, 4, "rint", TOKEN_PRINT);
    case 'r':
        return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
    case 's':
        return checkKeyword(1, 4, "uper", TOKEN_SUPER);
    case 't':
        if (scanner.current - scanner.start > 1)
        {
            switch (scanner.start[1])
            {
            case 'h':
                return checkKeyword(2, 2, "is", TOKEN_THIS);
            case 'r':
                return checkKeyword(2, 2, "ue", TOKEN_TRUE);
            }
        }
    case 'v':
        return checkKeyword(1, 2, "ar", TOKEN_VAR);
    case 'w':
        return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
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

static Token createIdentifierToken()
{
    while (isAlpha(peek()) || isDigit(peek()))
        advance();
    return createToken(getIdentifierType());
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
    if (isAlpha(c))
        return createIdentifierToken();
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