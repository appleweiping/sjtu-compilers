#ifndef TIGER_PARSE_PARSE_H_
#define TIGER_PARSE_PARSE_H_

#include "tiger/absyn/absyn.h"
#include "tiger/errormsg/errormsg.h"

// Bison entry point and the globals it communicates through.
extern int yyparse();
extern FILE *yyin;
void yyrestart(FILE *);

namespace parse {
extern absyn::Exp *absyn_root;   // AST root, set on a successful parse
extern err::ErrorMsg *errormsg;  // error reporter used by yyerror

// Parse `filename`, returning its AST (or nullptr on syntax error). `em`
// receives the file's error reporter.
absyn::Exp *ParseFile(const char *filename, err::ErrorMsg **em);
}  // namespace parse

#endif  // TIGER_PARSE_PARSE_H_
