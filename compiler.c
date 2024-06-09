#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

typedef struct
{
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

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
}

static void compileNumberToken()
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(value);
}

static void expression()
{
}

void compileCode(const char *sourceCode, Chunk *chunk)
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