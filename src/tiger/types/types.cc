#include "tiger/types/types.h"

namespace type {

Ty *Ty::ActualTy() { return this; }

Ty *NameTy::ActualTy() {
  // Follow the chain of NAME types; `ty` is set once the decl block resolves.
  return ty ? ty->ActualTy() : this;
}

IntTy *IntTy::Instance() {
  static IntTy *inst = new IntTy();
  return inst;
}

StringTy *StringTy::Instance() {
  static StringTy *inst = new StringTy();
  return inst;
}

NilTy *NilTy::Instance() {
  static NilTy *inst = new NilTy();
  return inst;
}

VoidTy *VoidTy::Instance() {
  static VoidTy *inst = new VoidTy();
  return inst;
}

}  // namespace type
