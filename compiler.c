#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
typedef void (*ParseFn)(bool canAssign);

typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

// struct to represent local variables
typedef struct
{
    Token name;
    int depth; // level of nesting where the variable appears
} Local;

typedef enum
{
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler
{
    struct Compiler *enclosing; // reference to the compiler of the surrounding function.
    FunctionObject *function;
    FunctionType functionType;
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;

Compiler *current = NULL;
Parser parser;
Chunk *compilingChunk;

static Chunk *getCurrentChunk()
{
    return &current->function->chunk;
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

static bool checkTokenType(TokenType type)
{
    return parser.current.type == type;
}

static bool match(TokenType type)
{
    if (!checkTokenType(type))
        return false;
    advance();
    return true;
}

// append byte to chunk
static void emitByte(uint8_t byte)
{
    writeChunk(getCurrentChunk(), byte, parser.previous.line);
}

static int emitJump(uint8_t instruction)
{
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return getCurrentChunk()->count - 2;
}

// append two bytes to chunk. (convenience function made for two byte operations)
static void emitBytes(uint8_t byte1, uint8_t byte2)
{
    emitByte(byte1);
    emitByte(byte2);
}

static void emitLoop(int loopStart)
{
    emitByte(OP_LOOP);

    int offset = getCurrentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
        errorAtCurrent("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
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

static void patchJump(int offset)
{
    int jump = getCurrentChunk()->count - offset - 2;
    if (jump > UINT16_MAX)
    {
        errorAtCurrent("Too much code to jump over.");
    }

    getCurrentChunk()->code[offset] = (jump >> 8) && 0xff;
    getCurrentChunk()->code[offset + 1] = jump & 0xff;
}

static void emitReturn()
{
    // return nil for functions that don't return anything
    emitByte(OP_NIL);
    emitByte(OP_RETURN);
}

static void beginScope()
{
    current->scopeDepth++;
}

static void endScope()
{
    current->scopeDepth--;

    // remove local variables from the stack after their scope ends.
    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth > current->scopeDepth)
    {
        emitByte(OP_POP);
        current->localCount--;
    }
}

static FunctionObject *endCompiler()
{
    emitReturn();
    FunctionObject *function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError)
        disassembleChunk(getCurrentChunk(), function->name != NULL ? function->name->chars : "<script>");
#endif

    current = current->enclosing;
    return function;
}

static void initCompiler(Compiler *compiler, FunctionType type)
{
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->functionType = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (type != TYPE_SCRIPT)
    {
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    // claim the zeroth stack slot in locals array for the VM's internal use.
    Local *local = &current->locals[current->localCount++];
    local->depth = 0;
    // give the slot an empty name so user can't write an identifier referring to it.
    local->name.start = "";
    local->name.length = 0;
}

static void expression();
static void grouping(bool canAssign);
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

    // make sure precedence is low enough to allow assignment
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence)
    {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        errorAtCurrent("Invalid assignment target.");
    }
}

static void binary(bool canAssign)
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

static void call(bool canAssign)
{
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static void literal(bool canAssign)
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

static void compileNumberToken(bool canAssign)
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

// the 'or' logical operator
// if left hand value is true, skip the right operand
// if left hand value is false, jump to the right operand
static void or_(bool canAssign)
{
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void string(bool canAssign)
{
    // the +1 and -2 are for trimming the quotation marks
    emitConstant(OBJECT_VAL(copyString(parser.previous.start + 1,
                                       parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign)
{
    uint8_t getOp, setOp;

    // see if there is a local variable with the given name.
    int arg = resolveLocal(current, &name);
    if (arg != -1)
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    // if no local variable is found, we assume it to be global variable
    else
    {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        // if there is an equal sign after an identifier, it means it is an assignment operation
        expression();
        emitBytes(setOp, (uint8_t)arg);
    }
    else
    {
        emitBytes(getOp, (uint8_t)arg);
    }
}

// record the existence of a local variable.
static void declareVariable()
{
    if (current->scopeDepth == 0)
        return;
    Token *name = &parser.previous;

    // throw error if there is an already existing variable with the same name in the current scope.
    for (int i = current->localCount - 1; i >= 0; i--)
    {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
        {
            break;
        }

        if (identifiersEqual(name, &local->name))
        {
            errorAtCurrent("Already a variable with this name in this scope");
        }
    }
    addLocal(*name);
}

static void variable(bool canAssign)
{
    namedVariable(parser.previous, canAssign);
}

static void compileUnaryExpression(bool canAssign)
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
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
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
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {compileNumberToken, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_NONE},
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

// takes a token and adds its lexeme to the chunk's constant table as a string
// returns index of that constant in the table
static uint8_t identifierConstant(Token *name)
{
    return makeConstant(OBJECT_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token *a, Token *b)
{
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

// returns the index of the last declared variable with a given name.
// if variable not found, the function returns -1.
static int resolveLocal(Compiler *compiler, Token *name)
{
    for (int i = compiler->localCount - 1; i >= 0; i--)
    {
        Local *local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name))
        {
            // local is in the special 'uninitialized' state.
            if (local->depth == -1)
            {
                errorAtCurrent("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

// add a local variable to the compiler's list of variables in the current scope.
static void addLocal(Token name)
{
    if (current->localCount == UINT8_COUNT)
    {
        errorAtCurrent("Too many local variables in a function.");
        return;
    }
    Local *local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1; // initial depth of -1 when the local is in a special temporary 'uninitialized' state.
}

static uint8_t parseVariable(const char *errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    // if we are in a local scope, no need to add variable to contant table.
    // just return a dummy table index.
    if (current->scopeDepth > 0)
        return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized()
{
    // make sure we are in a local scope
    if (current->scopeDepth == 0)
        return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global)
{
    // if we are in local scope, don't emit code to create variable.
    // because the local variable has been allocated at the top of the stack.
    if (current->scopeDepth > 0)
    {
        // mark the local variable as initialized
        markInitialized();
        return;
    }
    emitBytes(OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList()
{
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            expression();
            if (argCount == 255)
            {
                errorAtCurrent("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

// the 'and' logical operator is similar to an if-else
// if left side expression is false, skip the right operand
// if left side expression is true, evaluate the right side too
static void and_(bool canAssign)
{
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);
    patchJump(endJump);
}

static ParseRule *getRule(TokenType type)
{
    return &rules[type];
}

static void expression()
{
    // parse lowest precedence level, which automatically parses higher precedences
    parsePrecedence(PREC_ASSIGNMENT);
}

// compiles a function
static void function(FunctionType type)
{
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!checkTokenType(TOKEN_RIGHT_PAREN))
    {
        do
        {
            current->function->arity++;
            if (current->function->arity > 255)
            {
                errorAtCurrent("Can't have more than 255 parameters in a function.");
            }

            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    FunctionObject *function = endCompiler();
    emitBytes(OP_CONSTANT, makeConstant(OBJECT_VAL(function)));
}

static void funDeclaration()
{
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void varDeclaration()
{
    uint8_t global = parseVariable("Expect variable name.");

    // if variable is also being initialized
    if (match(TOKEN_EQUAL))
    {
        expression();
    }
    // if no initialization, implicitly assign value = nil
    else
    {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
}

static void expressionStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void forStatement()
{
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON))
    {
        // No initializer statement
    }
    else if (match(TOKEN_VAR))
    {
        varDeclaration();
    }
    else
    {
        expressionStatement();
    }
    int loopStart = getCurrentChunk()->count;
    int exitJump = -1;

    // check if any condition clause is present
    if (!match(TOKEN_SEMICOLON))
    {
        // condition clause is present
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP);
    }

    // check if increment clause is present
    if (!match(TOKEN_RIGHT_PAREN))
    {
        // increment clause is present
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = getCurrentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement();
    emitLoop(loopStart);

    if (exitJump != -1)
    {
        // patch the jump if any condition clause was present
        patchJump(exitJump);
        emitByte(OP_POP);
    }

    endScope();
}

static void ifStatement()
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE))
        statement();

    patchJump(elseJump);
}

static void block()
{
    while (!checkTokenType(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void printStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void returnStatement()
{
    if (current->functionType == TYPE_SCRIPT)
    {
        errorAtCurrent("Can't return from top-level code");
    }

    if (match(TOKEN_SEMICOLON))
    {
        emitReturn();
    }
    else
    {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void whileStatement()
{
    int loopStart = getCurrentChunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

static void synchronize()
{
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF)
    {
        // found a statement boundary
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;

        // keep skipping tokens until you find a statement boundary
        switch (parser.current.type)
        {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return;
        }

        advance();
    }
}

static void statement();
static void declaration();

static void declaration()
{
    if (match(TOKEN_FUN))
    {
        funDeclaration();
    }
    if (match(TOKEN_VAR))
    {
        varDeclaration();
    }
    else
    {
        statement();
    }
    if (parser.panicMode)
        synchronize();
}

static void statement()
{
    if (match(TOKEN_PRINT))
    {
        printStatement();
    }
    else if (match(TOKEN_FOR))
    {
        forStatement();
    }
    else if (match(TOKEN_IF))
    {
        ifStatement();
    }
    else if (match(TOKEN_RETURN))
    {
        returnStatement();
    }
    else if (match(TOKEN_WHILE))
    {
        whileStatement();
    }
    // blocks
    else if (match(TOKEN_LEFT_BRACE))
    {
        beginScope();
        block();
        endScope();
    }
    else
    {
        expressionStatement();
    }
}

// assuming '(' has already been consumed, this function aims to parse the ')'
static void grouping(bool canAssign)
{
    // recursively compile the expression between the parentheses
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

FunctionObject *compileCode(const char *sourceCode)
{
    initScanner(sourceCode);

    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF))
    {
        declaration();
    }

    FunctionObject *function = endCompiler();
    return parser.hadError ? NULL : function;
}