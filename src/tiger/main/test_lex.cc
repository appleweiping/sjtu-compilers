// test_lex: run the Tiger scanner over a source file and print the token
// stream in the reference format `%10s%5d [value]`, one token per line.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "tiger/errormsg/errormsg.h"
#include "tiger/lex/scanner.h"
#include "tiger/parse/parser.h"  // token codes + yylval

extern int yylex();
extern FILE *yyin;
void yyrestart(FILE *);

namespace {

// Map a bison token code to its printable name.
const char *TokenName(int tok) {
  switch (tok) {
    case ID: return "ID";
    case STRING: return "STRING";
    case INT: return "INT";
    case COMMA: return "COMMA";
    case COLON: return "COLON";
    case SEMICOLON: return "SEMICOLON";
    case LPAREN: return "LPAREN";
    case RPAREN: return "RPAREN";
    case LBRACK: return "LBRACK";
    case RBRACK: return "RBRACK";
    case LBRACE: return "LBRACE";
    case RBRACE: return "RBRACE";
    case DOT: return "DOT";
    case PLUS: return "PLUS";
    case MINUS: return "MINUS";
    case TIMES: return "TIMES";
    case DIVIDE: return "DIVIDE";
    case EQ: return "EQ";
    case NEQ: return "NEQ";
    case LT: return "LT";
    case LE: return "LE";
    case GT: return "GT";
    case GE: return "GE";
    case AND: return "AND";
    case OR: return "OR";
    case ASSIGN: return "ASSIGN";
    case ARRAY: return "ARRAY";
    case IF: return "IF";
    case THEN: return "THEN";
    case ELSE: return "ELSE";
    case WHILE: return "WHILE";
    case FOR: return "FOR";
    case TO: return "TO";
    case DO: return "DO";
    case LET: return "LET";
    case IN: return "IN";
    case END: return "END";
    case OF: return "OF";
    case BREAK: return "BREAK";
    case NIL: return "NIL";
    case FUNCTION: return "FUNCTION";
    case VAR: return "VAR";
    case TYPE: return "TYPE";
    default: return "?";
  }
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <file.tig>\n", argv[0]);
    return 1;
  }
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    fprintf(stderr, "cannot open %s\n", argv[1]);
    return 1;
  }
  auto *em = new err::ErrorMsg(argv[1]);
  lex::errormsg = em;
  lex::charPos = 1;
  yyin = f;
  yyrestart(yyin);

  int tok;
  while ((tok = yylex()) != 0) {
    // A STRING is reported at its opening quote; every other token at the
    // start of its lexeme (captured by Adjust into token_pos).
    int pos = (tok == STRING) ? lex::str_start : lex::token_pos;
    printf("%10s%5d", TokenName(tok), pos);
    if (tok == ID) {
      printf(" %s", yylval.sval->c_str());
    } else if (tok == INT) {
      printf(" %d", yylval.ival);
    } else if (tok == STRING) {
      // The reference lexer stored an empty string literal as a null char
      // pointer, so `printf("%s", ...)` rendered it as "(null)". Match that.
      if (yylval.sval->empty())
        printf(" %s", (const char *)nullptr);
      else
        printf(" %s", yylval.sval->c_str());
    }
    printf("\n");
  }
  fclose(f);
  return 0;
}
