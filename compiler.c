#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "scanner.h"
#include "value.h"
#include "object.h"
#include "vm.h"


typedef struct {
  Scanner* scanner;
  Chunk* compiling_chunk;
  const char* source;

  Token current;
  Token previous;
  bool had_error;
  bool panic_mode;

  VM* vm;
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

typedef void (*ParseFn)(Parser*, bool);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static void binary(Parser* parser, bool can_assign);
static void grouping(Parser* parser, bool can_assign);
static void unary(Parser* parser, bool can_assign);
static void number(Parser* parser, bool can_assign);
static void literal(Parser* parser, bool can_assign);
static void string(Parser* parser, bool can_assign);
static void variable(Parser* parser, bool can_assign);

static void expression(Parser* parser);
static void declaration(Parser* parser);
static void statement(Parser* parser);
static void print_statement(Parser* parser);
static void expression_statement(Parser* parser);
static void var_declaration(Parser* parser);

static void named_variable(Parser* parser, Token name, bool can_assign);
static size_t identifier_constant(Parser* parser, Token name);
static size_t parse_variable(Parser* parser, const char* err);
void define_variable(Parser* parser, size_t global);

static void parse_precedence(Parser* parser, Precedence precedence);

static void emit_byte(Parser* parser, uint8_t byte);
static void emit_bytes(Parser* parser, uint8_t byte1, uint8_t byte2);
static void emit_return(Parser* parser);
static void emit_addr_bytes(Parser* parser, uint8_t short_op, uint8_t long_op, size_t addr);
static void emit_constant(Parser* parser, Value value);
static void end_compiler(Parser* parser);

static void Parser_advance(Parser* parser);
static bool Parser_check(Parser* parser, TokenType type);
static bool Parser_match(Parser* parser, TokenType type);
static void Parser_consume(Parser* parser, TokenType type, const char* message);
static void Parser_synchronize(Parser* parser);

static void error_at(Parser* parser, Token* token, const char* message);
static void error_at_current(Parser* parser, const char* message);
static void error(Parser* parser, const char* message);

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
  [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
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
    fprintf(stderr, " at '%.*s'", (int) token->length, token->start);
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

static bool Parser_check(Parser* parser, TokenType type) {
  return parser->current.type == type;
}

static bool Parser_match(Parser* parser, TokenType type) {
  if (!Parser_check(parser, type)) return false;
  Parser_advance(parser);
  return true;
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

static void emit_bytes(Parser* parser, uint8_t byte1, uint8_t byte2) {
  emit_byte(parser, byte1);
  emit_byte(parser, byte2);
}

static void emit_return(Parser* parser) {
  emit_byte(parser, OP_RETURN);
}

/**
 * Emit bytes for constants addressing.
 * Emits `short_op` if `addr` can be packed as a single byte operand.
 * Otherwise, uses long_op and splits `addr` into multi-byte operand.
 */
static void emit_addr_bytes(Parser* parser, uint8_t short_op, uint8_t long_op, size_t addr) {
  if (addr <= 0xff) {
    emit_bytes(parser, short_op, addr);
    return;
  }

  emit_bytes(parser, long_op, (addr >>  0) & 0xff);
  emit_bytes(parser, (addr >>  8) & 0xff, (addr >> 16) & 0xff);
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

  bool can_assign = precedence <= PREC_ASSIGNMENT;
  prefix_rule(parser, can_assign);

  while (precedence <= get_rule(parser->current.type)->precedence) {
    Parser_advance(parser);
    ParseFn infix_rule = get_rule(parser->previous.type)->infix;
    infix_rule(parser, can_assign);
  }

  if (can_assign && Parser_match(parser, TOKEN_EQUAL)) {
    error(parser, "Invalid assignment target.");
  }
}

static void expression(Parser* parser) {
  parse_precedence(parser, PREC_ASSIGNMENT);
}

static void number(Parser* parser, bool can_assign) {
  double value = strtod(parser->previous.start, NULL);
  emit_constant(parser, NUMBER_VAL(value));
}

static void literal(Parser* parser, bool can_assign) {
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

static void grouping(Parser* parser, bool can_assign) {
  expression(parser);
  Parser_consume(parser, TOKEN_RIGHT_PAREN, "Expect ')'");
}

static void unary(Parser* parser, bool can_assign) {
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

static void binary(Parser* parser, bool can_assign) {
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

static void string(Parser* parser, bool can_assign) {
  ObjectString* str;
  VM_get_intern_str(parser->vm, parser->previous.start + 1, parser->previous.length - 2, &str);

  Value value = OBJECT_VAL(str);
  emit_constant(parser, value);
}

static void variable(Parser* parser, bool can_assign) {
  named_variable(parser, parser->previous, can_assign);
}

static void named_variable(Parser* parser, Token name, bool can_assign) {
  size_t addr = identifier_constant(parser, name);

  if (can_assign && Parser_match(parser, TOKEN_EQUAL)) {
    expression(parser);
    emit_addr_bytes(parser, OP_SET_GLOBAL, OP_SET_GLOBAL_LONG, addr);
  } else {
    emit_addr_bytes(parser, OP_GET_GLOBAL, OP_GET_GLOBAL_LONG, addr);
  }
}

static void print_statement(Parser* parser) {
  expression(parser);
  Parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after value.");
  emit_byte(parser, OP_PRINT);
}

static void expression_statement(Parser* parser) {
  expression(parser);
  Parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after expression.");
  emit_byte(parser, OP_POP);
}

static void statement(Parser* parser) {
  if (Parser_match(parser, TOKEN_PRINT)) {
    print_statement(parser);
  } else {
    expression_statement(parser);
  }
}

static void declaration(Parser* parser) {
  if (Parser_match(parser, TOKEN_VAR)) {
    var_declaration(parser);
  } else {
    statement(parser);
  }

  if (parser->panic_mode) Parser_synchronize(parser);
}

static void var_declaration(Parser* parser) {
  size_t global = parse_variable(parser, "Expect variable name.");

  if (Parser_match(parser, TOKEN_EQUAL)) {
    expression(parser);
  } else {
    emit_byte(parser, OP_NIL);
  }

  Parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  define_variable(parser, global);
}

void define_variable(Parser* parser, size_t global) {
  emit_addr_bytes(parser, OP_DEF_GLOBAL, OP_DEF_GLOBAL_LONG, global);
}

static size_t identifier_constant(Parser* parser, Token name) {
  ObjectString* str;
  VM_get_intern_str(parser->vm, name.start, name.length, &str);
  return Chunk_add_constant(parser->compiling_chunk, OBJECT_VAL(str));
}

static size_t parse_variable(Parser* parser, const char* err) {
  Parser_consume(parser, TOKEN_IDENTIFIER, err);
  return identifier_constant(parser, parser->previous);
}

static void Parser_synchronize(Parser* parser) {
  parser->panic_mode = false;

  while (parser->current.type != EOF) {
    if (parser->previous.type == TOKEN_SEMICOLON) return;

    switch (parser->current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default: ;  // noop
    }

    Parser_advance(parser);
  }
}

bool compile(VM* vm, const char* source, Chunk* chunk) {
  Scanner scanner;
  Scanner_init(&scanner, source);

  Parser parser = {
    .scanner = &scanner,
    .compiling_chunk = chunk,
    .had_error = false,
    .panic_mode = false,
    .source = source,
    .vm = vm,
  };

  Parser_advance(&parser);

  while (!Parser_match(&parser, TOKEN_EOF)) {
    declaration(&parser);
  }

  end_compiler(&parser);

  return !parser.had_error;
}

