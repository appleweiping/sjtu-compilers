// test_semant: parse a Tiger file and run semantic analysis, printing any type
// errors in the reference `file:line.col:message` format.
#include <cstdio>

#include "tiger/absyn/absyn.h"
#include "tiger/errormsg/errormsg.h"
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
  semant::Analyze(root, em);
  return 0;
}
