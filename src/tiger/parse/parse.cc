#include "tiger/parse/parse.h"

#include <cstdio>
#include <cstdlib>

#include "tiger/lex/scanner.h"

namespace parse {

absyn::Exp *ParseFile(const char *filename, err::ErrorMsg **em) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    fprintf(stderr, "cannot open %s\n", filename);
    exit(1);
  }
  auto *error = new err::ErrorMsg(filename);
  *em = error;
  parse::errormsg = error;
  lex::errormsg = error;
  lex::charPos = 1;
  parse::absyn_root = nullptr;

  yyin = f;
  yyrestart(yyin);
  int status = yyparse();
  fclose(f);

  if (status != 0 || error->AnyErrors()) return nullptr;
  return parse::absyn_root;
}

}  // namespace parse
