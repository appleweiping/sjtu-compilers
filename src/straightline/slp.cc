#include "straightline/slp.h"

#include <algorithm>
#include <cstdio>

namespace A {

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------
int A::CompoundStm::MaxArgs() const {
  return std::max(stm1->MaxArgs(), stm2->MaxArgs());
}

Table *A::CompoundStm::Interp(Table *t) const {
  return stm2->Interp(stm1->Interp(t));
}

int A::AssignStm::MaxArgs() const { return exp->MaxArgs(); }

Table *A::AssignStm::Interp(Table *t) const {
  IntAndTable *v = exp->Interp(t);
  return v->t->Update(id, v->i);
}

int A::PrintStm::MaxArgs() const {
  // A print of k expressions has k arguments; but any of those expressions may
  // itself contain a bigger print (via EseqExp), so take the max.
  return std::max(exps->NumExps(), exps->MaxArgs());
}

Table *A::PrintStm::Interp(Table *t) const {
  Table *t2 = exps->Interp(t);
  printf("\n");
  return t2;
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------
int IdExp::MaxArgs() const { return 0; }

IntAndTable *IdExp::Interp(Table *t) const {
  return new IntAndTable(t->Lookup(id), t);
}

int NumExp::MaxArgs() const { return 0; }

IntAndTable *NumExp::Interp(Table *t) const {
  return new IntAndTable(num, t);
}

int OpExp::MaxArgs() const {
  return std::max(left->MaxArgs(), right->MaxArgs());
}

IntAndTable *OpExp::Interp(Table *t) const {
  IntAndTable *l = left->Interp(t);
  IntAndTable *r = right->Interp(l->t);
  int result = 0;
  switch (oper) {
    case PLUS:
      result = l->i + r->i;
      break;
    case MINUS:
      result = l->i - r->i;
      break;
    case TIMES:
      result = l->i * r->i;
      break;
    case DIV:
      result = l->i / r->i;
      break;
  }
  return new IntAndTable(result, r->t);
}

int EseqExp::MaxArgs() const {
  return std::max(stm->MaxArgs(), exp->MaxArgs());
}

IntAndTable *EseqExp::Interp(Table *t) const {
  return exp->Interp(stm->Interp(t));
}

// ---------------------------------------------------------------------------
// Expression lists
// ---------------------------------------------------------------------------
int PairExpList::MaxArgs() const {
  return std::max(exp->MaxArgs(), tail->MaxArgs());
}

int PairExpList::NumExps() const { return 1 + tail->NumExps(); }

Table *PairExpList::Interp(Table *t) const {
  IntAndTable *v = exp->Interp(t);
  printf("%d ", v->i);
  return tail->Interp(v->t);
}

int LastExpList::MaxArgs() const { return exp->MaxArgs(); }

int LastExpList::NumExps() const { return 1; }

Table *LastExpList::Interp(Table *t) const {
  IntAndTable *v = exp->Interp(t);
  printf("%d", v->i);
  return v->t;
}

// ---------------------------------------------------------------------------
// The value environment (a persistent association list)
// ---------------------------------------------------------------------------
int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
    __builtin_unreachable();
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}

}  // namespace A
