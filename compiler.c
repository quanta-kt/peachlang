#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

void compile(const char* source) {
  Scanner scanner; Scanner_init(&scanner, source);

  size_t line = -1;
  for (;;) {
    Token token = Scanner_scan_token(&scanner, source);
    if (token.line != line) {
      printf("%4zu | ", token.line);
    } else {
      printf("     | ");
    }

    printf("%2d '%.*s'\n", token.type, (int)token.length, token.start);

    if (token.type == TOKEN_EOF) break;
  }
}

