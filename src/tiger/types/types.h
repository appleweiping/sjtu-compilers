#ifndef TIGER_TYPES_TYPES_H_
#define TIGER_TYPES_TYPES_H_

#include <list>
#include <string>

#include "tiger/symbol/symbol.h"

// Semantic types for Tiger (Appel §5). NAME types support forward references
// during mutually-recursive type declarations; ActualTy() chases the chain.
namespace type {

class Ty;
class TyList;
class Field;
class FieldList;

class Ty {
 public:
  virtual ~Ty() = default;
  // Resolve NAME indirections to the underlying type.
  virtual Ty *ActualTy();
};

class IntTy : public Ty {
 public:
  static IntTy *Instance();

 private:
  IntTy() = default;
};

class StringTy : public Ty {
 public:
  static StringTy *Instance();

 private:
  StringTy() = default;
};

// The type of `nil` and of procedures returning no value / `()`.
class NilTy : public Ty {
 public:
  static NilTy *Instance();

 private:
  NilTy() = default;
};

class VoidTy : public Ty {
 public:
  static VoidTy *Instance();

 private:
  VoidTy() = default;
};

// A record type: an ordered list of (name, type) fields. Identity-based:
// two RecordTy objects are equal only if they are the same object.
class RecordTy : public Ty {
 public:
  FieldList *fields;
  explicit RecordTy(FieldList *fields) : fields(fields) {}
};

class ArrayTy : public Ty {
 public:
  Ty *ty;
  explicit ArrayTy(Ty *ty) : ty(ty) {}
};

// A named type; `ty` is filled in once the declaration block is resolved.
class NameTy : public Ty {
 public:
  sym::Symbol *sym;
  Ty *ty;  // may be nullptr until resolved
  NameTy(sym::Symbol *sym, Ty *ty) : sym(sym), ty(ty) {}
  Ty *ActualTy() override;
};

class Field {
 public:
  sym::Symbol *name;
  Ty *ty;
  Field(sym::Symbol *name, Ty *ty) : name(name), ty(ty) {}
};

class FieldList {
 public:
  std::list<Field *> fields;
};

class TyList {
 public:
  std::list<Ty *> tys;
};

}  // namespace type

#endif  // TIGER_TYPES_TYPES_H_
