#ifndef TIGER_ABSYN_ABSYN_H_
#define TIGER_ABSYN_ABSYN_H_

#include <cstdio>
#include <list>
#include <string>

#include "tiger/symbol/symbol.h"

// Abstract syntax for the Tiger language (Appel, "Modern Compiler
// Implementation"). Each node stores the source position (character offset) of
// its first token so the semantic analyzer can report `line.col` errors.
namespace absyn {

using Pos = int;

// Binary operators. The names deliberately mirror the reference pretty-printer
// (PLUS, MINUS, ..., GREATEQ) so that test_parse output matches byte-for-byte.
enum Oper {
  PLUS_OP,
  MINUS_OP,
  TIMES_OP,
  DIVIDE_OP,
  EQ_OP,
  NEQ_OP,
  LT_OP,
  LE_OP,
  GT_OP,
  GE_OP,
};

// Forward declarations.
class Var;
class Exp;
class Dec;
class Ty;
class Field;
class ExpList;
class FieldList;
class EField;
class EFieldList;
class FunDec;
class FunDecList;
class NameAndTy;
class NameAndTyList;
class DecList;

// A record/function-parameter field: `name : typ`. `escape` starts TRUE and is
// refined by escape analysis (lab5).
class Field {
 public:
  Pos pos;
  sym::Symbol *name;
  sym::Symbol *typ;
  bool escape;
  Field(Pos pos, sym::Symbol *name, sym::Symbol *typ)
      : pos(pos), name(name), typ(typ), escape(true) {}
  void Print(FILE *out, int d) const;
};

class FieldList {
 public:
  std::list<Field *> fields;
  void Print(FILE *out, int d) const;
};

// A record-creation field: `name = exp`.
class EField {
 public:
  sym::Symbol *name;
  Exp *exp;
  EField(sym::Symbol *name, Exp *exp) : name(name), exp(exp) {}
  void Print(FILE *out, int d) const;
};

class EFieldList {
 public:
  std::list<EField *> efields;
  void Print(FILE *out, int d) const;
};

class ExpList {
 public:
  std::list<Exp *> exps;
  void Print(FILE *out, int d) const;
};

// -------------------------------------------------------------------------
// Variables (lvalues)
// -------------------------------------------------------------------------
class Var {
 public:
  Pos pos;
  explicit Var(Pos pos) : pos(pos) {}
  virtual ~Var() = default;
  virtual void Print(FILE *out, int d) const = 0;
};

class SimpleVar : public Var {
 public:
  sym::Symbol *sym;
  SimpleVar(Pos pos, sym::Symbol *sym) : Var(pos), sym(sym) {}
  void Print(FILE *out, int d) const override;
};

class FieldVar : public Var {
 public:
  Var *var;
  sym::Symbol *sym;
  FieldVar(Pos pos, Var *var, sym::Symbol *sym)
      : Var(pos), var(var), sym(sym) {}
  void Print(FILE *out, int d) const override;
};

class SubscriptVar : public Var {
 public:
  Var *var;
  Exp *subscript;
  SubscriptVar(Pos pos, Var *var, Exp *subscript)
      : Var(pos), var(var), subscript(subscript) {}
  void Print(FILE *out, int d) const override;
};

// -------------------------------------------------------------------------
// Expressions
// -------------------------------------------------------------------------
class Exp {
 public:
  Pos pos;
  explicit Exp(Pos pos) : pos(pos) {}
  virtual ~Exp() = default;
  virtual void Print(FILE *out, int d) const = 0;
};

class VarExp : public Exp {
 public:
  Var *var;
  VarExp(Pos pos, Var *var) : Exp(pos), var(var) {}
  void Print(FILE *out, int d) const override;
};

class NilExp : public Exp {
 public:
  explicit NilExp(Pos pos) : Exp(pos) {}
  void Print(FILE *out, int d) const override;
};

class IntExp : public Exp {
 public:
  int val;
  IntExp(Pos pos, int val) : Exp(pos), val(val) {}
  void Print(FILE *out, int d) const override;
};

class StringExp : public Exp {
 public:
  std::string str;
  StringExp(Pos pos, std::string str) : Exp(pos), str(std::move(str)) {}
  void Print(FILE *out, int d) const override;
};

class CallExp : public Exp {
 public:
  sym::Symbol *func;
  ExpList *args;
  CallExp(Pos pos, sym::Symbol *func, ExpList *args)
      : Exp(pos), func(func), args(args) {}
  void Print(FILE *out, int d) const override;
};

class OpExp : public Exp {
 public:
  Oper oper;
  Exp *left;
  Exp *right;
  OpExp(Pos pos, Oper oper, Exp *left, Exp *right)
      : Exp(pos), oper(oper), left(left), right(right) {}
  void Print(FILE *out, int d) const override;
};

class RecordExp : public Exp {
 public:
  sym::Symbol *typ;
  EFieldList *fields;
  RecordExp(Pos pos, sym::Symbol *typ, EFieldList *fields)
      : Exp(pos), typ(typ), fields(fields) {}
  void Print(FILE *out, int d) const override;
};

class SeqExp : public Exp {
 public:
  ExpList *seq;
  SeqExp(Pos pos, ExpList *seq) : Exp(pos), seq(seq) {}
  void Print(FILE *out, int d) const override;
};

class AssignExp : public Exp {
 public:
  Var *var;
  Exp *exp;
  AssignExp(Pos pos, Var *var, Exp *exp) : Exp(pos), var(var), exp(exp) {}
  void Print(FILE *out, int d) const override;
};

class IfExp : public Exp {
 public:
  Exp *test;
  Exp *then;
  Exp *elsee;  // may be nullptr
  IfExp(Pos pos, Exp *test, Exp *then, Exp *elsee)
      : Exp(pos), test(test), then(then), elsee(elsee) {}
  void Print(FILE *out, int d) const override;
};

class WhileExp : public Exp {
 public:
  Exp *test;
  Exp *body;
  WhileExp(Pos pos, Exp *test, Exp *body) : Exp(pos), test(test), body(body) {}
  void Print(FILE *out, int d) const override;
};

class ForExp : public Exp {
 public:
  sym::Symbol *var;
  Exp *lo;
  Exp *hi;
  Exp *body;
  bool escape;
  ForExp(Pos pos, sym::Symbol *var, Exp *lo, Exp *hi, Exp *body)
      : Exp(pos), var(var), lo(lo), hi(hi), body(body), escape(true) {}
  void Print(FILE *out, int d) const override;
};

class BreakExp : public Exp {
 public:
  explicit BreakExp(Pos pos) : Exp(pos) {}
  void Print(FILE *out, int d) const override;
};

class LetExp : public Exp {
 public:
  DecList *decs;
  Exp *body;
  LetExp(Pos pos, DecList *decs, Exp *body)
      : Exp(pos), decs(decs), body(body) {}
  void Print(FILE *out, int d) const override;
};

class ArrayExp : public Exp {
 public:
  sym::Symbol *typ;
  Exp *size;
  Exp *init;
  ArrayExp(Pos pos, sym::Symbol *typ, Exp *size, Exp *init)
      : Exp(pos), typ(typ), size(size), init(init) {}
  void Print(FILE *out, int d) const override;
};

// A parenthesized empty expression `()`.
class VoidExp : public Exp {
 public:
  explicit VoidExp(Pos pos) : Exp(pos) {}
  void Print(FILE *out, int d) const override;
};

// -------------------------------------------------------------------------
// Declarations
// -------------------------------------------------------------------------
class Dec {
 public:
  Pos pos;
  explicit Dec(Pos pos) : pos(pos) {}
  virtual ~Dec() = default;
  virtual void Print(FILE *out, int d) const = 0;
};

class DecList {
 public:
  std::list<Dec *> decs;
  void Print(FILE *out, int d) const;
};

// A single Tiger function declaration.
class FunDec {
 public:
  Pos pos;
  sym::Symbol *name;
  FieldList *params;
  sym::Symbol *result;  // may be nullptr (procedure)
  Exp *body;
  FunDec(Pos pos, sym::Symbol *name, FieldList *params, sym::Symbol *result,
         Exp *body)
      : pos(pos),
        name(name),
        params(params),
        result(result),
        body(body) {}
  void Print(FILE *out, int d) const;
};

class FunDecList {
 public:
  std::list<FunDec *> fundecs;
  void Print(FILE *out, int d) const;
};

// A block of (mutually-recursive) function declarations.
class FunctionDec : public Dec {
 public:
  FunDecList *functions;
  FunctionDec(Pos pos, FunDecList *functions)
      : Dec(pos), functions(functions) {}
  void Print(FILE *out, int d) const override;
};

class VarDec : public Dec {
 public:
  sym::Symbol *var;
  sym::Symbol *typ;  // may be nullptr (inferred)
  Exp *init;
  bool escape;
  VarDec(Pos pos, sym::Symbol *var, sym::Symbol *typ, Exp *init)
      : Dec(pos), var(var), typ(typ), init(init), escape(true) {}
  void Print(FILE *out, int d) const override;
};

// A single type binding `name = ty`.
class NameAndTy {
 public:
  sym::Symbol *name;
  Ty *ty;
  NameAndTy(sym::Symbol *name, Ty *ty) : name(name), ty(ty) {}
  void Print(FILE *out, int d) const;
};

class NameAndTyList {
 public:
  std::list<NameAndTy *> nametys;
  void Print(FILE *out, int d) const;
};

// A block of (mutually-recursive) type declarations.
class TypeDec : public Dec {
 public:
  NameAndTyList *types;
  TypeDec(Pos pos, NameAndTyList *types) : Dec(pos), types(types) {}
  void Print(FILE *out, int d) const override;
};

// -------------------------------------------------------------------------
// Type expressions
// -------------------------------------------------------------------------
class Ty {
 public:
  Pos pos;
  explicit Ty(Pos pos) : pos(pos) {}
  virtual ~Ty() = default;
  virtual void Print(FILE *out, int d) const = 0;
};

class NameTy : public Ty {
 public:
  sym::Symbol *name;
  NameTy(Pos pos, sym::Symbol *name) : Ty(pos), name(name) {}
  void Print(FILE *out, int d) const override;
};

class RecordTy : public Ty {
 public:
  FieldList *record;
  RecordTy(Pos pos, FieldList *record) : Ty(pos), record(record) {}
  void Print(FILE *out, int d) const override;
};

class ArrayTy : public Ty {
 public:
  sym::Symbol *array;
  ArrayTy(Pos pos, sym::Symbol *array) : Ty(pos), array(array) {}
  void Print(FILE *out, int d) const override;
};

// Print `exp` in the canonical Tiger AST format to `out`.
void PrintProgram(FILE *out, Exp *exp);

}  // namespace absyn

#endif  // TIGER_ABSYN_ABSYN_H_
