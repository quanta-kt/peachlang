#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "scanner.h"
#include "value.h"
#include "object.h"
#include "vm.h"
#include "memory.h"


typedef struct {
  Token name;
  int depth;
  bool is_captured;
} Local;

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT
} FunctionType;


typedef struct {
  bool is_local;
  int index;
} Upvalue;

typedef struct Compiler Compiler;
struct Compiler {
  Compiler* enclosing;
  ObjectFunction* function;
  FunctionType type;

  Local* locals;
  size_t local_count;
  size_t local_capacity;
  int scope_depth;

  Upvalue upvalues[UINT8_COUNT];
};

typedef struct {
  Scanner* scanner;
  const char* source;

  Token current;
  Token previous;
  bool had_error;
  bool panic_mode;

  VM* vm;

  Compiler* current_compiler;
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
static void and_(Parser* parser, bool can_assign);
static void or_(Parser* parser, bool can_assign);
static void number(Parser* parser, bool can_assign);
static void literal(Parser* parser, bool can_assign);
static void string(Parser* parser, bool can_assign);
static void variable(Parser* parser, bool can_assign);
static void call(Parser* parser, bool can_assign);

static void expression(Parser* parser);
static void declaration(Parser* parser);
static void statement(Parser* parser);
static void block(Parser* parser);
static void print_statement(Parser* parser);
static void if_statement(Parser* parser);
static void while_statement(Parser* parser);
static void expression_statement(Parser* parser);
static void fn_declaration(Parser* parser);
static void var_declaration(Parser* parser);
static void return_statement(Parser* parser);

static void named_variable(Parser* parser, Token name, bool can_assign);
static size_t identifier_constant(Parser* parser, Token name);
static size_t parse_variable(Parser* parser, const char* err);
static void define_variable(Parser* parser, size_t global);
static void declare_variable(Parser* parser);
static void add_local(Parser* parser, Token name);
static bool identifier_equals(Token* const a, Token* const b);
static int resolve_local(Parser* parser, Compiler* compiler, Token* name);
static void mark_initialized(Parser* parser);
static void patch_jump(Parser* parser, size_t offset);

static void parse_precedence(Parser* parser, Precedence precedence);

static void emit_byte(Parser* parser, uint8_t byte);
static void emit_bytes(Parser* parser, uint8_t byte1, uint8_t byte2);
static void emit_return(Parser* parser);
static void emit_addr_bytes(Parser* parser, uint8_t short_op, uint8_t long_op, size_t addr);
static void emit_constant(Parser* parser, Value value);
static size_t emit_jump(Parser* parser, OpCode op);
static ObjectFunction* end_compiler(Parser* parser);
static void begin_scope(Parser* parser);
static void end_scope(Parser* parser);

static void Parser_advance(Parser* parser);
static bool Parser_check(Parser* parser, TokenType type);
static bool Parser_match(Parser* parser, TokenType type);
static void Parser_consume(Parser* parser, TokenType type, const char* message);
static void Parser_synchronize(Parser* parser);

static void error_at(Parser* parser, Token* token, const char* message);
static void error_at_current(Parser* parser, const char* message);
static void error(Parser* parser, const char* message);

static void Compiler_init(Compiler* compiler, Parser* parser, FunctionType type);
static void Compiler_free(Compiler* compiler);

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
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
  [TOKEN_LET]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FN]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};


static ParseRule* get_rule(TokenType type);

static Chunk* current_chunk(Parser* parser) {
  return &parser->current_compiler->function->chunk; 
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
  emit_byte(parser, OP_NIL);
  emit_byte(parser, OP_RETURN);
}

static size_t emit_jump(Parser* parser, OpCode op) {
  emit_byte(parser, op);

  // emit placeholder location which will be patched later
  // see: patch_jump
  emit_byte(parser, 0xff);
  emit_byte(parser, 0xff);
  return current_chunk(parser)->count - 2;
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

static ObjectFunction* end_compiler(Parser* parser) {
  emit_return(parser);
  ObjectFunction* function = parser->current_compiler->function;

  #ifdef DEBUG_PRINT_CODE
  if (!parser->had_error) {
    disassemble_chunk(current_chunk(parser),
                      function->name != NULL ? function->name->chars : "code");
  }
  #endif


  Compiler* compiler = parser->current_compiler;
  parser->current_compiler = compiler->enclosing;
  Compiler_free(compiler);

  return function;
}

static void begin_scope(Parser* parser) {
  parser->current_compiler->scope_depth++;
}

static void end_scope(Parser* parser) {
  parser->current_compiler->scope_depth--;

  Compiler* const compiler = parser->current_compiler;

  // pop out all the variables in the scope
  while (
    compiler->local_count > 0 &&
    compiler->locals[compiler->local_count - 1].depth > compiler->scope_depth) {
      if (compiler->locals[compiler->local_count - 1].is_captured) {
        emit_byte(parser, OP_CLOSE_UPVALUE);
      } else {
        emit_byte(parser, OP_POP);
      }
      compiler->local_count--;
  }
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

static void and_(Parser* parser, bool can_assign) {
  size_t end_jump = emit_jump(parser, OP_JUMP_IF_FALSE);

  emit_byte(parser, OP_POP);
  parse_precedence(parser, PREC_AND);

  patch_jump(parser, end_jump);
}

static void or_(Parser* parser, bool can_assign) {
  size_t else_jump = emit_jump(parser, OP_JUMP_IF_FALSE);
  size_t end_jump = emit_jump(parser, OP_JUMP);

  patch_jump(parser, else_jump);
  emit_byte(parser, OP_POP);

  parse_precedence(parser, PREC_OR);
  patch_jump(parser, end_jump);
}

static uint8_t argument_list(Parser* parser) {
  uint8_t arg_count = 0;
  if (!Parser_check(parser, TOKEN_RIGHT_PAREN)) {
    do {
      expression(parser);

      if (arg_count == 255) {
        error(parser, "Can't have more than 255 arguments.");
      }

      arg_count++;
    } while (Parser_match(parser, TOKEN_COMMA));
  }
  Parser_consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return arg_count;
}

static void call(Parser* parser, bool can_assign) {
  uint8_t arg_count = argument_list(parser);
  emit_bytes(parser, OP_CALL, arg_count);
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

static uint8_t add_upvalue(Parser* parser, Compiler* compiler, int index, bool is_local) {
  uint8_t upvalue_count = compiler->function->upvalue_count;

  for (uint8_t i = 0; i < upvalue_count; i++) {
    Upvalue* upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->is_local == is_local) {
      return i;
    }
  }

  if (upvalue_count == UINT8_COUNT) {
    // FIXME: Increase the upvalue limit by allocating a dynamic array for
    // upvalues
    error(parser, "Too many closure variables in a function.");
    return 0;
  }

  compiler->upvalues[upvalue_count].is_local = is_local;
  compiler->upvalues[upvalue_count].index = index;
  return compiler->function->upvalue_count++;
}

static int resolve_upvalue(Parser* parser, Compiler* compiler, Token* name) {
  if (compiler->enclosing == NULL) {
    return -1;
  }

  int local = resolve_local(parser, compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].is_captured = true;
    return add_upvalue(parser, compiler, local, true);
  }

  int upvalue = resolve_upvalue(parser, compiler->enclosing, name);
  if (upvalue != -1) {
    return add_upvalue(parser, compiler, upvalue, false);
  }

  return -1;
}

static void named_variable(Parser* parser, Token name, bool can_assign) {
  uint8_t get_op, get_op_long, set_op, set_op_long;
  int addr = resolve_local(parser, parser->current_compiler, &name);

  if (addr != -1) {
    get_op = OP_GET_LOCAL;
    get_op_long = OP_GET_LOCAL_LONG;
    set_op = OP_SET_LOCAL;
    set_op_long = OP_SET_LOCAL_LONG;
  } else if ((addr = resolve_upvalue(parser, parser->current_compiler, &name)) != -1) {
    get_op = OP_GET_UPVALUE;
    set_op = OP_SET_UPVALUE;
    // is it currently not possible for upvalue address to be greater than a byte.
  } else {
    addr = identifier_constant(parser, name);
    get_op = OP_GET_GLOBAL;
    get_op_long = OP_GET_GLOBAL_LONG;
    set_op = OP_SET_GLOBAL;
    set_op_long = OP_SET_GLOBAL_LONG;
  }


  if (can_assign && Parser_match(parser, TOKEN_EQUAL)) {
    expression(parser);
    emit_addr_bytes(parser, set_op, set_op_long, addr);
  } else {
    emit_addr_bytes(parser, get_op, get_op_long, addr);
  }
}

static int resolve_local(Parser* parser, Compiler* compiler, Token* name) {
  if (!compiler->local_count) return -1;

  size_t i = compiler->local_count - 1;

  for (;;) {
    Local* const local = &compiler->locals[i];

    if (identifier_equals(name, &local->name)) {
      if (local->depth == -1) {
        error(parser, "Can't read local variable in its own initializer.");
      }
      return i;
    }

    if (i == 0) {
      return -1;
    }

    i--;
  }
}

static void print_statement(Parser* parser) {
  expression(parser);
  Parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after value.");
  emit_byte(parser, OP_PRINT);
}

static void patch_jump(Parser* parser, size_t offset) {
  // -2 to get past the jump offset itself
  size_t jump = current_chunk(parser)->count - offset - 2;

  if (jump > UINT16_MAX) {
    error(parser, "Too much code to jump over.");
  }

  current_chunk(parser)->code[offset + 0] = (jump >> 0) & 0xff;
  current_chunk(parser)->code[offset + 1] = (jump >> 8) & 0xff;
}

static void if_statement(Parser* parser) {
  expression(parser);

  const size_t then_jump = emit_jump(parser, OP_JUMP_IF_FALSE);
  emit_byte(parser, OP_POP);

  Parser_consume(parser, TOKEN_LEFT_BRACE, "Expect '{' after condition.");
  begin_scope(parser);
  block(parser);
  end_scope(parser);

  size_t else_jump = emit_jump(parser, OP_JUMP);

  patch_jump(parser, then_jump);

  emit_byte(parser, OP_POP);
  if (Parser_match(parser, TOKEN_ELSE)) {
    Parser_consume(parser, TOKEN_LEFT_BRACE, "Expect '{' after else.");
    begin_scope(parser);
    block(parser);
    end_scope(parser);
  }

  patch_jump(parser, else_jump);
}

static void emit_loop(Parser* parser, size_t loop_start) {
  emit_byte(parser, OP_LOOP);


  size_t offset = current_chunk(parser)->count - loop_start + 2;
  if (offset > UINT16_MAX) {
    error(parser, "Loop body too large");
  }

  emit_byte(parser, (offset >> 0) & 0xff);
  emit_byte(parser, (offset >> 8) & 0xff);
}

static void while_statement(Parser* parser) {
  const size_t loop_start = current_chunk(parser)->count;

  expression(parser);

  const size_t exit_jump = emit_jump(parser, OP_JUMP_IF_FALSE);
  emit_byte(parser, OP_POP);

  Parser_consume(parser, TOKEN_LEFT_BRACE, "Expect '{' after condition.");
  begin_scope(parser);
  block(parser);
  end_scope(parser);
  emit_loop(parser, loop_start);

  patch_jump(parser, exit_jump);
  emit_byte(parser, OP_POP);
}

static void block(Parser* parser) {
  while (!Parser_check(parser, TOKEN_RIGHT_BRACE) && !Parser_check(parser, TOKEN_EOF)) {
    declaration(parser);
  }

  Parser_consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void expression_statement(Parser* parser) {
  expression(parser);
  Parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after expression.");
  emit_byte(parser, OP_POP);
}

static void statement(Parser* parser) {
  if (Parser_match(parser, TOKEN_PRINT)) {
    print_statement(parser);
  } else if (Parser_match(parser, TOKEN_IF)) {
    if_statement(parser);
  } else if (Parser_match(parser, TOKEN_WHILE)) {
    while_statement(parser);
  } else if (Parser_match(parser, TOKEN_LEFT_BRACE)) {
    begin_scope(parser);
    block(parser);
    end_scope(parser);
  } else if (Parser_match(parser, TOKEN_RETURN)) {
    return_statement(parser);
  } else {
    expression_statement(parser);
  }
}

static void declaration(Parser* parser) {
  if (Parser_match(parser, TOKEN_FN)) {
    fn_declaration(parser);
  } else if (Parser_match(parser, TOKEN_LET)) {
    var_declaration(parser);
  } else {
    statement(parser);
  }

  if (parser->panic_mode) Parser_synchronize(parser);
}

static void return_statement(Parser* parser) {
  if (parser->current_compiler->type == TYPE_SCRIPT) {
    error(parser, "Can't return from top-level code.");
  }

  if (Parser_match(parser, TOKEN_SEMICOLON)) {
    emit_return(parser);
  } else {
    expression(parser);
    Parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after return value.");
    emit_byte(parser, OP_RETURN);
  }
}

static void function(Parser* parser, FunctionType type) {
  Compiler compiler;
  Compiler_init(&compiler, parser, type);
  begin_scope(parser); 

  size_t line = parser->current.line;

  Parser_consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after function name.");

  if (!Parser_check(parser, TOKEN_RIGHT_PAREN)) {
    do {
      parser->current_compiler->function->arity++;

      if (parser->current_compiler->function->arity > 255) {
        error_at_current(parser, "Can't have more than 255 parameters.");
      }

      uint8_t constant = parse_variable(parser, "Expect parameter name.");
      define_variable(parser, constant);
    } while (Parser_match(parser, TOKEN_COMMA));
  }

  Parser_consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  Parser_consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
  block(parser);

  ObjectFunction* function = end_compiler(parser);
  emit_bytes(parser, OP_CLOSURE, Chunk_add_constant(current_chunk(parser), OBJECT_VAL(function)));

  for (int i = 0; i < function->upvalue_count; i++) {
    emit_byte(parser, compiler.upvalues[i].is_local ? 1 : 0);
    emit_byte(parser, compiler.upvalues[i].index);
  }
}

static void fn_declaration(Parser* parser) {
  uint8_t global = parse_variable(parser, "Expect function name.");
  mark_initialized(parser);
  function(parser, TYPE_FUNCTION);
  define_variable(parser, global);
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

static void declare_variable(Parser* parser) {
  if (parser->current_compiler->scope_depth == 0) return;

  Token* name = &parser->previous;

  // Re-declaration is prohibited in the same scope
  // while shadowing is allowed.
  if (parser->current_compiler->local_count == 0) goto end;

  for (size_t i = parser->current_compiler->local_count - 1; i > 0; i--) {
    Local* local = &parser->current_compiler->locals[i];

    if (local->depth != -1 && local->depth < parser->current_compiler->scope_depth) {
      break;
    }

    if (identifier_equals(name, &local->name)) {
      error(parser, "Already a variable with name in this scope");
      return;
    } 
  }

  end:
  add_local(parser, *name);
}

static bool identifier_equals(Token* const a, Token* const b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static void add_local(Parser* parser, Token name) {
  Compiler* const compiler = parser->current_compiler;

  if (compiler->local_capacity < compiler->local_count + 1) {
    size_t old_capacity = compiler->local_capacity;
    compiler->local_capacity = GROW_CAPACITY(compiler->local_capacity);
    compiler->locals = GROW_ARRAY(Local, compiler->locals, old_capacity, compiler->local_capacity);
  }

  Local* local = &compiler->locals[compiler->local_count++];
  local->name = name;
  local->depth = -1;
  local->is_captured = false;
}

static void mark_initialized(Parser* parser) {
  Compiler* compiler = parser->current_compiler;
  if (compiler->scope_depth == 0) return;

  size_t inx = compiler->local_count - 1;
  compiler->locals[inx].depth = compiler->scope_depth;
}

void define_variable(Parser* parser, size_t global) {
  if (parser->current_compiler->scope_depth > 0) {
    mark_initialized(parser);
    return;
  }

  emit_addr_bytes(parser, OP_DEF_GLOBAL, OP_DEF_GLOBAL_LONG, global);
}

static size_t identifier_constant(Parser* parser, Token name) {
  ObjectString* str;
  VM_get_intern_str(parser->vm, name.start, name.length, &str);
  return Chunk_add_constant(&parser->current_compiler->function->chunk, OBJECT_VAL(str));
}

static size_t parse_variable(Parser* parser, const char* err) {
  Parser_consume(parser, TOKEN_IDENTIFIER, err);

  declare_variable(parser);
  if (parser->current_compiler->scope_depth > 0) return 0;

  return identifier_constant(parser, parser->previous);
}

static void Parser_synchronize(Parser* parser) {
  parser->panic_mode = false;

  while (parser->current.type != EOF) {
    if (parser->previous.type == TOKEN_SEMICOLON) return;

    switch (parser->current.type) {
      case TOKEN_CLASS:
      case TOKEN_FN:
      case TOKEN_LET:
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

void Compiler_init(Compiler* compiler, Parser* parser, FunctionType type) {
  compiler->enclosing = parser->current_compiler;
  compiler->scope_depth = 0;
  compiler->function = NULL;
  compiler->type = type;

  compiler->function = ObjectFunction_create();

  compiler->local_count = 1;
  compiler->local_capacity = 1;
  compiler->locals = ALLOCATE(Local, compiler->local_capacity);

  parser->current_compiler = compiler;

  if (type != TYPE_SCRIPT) {
    compiler->function->name =
      ObjectString_copy(parser->previous.start, parser->previous.length);
  }

  Local* local = &parser->current_compiler->locals[0];
  local->depth = 0;
  local->name.start = "";
  local->name.length = 0;
  local->is_captured = false;
}

void Compiler_free(Compiler* compiler) {
  FREE_ARRAY(Local, compiler->locals, compiler->local_capacity);
  compiler->local_count = 0;
  compiler->local_capacity = 0;
  compiler->locals = NULL;
  compiler->scope_depth = 0;
}

ObjectFunction* compile(VM* vm, const char* source) {
  Scanner scanner;
  Scanner_init(&scanner, source);

  Parser parser = {
    .scanner = &scanner,
    .had_error = false,
    .panic_mode = false,
    .source = source,
    .vm = vm,
    .current_compiler = NULL,
  };

  Compiler compiler;
  Compiler_init(&compiler, &parser, TYPE_SCRIPT);

  Parser_advance(&parser);

  while (!Parser_match(&parser, TOKEN_EOF)) {
    declaration(&parser);
  }

  ObjectFunction* fn = end_compiler(&parser);

  return parser.had_error ? NULL : fn;
}

