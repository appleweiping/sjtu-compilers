#ifndef TIGER_ESCAPE_ESCAPE_H_
#define TIGER_ESCAPE_ESCAPE_H_

#include "tiger/absyn/absyn.h"

namespace escape {

// Compute which variables escape (are referenced from a more deeply nested
// function than the one that declares them) and record the result in the
// `escape` flags of VarDec / ForExp / Field. Appel §6.2.
void FindEscape(absyn::Exp *exp);

}  // namespace escape

#endif  // TIGER_ESCAPE_ESCAPE_H_
