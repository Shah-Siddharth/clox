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

static bool isAtEnd()
{
    return *scanner.current == '\0';
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

static Token createErrorToken()
{
    // TODO: add logic to include error msg.
    Token token;
    token.type = TOKEN_ERROR;
    return token;
}

/*
each time this function is called, a complete token is scanned.
so, whenever function is executed, the scanner is always at the beginning of a new token.
thus, scanner.start can point to current character so we can remember the start of the token
*/
Token scanToken()
{
    scanner.start = scanner.current;

    if (isAtEnd())
        return createToken(TOKEN_EOF);

    // TODO: add code for different token cases
    return createErrorToken("Unexpected character.");
}