#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct
{
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum
{
    // precedence levels from lowest to highest
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY
} Precedence;

// ParseFn is simply a function type that takes no args and returns nothing
typedef void (*ParseFn)();

typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;
Chunk *compilingChunk;

static Chunk *getCurrentChunk()
{
    return compilingChunk;
}

static void errorAt(Token *token, const char *message)
{
    // if parser is already in panic mode, don't report any other erors (prevent error cascade)
    if (parser.panicMode)
        return;

    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
    {
        fprintf(stderr, "at end");
    }
    else if (token->type == TOKEN_ERROR)
    {
        // NOTHING
    }
    else
    {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void errorAtCurrent(const char *message)
{
    errorAt(&parser.current, message);
}

static void errorAtPrevious(const char *message)
{
    errorAt(&parser.previous, message);
}

static void advance()
{
    parser.previous = parser.current;
    for (;;)
    {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char *message)
{
    if (parser.current.type == type)
    {
        advance();
        return;
    }

    errorAtCurrent(message);
}

// append byte to chunk
static void emitByte(uint8_t byte)
{
    writeChunk(getCurrentChunk(), byte, parser.previous.line);
}

// append two bytes to chunk. (convenience function made for two byte operations)
static void emitBytes(uint8_t byte1, uint8_t byte2)
{
    emitByte(byte1);
    emitByte(byte2);
}

static uint8_t makeConstant(Value value)
{
    int constantIdx = addConstant(getCurrentChunk(), value);
    // since OP_CONSTANT instruction uses one byte to store the index, we can only store upto 256 consts.
    if (constantIdx > UINT8_MAX)
    {
        errorAtPrevious("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constantIdx;
}

static void emitConstant(Value value)
{
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void emitReturn()
{
    emitByte(OP_RETURN);
}

static void endCompiler()
{
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError)
        disassembleChunk(getCurrentChunk(), "code");
#endif
}

static void expression();
static void grouping();
static ParseRule *getRule(TokenType type);

// parse expression of given precedence, and any expressions of higher precedence
static void parsePrecedence(Precedence precedence)
{
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL)
    {
        errorAtPrevious("Expect expression");
        return;
    }

    prefixRule();

    while (precedence <= getRule(parser.current.type)->precedence)
    {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }
}

static void binary()
{
    TokenType operatorType = parser.previous.type;
    ParseRule *rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType)
    {
    case TOKEN_BANG_EQUAL:
        emitBytes(OP_EQUAL, OP_NOT);
        break;
    case TOKEN_EQUAL_EQUAL:
        emitByte(OP_EQUAL);
        break;
    case TOKEN_GREATER:
        emitByte(OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emitBytes(OP_LESS, OP_NOT);
        break;
    case TOKEN_LESS:
        emitByte(OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emitBytes(OP_GREATER, OP_NOT);
    case TOKEN_PLUS:
        emitByte(OP_ADD);
        break;
    case TOKEN_MINUS:
        emitByte(OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emitByte(OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emitByte(OP_DIVIDE);
        break;
    default:
        return;
    }
}

static void literal()
{
    switch (parser.previous.type)
    {
    case TOKEN_NIL:
        emitByte(OP_FALSE);
        break;
    case TOKEN_FALSE:
        emitByte(OP_FALSE);
        break;
    case TOKEN_TRUE:
        emitByte(OP_TRUE);
        break;
    default:
        return;
    }
}

static void compileNumberToken()
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void string()
{
    // the +1 and -2 are for trimming the quotation marks
    emitConstant(OBJECT_VAL(copyString(parser.previous.start + 1,
                                       parser.previous.length - 2)));
}

static void compileUnaryExpression()
{
    TokenType operatorType = parser.previous.type;

    // compile the operand
    parsePrecedence(PREC_UNARY);

    // emit operator instruction
    switch (operatorType)
    {
    case TOKEN_BANG:
        emitByte(OP_NOT);
        break;
    case TOKEN_MINUS:
        emitByte(OP_NEGATE);
        break;
    default:
        return;
    }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {compileUnaryExpression, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {compileUnaryExpression, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_NONE},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_NONE},
    [TOKEN_GREATER] = {NULL, binary, PREC_NONE},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_NONE},
    [TOKEN_LESS] = {NULL, binary, PREC_NONE},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_NONE},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {compileNumberToken, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type)
{
    return &rules[type];
}

static void expression()
{
    // parse lowest precedence level, which automatically parses higher precedences
    parsePrecedence(PREC_ASSIGNMENT);
}

// assuming '(' has already been consumed, this function aims to parse the ')'
static void grouping()
{
    // recursively compile the expression between the parentheses
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

bool compileCode(const char *sourceCode, Chunk *chunk)
{
    initScanner(sourceCode);
    parser.hadError = false;
    parser.panicMode = false;

    compilingChunk = chunk;

    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");
    endCompiler();
    return !parser.hadError;
}