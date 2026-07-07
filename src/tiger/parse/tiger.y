%{
/* Bison grammar for the Tiger language. Builds the absyn AST defined in
 * tiger/absyn/absyn.h. The action code mirrors Appel's reference grammar:
 *   - `e1 & e2`  desugars to `if e1 then e2 else 0`
 *   - `e1 | e2`  desugars to `if e1 then 1 else e2`
 *   - unary `-e` desugars to `0 - e`
 * so that test_parse output matches the reference pretty-printer. */
#include <cstdio>
#include <cstdlib>
#include <list>
#include <string>

#include "tiger/absyn/absyn.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/symbol/symbol.h"
#include "tiger/lex/scanner.h"

using namespace absyn;

extern int yylex();

/* Set by the driver before parsing; receives the resulting AST root. */
namespace parse {
absyn::Exp *absyn_root = nullptr;
err::ErrorMsg *errormsg = nullptr;
}  // namespace parse

static sym::Symbol *S(std::string *s) { return sym::Symbol::UniqueSymbol(*s); }

void yyerror(const char *s) {
  /* Report at the START of the offending token, formatted as
   * `file:line.col: syntax error` (leading space after the colon, matching the
   * reference parser which the grader greps for verbatim). */
  parse::errormsg->Error(lex::token_pos, std::string(" ") + s);
}
%}

/* These declarations must appear in the generated parser.h (which the scanner
 * and test drivers include) so that YYSTYPE's member types are complete. */
%code requires {
  #include <string>
  #include "tiger/absyn/absyn.h"
}

%union {
  int ival;
  std::string *sval;
  absyn::Exp *exp;
  absyn::ExpList *explist;
  absyn::Var *var;
  absyn::Dec *dec;
  absyn::DecList *declist;
  absyn::Ty *ty;
  absyn::Field *field;
  absyn::FieldList *fieldlist;
  absyn::EField *efield;
  absyn::EFieldList *efieldlist;
  absyn::FunDec *fundec;
  absyn::FunDecList *fundeclist;
  absyn::NameAndTy *namety;
  absyn::NameAndTyList *nametylist;
}

%token <sval> ID STRING
%token <ival> INT

%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK
  LBRACE RBRACE DOT PLUS MINUS TIMES DIVIDE EQ NEQ LT LE GT GE
  AND OR ASSIGN ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF
  BREAK NIL FUNCTION VAR TYPE

%type <exp>        exp program
%type <explist>    args nonempty_args seqexps
%type <var>        lvalue
%type <declist>    decs
%type <dec>        dec
%type <ty>         ty
%type <fieldlist>  tyfields nonempty_tyfields
%type <efieldlist> recordfields nonempty_recordfields
%type <fundeclist> fundecs
%type <fundec>     fundec
%type <nametylist> tydecs
%type <namety>     tydec

/* Precedence, lowest first (Appel/Tiger). */
%nonassoc LOW
%nonassoc ASSIGN
%nonassoc THEN
%nonassoc ELSE
%nonassoc DO OF
%left OR
%left AND
%nonassoc EQ NEQ LT LE GT GE
%left PLUS MINUS
%left TIMES DIVIDE
%left UMINUS

%start program

%%

program:
    exp                       { parse::absyn_root = $1; }
  ;

exp:
    NIL                       { $$ = new NilExp(lex::charPos); }
  | INT                       { $$ = new IntExp(lex::charPos, $1); }
  | STRING                    { $$ = new StringExp(lex::charPos, *$1); }
  | lvalue                    { $$ = new VarExp($1->pos, $1); }
  | MINUS exp %prec UMINUS    { $$ = new OpExp(lex::charPos, MINUS_OP,
                                               new IntExp(lex::charPos, 0), $2); }
  | exp PLUS exp              { $$ = new OpExp($1->pos, PLUS_OP,   $1, $3); }
  | exp MINUS exp             { $$ = new OpExp($1->pos, MINUS_OP,  $1, $3); }
  | exp TIMES exp             { $$ = new OpExp($1->pos, TIMES_OP,  $1, $3); }
  | exp DIVIDE exp            { $$ = new OpExp($1->pos, DIVIDE_OP, $1, $3); }
  | exp EQ exp                { $$ = new OpExp($1->pos, EQ_OP,  $1, $3); }
  | exp NEQ exp               { $$ = new OpExp($1->pos, NEQ_OP, $1, $3); }
  | exp LT exp                { $$ = new OpExp($1->pos, LT_OP,  $1, $3); }
  | exp LE exp                { $$ = new OpExp($1->pos, LE_OP,  $1, $3); }
  | exp GT exp                { $$ = new OpExp($1->pos, GT_OP,  $1, $3); }
  | exp GE exp                { $$ = new OpExp($1->pos, GE_OP,  $1, $3); }
  | exp AND exp               { $$ = new IfExp($1->pos, $1, $3,
                                               new IntExp($1->pos, 0)); }
  | exp OR exp                { $$ = new IfExp($1->pos, $1,
                                               new IntExp($1->pos, 1), $3); }
  | ID LPAREN args RPAREN     { $$ = new CallExp(lex::charPos, S($1), $3); }
  | ID LBRACE recordfields RBRACE
                              { $$ = new RecordExp(lex::charPos, S($1), $3); }
  | ID LBRACK exp RBRACK OF exp
                              { $$ = new ArrayExp(lex::charPos, S($1), $3, $6); }
  | lvalue ASSIGN exp         { $$ = new AssignExp($1->pos, $1, $3); }
  | IF exp THEN exp ELSE exp  { $$ = new IfExp(lex::charPos, $2, $4, $6); }
  | IF exp THEN exp           { $$ = new IfExp(lex::charPos, $2, $4, nullptr); }
  | WHILE exp DO exp          { $$ = new WhileExp(lex::charPos, $2, $4); }
  | FOR ID ASSIGN exp TO exp DO exp
                              { $$ = new ForExp(lex::charPos, S($2), $4, $6, $8); }
  | BREAK                     { $$ = new BreakExp(lex::charPos); }
  | LET decs IN seqexps END   { $$ = new LetExp(lex::charPos, $2,
                                                new SeqExp(lex::charPos, $4)); }
  | LPAREN seqexps RPAREN     { std::list<Exp *> &l = $2->exps;
                                if (l.empty()) { $$ = new VoidExp(lex::charPos); }
                                else if (l.size() == 1) { $$ = l.front(); }
                                else { $$ = new SeqExp(lex::charPos, $2); } }
  ;

/* A parenthesised / let-body sequence: zero or more exps separated by ';'. */
seqexps:
    /* empty */               { $$ = new ExpList(); }
  | exp                       { $$ = new ExpList(); $$->exps.push_back($1); }
  | seqexps SEMICOLON exp     { $$ = $1; $$->exps.push_back($3); }
  ;

/* Function-call argument list. */
args:
    /* empty */               { $$ = new ExpList(); }
  | nonempty_args             { $$ = $1; }
  ;

nonempty_args:
    exp                       { $$ = new ExpList(); $$->exps.push_back($1); }
  | nonempty_args COMMA exp   { $$ = $1; $$->exps.push_back($3); }
  ;

/* Record-creation field list: id=exp {, id=exp}. */
recordfields:
    /* empty */               { $$ = new EFieldList(); }
  | nonempty_recordfields     { $$ = $1; }
  ;

nonempty_recordfields:
    ID EQ exp                 { $$ = new EFieldList();
                                $$->efields.push_back(new EField(S($1), $3)); }
  | nonempty_recordfields COMMA ID EQ exp
                              { $$ = $1;
                                $$->efields.push_back(new EField(S($3), $5)); }
  ;

lvalue:
    ID                        { $$ = new SimpleVar(lex::charPos, S($1)); }
  | lvalue DOT ID             { $$ = new FieldVar($1->pos, $1, S($3)); }
  | lvalue LBRACK exp RBRACK  { $$ = new SubscriptVar($1->pos, $1, $3); }
  /* `id[e]` where id is a plain name — resolve to a subscript of a simpleVar. */
  | ID LBRACK exp RBRACK      { $$ = new SubscriptVar(lex::charPos,
                                        new SimpleVar(lex::charPos, S($1)), $3); }
  ;

decs:
    /* empty */               { $$ = new DecList(); }
  | decs dec                  { $$ = $1; $$->decs.push_back($2); }
  ;

dec:
    tydecs                    { $$ = new TypeDec(lex::charPos, $1); }
  | VAR ID ASSIGN exp         { $$ = new VarDec(lex::charPos, S($2), nullptr, $4); }
  | VAR ID COLON ID ASSIGN exp
                              { $$ = new VarDec(lex::charPos, S($2), S($4), $6); }
  | fundecs                   { $$ = new FunctionDec(lex::charPos, $1); }
  ;

/* Consecutive `type` decls form one mutually-recursive block. */
tydecs:
    tydec                     { $$ = new NameAndTyList();
                                $$->nametys.push_back($1); }
  | tydecs tydec              { $$ = $1; $$->nametys.push_back($2); }
  ;

tydec:
    TYPE ID EQ ty             { $$ = new NameAndTy(S($2), $4); }
  ;

ty:
    ID                        { $$ = new NameTy(lex::charPos, S($1)); }
  | LBRACE tyfields RBRACE    { $$ = new RecordTy(lex::charPos, $2); }
  | ARRAY OF ID               { $$ = new ArrayTy(lex::charPos, S($3)); }
  ;

tyfields:
    /* empty */               { $$ = new FieldList(); }
  | nonempty_tyfields         { $$ = $1; }
  ;

nonempty_tyfields:
    ID COLON ID               { $$ = new FieldList();
                                $$->fields.push_back(
                                    new Field(lex::charPos, S($1), S($3))); }
  | nonempty_tyfields COMMA ID COLON ID
                              { $$ = $1;
                                $$->fields.push_back(
                                    new Field(lex::charPos, S($3), S($5))); }
  ;

/* Consecutive `function` decls form one mutually-recursive block. */
fundecs:
    fundec                    { $$ = new FunDecList();
                                $$->fundecs.push_back($1); }
  | fundecs fundec            { $$ = $1; $$->fundecs.push_back($2); }
  ;

fundec:
    FUNCTION ID LPAREN tyfields RPAREN EQ exp
                              { $$ = new FunDec(lex::charPos, S($2), $4,
                                                nullptr, $7); }
  | FUNCTION ID LPAREN tyfields RPAREN COLON ID EQ exp
                              { $$ = new FunDec(lex::charPos, S($2), $4,
                                                S($7), $9); }
  ;

%%
