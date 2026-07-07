%{
/* Flex scanner for the Tiger language.
 *
 * Position handling: `lex::charPos` holds the 1-based character offset of the
 * current token. Adjust() advances it by each matched lexeme; newlines are
 * additionally recorded with ErrorMsg::Newline for line/column reporting.
 *
 * The scanner returns bison token codes (from parser.h) and fills yylval.
 * Strings and nested comments are handled with start conditions. */
#include <cstdlib>
#include <string>

#include "tiger/lex/scanner.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/symbol/symbol.h"
#include "tiger/absyn/absyn.h"
#include "tiger/parse/parser.h"

int lex::charPos = 1;
int lex::token_pos = 1;
int lex::str_start = 1;
err::ErrorMsg *lex::errormsg = nullptr;
std::string lex::string_buf;

void lex::Adjust(const char *text, int leng) {
  lex::token_pos = lex::charPos;
  lex::charPos += leng;
}

/* Depth of nested comments (Tiger comments nest). */
static int comment_depth = 0;
/* Character position at which the current string/comment started. */
static int token_start = 0;
%}

%option noyywrap
%option nounput
%option noinput
%x COMMENT STR

digit    [0-9]
id       [A-Za-z][A-Za-z0-9_]*

%%

 /* ---- whitespace ----
  * Carriage returns are consumed but do NOT advance the position counter, so
  * that CRLF-terminated source files produce the same offsets as LF ones
  * (matching the reference lexer). */
\r           { /* ignore, no position change */ }
\n           { lex::errormsg->Newline(lex::charPos); lex::Adjust(yytext, yyleng); }
[ \t]+       { lex::Adjust(yytext, yyleng); }

 /* ---- comments (nested) ---- */
"/*"         { lex::Adjust(yytext, yyleng); comment_depth = 1; BEGIN(COMMENT); }
<COMMENT>{
  "/*"       { lex::Adjust(yytext, yyleng); ++comment_depth; }
  "*/"       { lex::Adjust(yytext, yyleng); if (--comment_depth == 0) BEGIN(INITIAL); }
  \r         { /* ignore, no position change */ }
  \n         { lex::errormsg->Newline(lex::charPos); lex::Adjust(yytext, yyleng); }
  .          { lex::Adjust(yytext, yyleng); }
  <<EOF>>    { lex::errormsg->Error(lex::charPos, "unterminated comment"); yyterminate(); }
}

 /* ---- keywords ---- */
"array"      { lex::Adjust(yytext, yyleng); return ARRAY; }
"if"         { lex::Adjust(yytext, yyleng); return IF; }
"then"       { lex::Adjust(yytext, yyleng); return THEN; }
"else"       { lex::Adjust(yytext, yyleng); return ELSE; }
"while"      { lex::Adjust(yytext, yyleng); return WHILE; }
"for"        { lex::Adjust(yytext, yyleng); return FOR; }
"to"         { lex::Adjust(yytext, yyleng); return TO; }
"do"         { lex::Adjust(yytext, yyleng); return DO; }
"let"        { lex::Adjust(yytext, yyleng); return LET; }
"in"         { lex::Adjust(yytext, yyleng); return IN; }
"end"        { lex::Adjust(yytext, yyleng); return END; }
"of"         { lex::Adjust(yytext, yyleng); return OF; }
"break"      { lex::Adjust(yytext, yyleng); return BREAK; }
"nil"        { lex::Adjust(yytext, yyleng); return NIL; }
"function"   { lex::Adjust(yytext, yyleng); return FUNCTION; }
"var"        { lex::Adjust(yytext, yyleng); return VAR; }
"type"       { lex::Adjust(yytext, yyleng); return TYPE; }

 /* ---- punctuation & operators ---- */
","          { lex::Adjust(yytext, yyleng); return COMMA; }
":"          { lex::Adjust(yytext, yyleng); return COLON; }
";"          { lex::Adjust(yytext, yyleng); return SEMICOLON; }
"("          { lex::Adjust(yytext, yyleng); return LPAREN; }
")"          { lex::Adjust(yytext, yyleng); return RPAREN; }
"["          { lex::Adjust(yytext, yyleng); return LBRACK; }
"]"          { lex::Adjust(yytext, yyleng); return RBRACK; }
"{"          { lex::Adjust(yytext, yyleng); return LBRACE; }
"}"          { lex::Adjust(yytext, yyleng); return RBRACE; }
"."          { lex::Adjust(yytext, yyleng); return DOT; }
"+"          { lex::Adjust(yytext, yyleng); return PLUS; }
"-"          { lex::Adjust(yytext, yyleng); return MINUS; }
"*"          { lex::Adjust(yytext, yyleng); return TIMES; }
"/"          { lex::Adjust(yytext, yyleng); return DIVIDE; }
"="          { lex::Adjust(yytext, yyleng); return EQ; }
"<>"         { lex::Adjust(yytext, yyleng); return NEQ; }
"<"          { lex::Adjust(yytext, yyleng); return LT; }
"<="         { lex::Adjust(yytext, yyleng); return LE; }
">"          { lex::Adjust(yytext, yyleng); return GT; }
">="         { lex::Adjust(yytext, yyleng); return GE; }
"&"          { lex::Adjust(yytext, yyleng); return AND; }
"|"          { lex::Adjust(yytext, yyleng); return OR; }
":="         { lex::Adjust(yytext, yyleng); return ASSIGN; }

 /* ---- literals ---- */
{digit}+     { lex::Adjust(yytext, yyleng); yylval.ival = atoi(yytext); return INT; }
{id}         { lex::Adjust(yytext, yyleng);
               yylval.sval = new std::string(yytext); return ID; }

 /* ---- string literals ---- */
\"           { lex::str_start = lex::charPos; token_start = lex::charPos;
               lex::Adjust(yytext, yyleng);
               lex::string_buf.clear(); BEGIN(STR); }
<STR>{
  \"                 { lex::Adjust(yytext, yyleng);
                       yylval.sval = new std::string(lex::string_buf);
                       BEGIN(INITIAL); return STRING; }
  \\n                { lex::Adjust(yytext, yyleng); lex::string_buf += '\n'; }
  \\t                { lex::Adjust(yytext, yyleng); lex::string_buf += '\t'; }
  \\\"               { lex::Adjust(yytext, yyleng); lex::string_buf += '"'; }
  \\\\               { lex::Adjust(yytext, yyleng); lex::string_buf += '\\'; }
  \\[0-9]{3}         { lex::Adjust(yytext, yyleng);
                       lex::string_buf += (char)atoi(yytext + 1); }
  \\\^[@-_]          { lex::Adjust(yytext, yyleng);
                       lex::string_buf += (char)(yytext[2] - '@'); }
  \\[ \t\r\n\f]+\\   { /* line-continuation: swallow whitespace between \ \ */
                       for (int i = 0; i < yyleng; ++i)
                         if (yytext[i] == '\n') lex::errormsg->Newline(lex::charPos + i);
                       lex::Adjust(yytext, yyleng); }
  \r                 { /* ignore, no position change */ }
  \n                 { lex::errormsg->Error(lex::charPos, "unterminated string");
                       lex::errormsg->Newline(lex::charPos);
                       lex::Adjust(yytext, yyleng); BEGIN(INITIAL); }
  \\.                { lex::Adjust(yytext, yyleng);
                       lex::errormsg->Error(lex::charPos, "illegal escape"); }
  .                  { lex::Adjust(yytext, yyleng); lex::string_buf += yytext[0]; }
  <<EOF>>            { lex::errormsg->Error(token_start, "unterminated string");
                       yyterminate(); }
}

 /* ---- anything else is an error ---- */
.            { lex::Adjust(yytext, yyleng);
               lex::errormsg->Error(lex::charPos, std::string("illegal token ") + yytext); }

%%
