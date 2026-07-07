#include "tiger/env/env.h"

namespace env {

namespace {
sym::Symbol *Sym(const char *s) { return sym::Symbol::UniqueSymbol(s); }

// Build a TyList from the given types.
type::TyList *Formals(std::initializer_list<type::Ty *> tys) {
  auto *list = new type::TyList();
  for (auto *t : tys) list->tys.push_back(t);
  return list;
}
}  // namespace

void FillBaseTEnv(TEnv *tenv) {
  tenv->Enter(Sym("int"), type::IntTy::Instance());
  tenv->Enter(Sym("string"), type::StringTy::Instance());
}

void FillBaseVEnv(VEnv *venv) {
  type::Ty *intTy = type::IntTy::Instance();
  type::Ty *strTy = type::StringTy::Instance();
  type::Ty *voidTy = type::VoidTy::Instance();

  // The Tiger standard library (Appel, appendix).
  venv->Enter(Sym("print"), new FunEntry(Formals({strTy}), voidTy));
  venv->Enter(Sym("printi"), new FunEntry(Formals({intTy}), voidTy));
  venv->Enter(Sym("flush"), new FunEntry(Formals({}), voidTy));
  venv->Enter(Sym("getchar"), new FunEntry(Formals({}), strTy));
  venv->Enter(Sym("ord"), new FunEntry(Formals({strTy}), intTy));
  venv->Enter(Sym("chr"), new FunEntry(Formals({intTy}), strTy));
  venv->Enter(Sym("size"), new FunEntry(Formals({strTy}), intTy));
  venv->Enter(Sym("substring"),
              new FunEntry(Formals({strTy, intTy, intTy}), strTy));
  venv->Enter(Sym("concat"), new FunEntry(Formals({strTy, strTy}), strTy));
  venv->Enter(Sym("not"), new FunEntry(Formals({intTy}), intTy));
  venv->Enter(Sym("exit"), new FunEntry(Formals({intTy}), voidTy));
}

}  // namespace env
