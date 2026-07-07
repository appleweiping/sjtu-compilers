// test_parse: parse a Tiger source file and pretty-print its AST. Exits with a
// non-zero status on a syntax error (so the grader can detect negative cases).
#include <cstdio>

#include "tiger/absyn/absyn.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/parse/parse.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <file.tig>\n", argv[0]);
    return 1;
  }
  err::ErrorMsg *em = nullptr;
  absyn::Exp *root = parse::ParseFile(argv[1], &em);
  if (!root) {
    // A syntax error was already reported by yyerror.
    return 1;
  }
  absyn::PrintProgram(stdout, root);
  return 0;
}
