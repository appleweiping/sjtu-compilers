// tiger-compiler: front-end driver. Parses, type-checks, and (lab5) runs
// escape analysis over a Tiger source file. Code generation lives behind the
// LLVM-backed translation pass; see README for the lab5 status.
#include <cstdio>

#include "tiger/absyn/absyn.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/escape/escape.h"
#include "tiger/parse/parse.h"
#include "tiger/semant/semant.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <file.tig>\n", argv[0]);
    return 1;
  }
  err::ErrorMsg *em = nullptr;
  absyn::Exp *root = parse::ParseFile(argv[1], &em);
  if (!root) return 1;

  escape::FindEscape(root);
  semant::Analyze(root, em);
  if (em->AnyErrors()) return 1;

  printf("front-end OK: %s\n", argv[1]);
  return 0;
}
