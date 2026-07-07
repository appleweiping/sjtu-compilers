#ifndef TIGER_ENV_ENV_H_
#define TIGER_ENV_ENV_H_

#include "tiger/symbol/symbol.h"
#include "tiger/types/types.h"

// The type and value environments used by semantic analysis (Appel §5.2).
namespace env {

// An entry in the value environment: either a variable or a function.
class EnvEntry {
 public:
  virtual ~EnvEntry() = default;
};

class VarEntry : public EnvEntry {
 public:
  type::Ty *ty;
  bool readonly;  // true for the loop variable of a for-loop
  explicit VarEntry(type::Ty *ty, bool readonly = false)
      : ty(ty), readonly(readonly) {}
};

class FunEntry : public EnvEntry {
 public:
  type::TyList *formals;
  type::Ty *result;
  FunEntry(type::TyList *formals, type::Ty *result)
      : formals(formals), result(result) {}
};

using TEnv = sym::Table<type::Ty>;
using VEnv = sym::Table<EnvEntry>;

// Populate `tenv` with the predefined types (int, string).
void FillBaseTEnv(TEnv *tenv);

// Populate `venv` with the standard library functions (print, ord, chr, ...).
void FillBaseVEnv(VEnv *venv);

}  // namespace env

#endif  // TIGER_ENV_ENV_H_
