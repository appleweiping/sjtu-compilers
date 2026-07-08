#ifndef TIGER_TRANSLATE_TRANSLATE_H_
#define TIGER_TRANSLATE_TRANSLATE_H_

#include <string>

#include "tiger/absyn/absyn.h"

namespace tr {

// Translate the type-checked, escape-analyzed program `root` to textual LLVM
// IR and return it as a string. The IR calls into runtime.c for the standard
// library and for record/array/string allocation.
std::string Translate(absyn::Exp *root);

}  // namespace tr

#endif  // TIGER_TRANSLATE_TRANSLATE_H_
