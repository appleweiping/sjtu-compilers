#ifndef TIGER_SEMANT_SEMANT_H_
#define TIGER_SEMANT_SEMANT_H_

#include "tiger/absyn/absyn.h"
#include "tiger/errormsg/errormsg.h"

namespace semant {

// Type-check the program rooted at `exp`, reporting errors through `em`.
void Analyze(absyn::Exp *exp, err::ErrorMsg *em);

}  // namespace semant

#endif  // TIGER_SEMANT_SEMANT_H_
