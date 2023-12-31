#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "scanner.h"
#include "value.h"
#include "object.h"


typedef struct {
  Scanner* scanner;
  Chunk* compiling_chunk;
  const char* source;

  Token current;
  Token previous;
  bool had_error;
  bool panic_mode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,
  PREC_OR,
  PREC_AND,
  PREC_EQUALITY,
  PREC_COMPARISON,
  PREC_TERM,
  PREC_FACTOR,
  PREC_UNARY,
  PREC_CALL,
  PREC_PRIMARY,
} Precedence;

typedef void (*ParseFn)(Parser*);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;


static void expression(Parser* parser);
static void binary(Parser* parser);
static void grouping(Parser* parser);
static void unary(Parser* parser);
static void number(Parser* parser);
static void literal(Parser* parser);
static void string(Parser* parser);

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};


static void Parser_advance(Parser* parser);
static void Parser_consume(Parser* parser, TokenType type, const char* message);
static void parse_precedence(Parser* parser, Precedence precedence);

static ParseRule* get_rule(TokenType type);


static Chunk* current_chunk(Parser* parser) {
  return parser->compiling_chunk;
}

static void error_at(Parser* parser, Token* token, const char* message) {
  if (parser->panic_mode) return;

  parser->panic_mode = true;
  fprintf(stderr, "[line %zu] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {

  } else {
    fprintf(stderr, "at '%*.s'", (int) token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser->had_error = true;
}

static void error_at_current(Parser* parser, const char* message) {
  error_at(parser, &parser->current, message);
}

static void error(Parser* parser, const char* message) {
  error_at(parser, &parser->previous, message);
}

static void Parser_advance(Parser* parser) {
  parser->previous = parser->current;

  for (;;) {
    parser->current = Scanner_scan_token(parser->scanner);
    if (parser->current.type != TOKEN_ERROR) break;

    error_at_current(parser, parser->current.start);
  }
}

static void Parser_consume(Parser* parser, TokenType type, const char* message) {
  if (parser->current.type == type) {
    Parser_advance(parser);
    return;
  }

  error_at_current(parser, message);
}

static void emit_byte(Parser* parser, uint8_t byte) {
  Chunk_write(current_chunk(parser), byte, parser->previous.line);
}

static void emit_return(Parser* parser) {
  emit_byte(parser, OP_RETURN);
}

static void emit_bytes(Parser* parser, uint8_t byte1, uint8_t byte2) {
  emit_byte(parser, byte1);
  emit_byte(parser, byte2);
}

static void emit_constant(Parser* parser, Value value) {
  Chunk_write_constant(current_chunk(parser), value, parser->previous.line);
}

static void end_compiler(Parser* parser) {
  emit_return(parser);

  #ifdef DEBUG_PRINT_CODE
  if (!parser->had_error) {
    disassemble_chunk(current_chunk(parser), "code");
  }
  #endif
}


static ParseRule* get_rule(TokenType type) {
  return &rules[type];
}

static void parse_precedence(Parser* parser, Precedence precedence) {
  Parser_advance(parser);

  ParseFn prefix_rule = get_rule(parser->previous.type)->prefix;
  if (prefix_rule == NULL) {
    error(parser, "Expect expression.");
    return;
  }

  prefix_rule(parser);

  while (precedence <= get_rule(parser->current.type)->precedence) {
    Parser_advance(parser);
    ParseFn infix_rule = get_rule(parser->previous.type)->infix;
    infix_rule(parser);
  }
}

static void expression(Parser* parser) {
  parse_precedence(parser, PREC_ASSIGNMENT);
}

static void number(Parser* parser) {
  double value = strtod(parser->previous.start, NULL);
  emit_constant(parser, NUMBER_VAL(value));
}

static void literal(Parser* parser) {
  switch (parser->previous.type) {
    case TOKEN_NIL:
      emit_byte(parser, OP_NIL);
      break;

    case TOKEN_TRUE:
      emit_byte(parser, OP_TRUE);
      break;

    case TOKEN_FALSE:
      emit_byte(parser, OP_FALSE);
      break;

    default: // Unreachable
      return;
  }
}

static void grouping(Parser* parser) {
  expression(parser);
  Parser_consume(parser, TOKEN_RIGHT_PAREN, "Expect ')'");
}

static void unary(Parser* parser) {
  TokenType op_type = parser->previous.type;

  parse_precedence(parser, PREC_UNARY);

  switch (op_type) {
    case TOKEN_MINUS:
      emit_byte(parser, OP_NEGATE);
      break;

    case TOKEN_BANG:
      emit_byte(parser, OP_NOT);
      break;

    default:
      return; // Unreachable
  }
}

static void binary(Parser* parser) {
  TokenType op_type = parser->previous.type;
  ParseRule* rule = get_rule(op_type);
  parse_precedence(parser, (Precedence)(rule->precedence + 1));

  switch (op_type) {
    case TOKEN_BANG_EQUAL:    emit_bytes(parser, OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL:   emit_byte(parser, OP_EQUAL); break;
    case TOKEN_GREATER:       emit_byte(parser, OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emit_bytes(parser, OP_LESS, OP_NOT); break;
    case TOKEN_LESS:          emit_byte(parser, OP_LESS); break;
    case TOKEN_LESS_EQUAL:    emit_bytes(parser, OP_GREATER, OP_NOT); break;

    case TOKEN_PLUS:  emit_byte(parser, OP_ADD); break;
    case TOKEN_MINUS: emit_byte(parser, OP_SUB); break;
    case TOKEN_STAR:  emit_byte(parser, OP_MUL); break;
    case TOKEN_SLASH: emit_byte(parser, OP_DIV); break;

    default: return; // Unreachable
  }
}

static void string(Parser* parser) {
  ObjectString* str = ObjectString_from_cstring(parser->previous.start + 1, parser->previous.length - 2);
  Value value = OBJECT_VAL(str);
  emit_constant(parser, value);
}

bool compile(const char* source, Chunk* chunk) {
  Scanner scanner;
  Scanner_init(&scanner, source);

  Parser parser = {
    .scanner = &scanner,
    .compiling_chunk = chunk,
    .had_error = false,
    .panic_mode = false,
    .source = source
  };

  Parser_advance(&parser);
  expression(&parser);
  Parser_consume(&parser, TOKEN_EOF, "Expect end of expression.");

  end_compiler(&parser);

  return !parser.had_error;
}

