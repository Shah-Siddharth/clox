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