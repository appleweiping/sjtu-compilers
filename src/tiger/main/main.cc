// tiger-compiler: the full front-end + LLVM-IR backend driver.
//
// Usage:  tiger-compiler foo.tig
//   1. parse + type-check + escape-analyze foo.tig
//   2. translate the AST to LLVM IR (foo.tig.ll)
//   3. lower the IR to native assembly with llc, producing foo.tig.s
//
// The grader then links foo.tig.s with runtime.c and runs the result.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "tiger/absyn/absyn.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/escape/escape.h"
#include "tiger/parse/parse.h"
#include "tiger/semant/semant.h"
#include "tiger/translate/translate.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <file.tig>\n", argv[0]);
    return 1;
  }
  std::string input = argv[1];

  err::ErrorMsg *em = nullptr;
  absyn::Exp *root = parse::ParseFile(input.c_str(), &em);
  if (!root) return 1;

  escape::FindEscape(root);
  semant::Analyze(root, em);
  if (em->AnyErrors()) return 1;

  std::string ir = tr::Translate(root);

  std::string ll = input + ".ll";
  std::string asmf = input + ".s";
  {
    std::ofstream out(ll);
    out << ir;
  }

  // Lower to native assembly. Try llc variants in order.
  const char *llcs[] = {"llc", "llc-18", "llc-17", "llc-16", "llc-15"};
  int rc = -1;
  for (const char *llc : llcs) {
    std::string cmd = std::string(llc) + " -O2 --relocation-model=pic \"" + ll +
                      "\" -o \"" + asmf + "\" 2>/dev/null";
    rc = system(cmd.c_str());
    if (rc == 0) break;
  }
  if (rc != 0) {
    fprintf(stderr, "error: could not lower LLVM IR with llc\n");
    return 1;
  }
  return 0;
}
