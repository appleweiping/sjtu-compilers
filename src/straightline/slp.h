#ifndef STRAIGHTLINE_SLP_H_
#define STRAIGHTLINE_SLP_H_

#include <algorithm>
#include <cassert>
#include <string>
#include <list>

namespace A {

class Stm;
class Exp;
class ExpList;

enum BinOp { PLUS = 0, MINUS, TIMES, DIV };

// some data structures used by interp
class Table;
class IntAndTable;

class Stm {
 public:
  virtual int MaxArgs() const = 0;
  virtual Table *Interp(Table *) const = 0;
};

class CompoundStm : public Stm {
 public:
  CompoundStm(Stm *stm1, Stm *stm2) : stm1(stm1), stm2(stm2) {}
  int MaxArgs() const override;
  Table *Interp(Table *) const override;

 private:
  Stm *stm1, *stm2;
};

class AssignStm : public Stm {
 public:
  AssignStm(std::string id, Exp *exp) : id(std::move(id)), exp(exp) {}
  int MaxArgs() const override;
  Table *Interp(Table *) const override;

 private:
  std::string id;
  Exp *exp;
};

class PrintStm : public Stm {
 public:
  explicit PrintStm(ExpList *exps) : exps(exps) {}
  int MaxArgs() const override;
  Table *Interp(Table *) const override;

 private:
  ExpList *exps;
};

class Exp {
 public:
  // Maximum number of arguments to any single `print` reachable from this
  // expression (through embedded EseqExp statements).
  virtual int MaxArgs() const = 0;
  // Evaluate the expression in table `t`, yielding the value and the
  // (possibly updated) table.
  virtual IntAndTable *Interp(Table *t) const = 0;
  virtual ~Exp() = default;
};

class IdExp : public Exp {
 public:
  explicit IdExp(std::string id) : id(std::move(id)) {}
  int MaxArgs() const override;
  IntAndTable *Interp(Table *t) const override;

 private:
  std::string id;
};

class NumExp : public Exp {
 public:
  explicit NumExp(int num) : num(num) {}
  int MaxArgs() const override;
  IntAndTable *Interp(Table *t) const override;

 private:
  int num;
};

class OpExp : public Exp {
 public:
  OpExp(Exp *left, BinOp oper, Exp *right)
      : left(left), oper(oper), right(right) {}
  int MaxArgs() const override;
  IntAndTable *Interp(Table *t) const override;

 private:
  Exp *left;
  BinOp oper;
  Exp *right;
};

class EseqExp : public Exp {
 public:
  EseqExp(Stm *stm, Exp *exp) : stm(stm), exp(exp) {}
  int MaxArgs() const override;
  IntAndTable *Interp(Table *t) const override;

 private:
  Stm *stm;
  Exp *exp;
};

class ExpList {
 public:
  // Largest MaxArgs among the expressions in this list.
  virtual int MaxArgs() const = 0;
  // Number of expressions in this list (arguments to the enclosing print).
  virtual int NumExps() const = 0;
  // Evaluate & print every expression, returning the final table.
  virtual Table *Interp(Table *t) const = 0;
  virtual ~ExpList() = default;
};

class PairExpList : public ExpList {
 public:
  PairExpList(Exp *exp, ExpList *tail) : exp(exp), tail(tail) {}
  int MaxArgs() const override;
  int NumExps() const override;
  Table *Interp(Table *t) const override;

 private:
  Exp *exp;
  ExpList *tail;
};

class LastExpList : public ExpList {
 public:
  explicit LastExpList(Exp *exp) : exp(exp) {}
  int MaxArgs() const override;
  int NumExps() const override;
  Table *Interp(Table *t) const override;

 private:
  Exp *exp;
};

class Table {
 public:
  Table(std::string id, int value, const Table *tail)
      : id(std::move(id)), value(value), tail(tail) {}
  int Lookup(const std::string &key) const;
  Table *Update(const std::string &key, int val) const;

 private:
  std::string id;
  int value;
  const Table *tail;
};

struct IntAndTable {
  int i;
  Table *t;

  IntAndTable(int i, Table *t) : i(i), t(t) {}
};

}  // namespace A

#endif  // STRAIGHTLINE_SLP_H_
