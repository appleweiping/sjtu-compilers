#ifndef TIGER_LEX_SCANNER_H_
#define TIGER_LEX_SCANNER_H_

#include <string>

#include "tiger/errormsg/errormsg.h"

// Shared state and helpers for the flex-generated Tiger scanner. `charPos`
// tracks the 1-based character offset of the current token, matching the
// reference lexer's position column.
namespace lex {

// 1-based character position of the token currently being scanned.
extern int charPos;

// Start position of the most recently matched token (set by Adjust before it
// advances charPos). Used by test_lex to print each token's position.
extern int token_pos;

// The error reporter for the file being scanned (set by the driver).
extern err::ErrorMsg *errormsg;

// Accumulates the decoded contents of the string literal in progress.
extern std::string string_buf;

// Position of the opening quote of the string literal in progress (a STRING
// token is reported at its opening quote, not its closing one).
extern int str_start;

// Advance `charPos` by the length of the just-matched lexeme `text`.
void Adjust(const char *text, int leng);

}  // namespace lex

#endif  // TIGER_LEX_SCANNER_H_
