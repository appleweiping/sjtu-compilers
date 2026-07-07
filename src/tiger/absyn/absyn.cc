#include "tiger/absyn/absyn.h"

// Pretty-printer for the Tiger AST. The output layout reproduces the reference
// `test_parse` exactly: every level is indented by one extra space, list nodes
// nest their tail so the closing parentheses stack up at the end.

namespace absyn {

namespace {

// Emit `d` spaces of indentation.
void Indent(FILE *out, int d) {
  for (int i = 0; i < d; ++i) fprintf(out, " ");
}

const char *OperName(Oper o) {
  switch (o) {
    case PLUS_OP:
      return "PLUS";
    case MINUS_OP:
      return "MINUS";
    case TIMES_OP:
      return "TIMES";
    case DIVIDE_OP:
      return "DIVIDE";
    case EQ_OP:
      return "EQUAL";
    case NEQ_OP:
      return "NOTEQUAL";
    case LT_OP:
      return "LESSTHAN";
    case LE_OP:
      return "LESSEQ";
    case GT_OP:
      return "GREAT";
    case GE_OP:
      return "GREATEQ";
  }
  return "??";
}

const char *SymName(sym::Symbol *s) { return s ? s->Name().c_str() : ""; }

}  // namespace

void PrintProgram(FILE *out, Exp *exp) {
  exp->Print(out, 1);
  fprintf(out, "\n");
}

// -------------------------------------------------------------------------
// Variables
// -------------------------------------------------------------------------
void SimpleVar::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "simpleVar(%s)", SymName(sym));
}

void FieldVar::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "fieldVar(\n");
  var->Print(out, d + 1);
  fprintf(out, ",\n");
  Indent(out, d + 1);
  fprintf(out, "%s)", SymName(sym));
}

void SubscriptVar::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "subscriptVar(\n");
  var->Print(out, d + 1);
  fprintf(out, ",\n");
  subscript->Print(out, d + 1);
  fprintf(out, ")");
}

// -------------------------------------------------------------------------
// Expressions
// -------------------------------------------------------------------------
void VarExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "varExp(\n");
  var->Print(out, d + 1);
  fprintf(out, ")");
}

void NilExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "nilExp()");
}

void IntExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "intExp(%d)", val);
}

void StringExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "stringExp(%s)", str.c_str());
}

void CallExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "callExp(%s,\n", SymName(func));
  args->Print(out, d + 1);
  fprintf(out, ")");
}

void OpExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "opExp(\n");
  Indent(out, d + 1);
  fprintf(out, "%s,\n", OperName(oper));
  left->Print(out, d + 1);
  fprintf(out, ",\n");
  right->Print(out, d + 1);
  fprintf(out, ")");
}

void RecordExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "recordExp(%s,\n", SymName(typ));
  fields->Print(out, d + 1);
  fprintf(out, ")");
}

void SeqExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "seqExp(\n");
  seq->Print(out, d + 1);
  fprintf(out, ")");
}

void AssignExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "assignExp(\n");
  var->Print(out, d + 1);
  fprintf(out, ",\n");
  exp->Print(out, d + 1);
  fprintf(out, ")");
}

void IfExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "iffExp(\n");
  test->Print(out, d + 1);
  fprintf(out, ",\n");
  then->Print(out, d + 1);
  if (elsee) {
    fprintf(out, ",\n");
    elsee->Print(out, d + 1);
  }
  fprintf(out, ")");
}

void WhileExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "whileExp(\n");
  test->Print(out, d + 1);
  fprintf(out, ",\n");
  body->Print(out, d + 1);
  fprintf(out, ")");
}

void ForExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "forExp(%s,\n", SymName(var));
  lo->Print(out, d + 1);
  fprintf(out, ",\n");
  hi->Print(out, d + 1);
  fprintf(out, ",\n");
  body->Print(out, d + 1);
  fprintf(out, ",\n");
  Indent(out, d + 1);
  fprintf(out, "%s)", escape ? "TRUE" : "FALSE");
}

void BreakExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "breakExp()");
}

void LetExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "letExp(\n");
  decs->Print(out, d + 1);
  fprintf(out, ",\n");
  body->Print(out, d + 1);
  fprintf(out, ")");
}

void ArrayExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "arrayExp(%s,\n", SymName(typ));
  size->Print(out, d + 1);
  fprintf(out, ",\n");
  init->Print(out, d + 1);
  fprintf(out, ")");
}

void VoidExp::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "voidExp()");
}

// -------------------------------------------------------------------------
// Declarations
// -------------------------------------------------------------------------
void FunctionDec::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "functionDec(\n");
  functions->Print(out, d + 1);
  fprintf(out, ")");
}

void VarDec::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "varDec(%s,\n", SymName(var));
  if (typ) {
    Indent(out, d + 1);
    fprintf(out, "%s,\n", SymName(typ));
  }
  init->Print(out, d + 1);
  fprintf(out, ",\n");
  Indent(out, d + 1);
  fprintf(out, "%s)", escape ? "TRUE" : "FALSE");
}

void TypeDec::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "typeDec(\n");
  types->Print(out, d + 1);
  fprintf(out, ")");
}

// -------------------------------------------------------------------------
// Type expressions
// -------------------------------------------------------------------------
void NameTy::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "nameTy(%s)", SymName(name));
}

void RecordTy::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "recordTy(\n");
  record->Print(out, d + 1);
  fprintf(out, ")");
}

void ArrayTy::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "arrayTy(%s)", SymName(array));
}

// -------------------------------------------------------------------------
// List and leaf helpers
// -------------------------------------------------------------------------
void ExpList::Print(FILE *out, int d) const {
  Indent(out, d);
  if (exps.empty()) {
    fprintf(out, "expList()");
    return;
  }
  fprintf(out, "expList(\n");
  auto it = exps.begin();
  (*it)->Print(out, d + 1);
  fprintf(out, ",\n");
  ExpList tail;
  tail.exps.assign(std::next(it), exps.end());
  tail.Print(out, d + 1);
  fprintf(out, ")");
}

void FieldList::Print(FILE *out, int d) const {
  Indent(out, d);
  if (fields.empty()) {
    fprintf(out, "fieldList()");
    return;
  }
  fprintf(out, "fieldList(\n");
  auto it = fields.begin();
  (*it)->Print(out, d + 1);
  fprintf(out, ",\n");
  FieldList tail;
  tail.fields.assign(std::next(it), fields.end());
  tail.Print(out, d + 1);
  fprintf(out, ")");
}

void Field::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "field(%s,\n", SymName(name));
  Indent(out, d + 1);
  fprintf(out, "%s,\n", SymName(typ));
  Indent(out, d + 1);
  fprintf(out, "%s)", escape ? "TRUE" : "FALSE");
}

void EFieldList::Print(FILE *out, int d) const {
  Indent(out, d);
  if (efields.empty()) {
    fprintf(out, "efieldList()");
    return;
  }
  fprintf(out, "efieldList(\n");
  auto it = efields.begin();
  (*it)->Print(out, d + 1);
  fprintf(out, ",\n");
  EFieldList tail;
  tail.efields.assign(std::next(it), efields.end());
  tail.Print(out, d + 1);
  fprintf(out, ")");
}

void EField::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "efield(%s,\n", SymName(name));
  exp->Print(out, d + 1);
  fprintf(out, ")");
}

void DecList::Print(FILE *out, int d) const {
  Indent(out, d);
  if (decs.empty()) {
    fprintf(out, "decList()");
    return;
  }
  fprintf(out, "decList(\n");
  auto it = decs.begin();
  (*it)->Print(out, d + 1);
  fprintf(out, ",\n");
  DecList tail;
  tail.decs.assign(std::next(it), decs.end());
  tail.Print(out, d + 1);
  fprintf(out, ")");
}

void FunDecList::Print(FILE *out, int d) const {
  Indent(out, d);
  if (fundecs.empty()) {
    fprintf(out, "fundecList()");
    return;
  }
  fprintf(out, "fundecList(\n");
  auto it = fundecs.begin();
  (*it)->Print(out, d + 1);
  fprintf(out, ",\n");
  FunDecList tail;
  tail.fundecs.assign(std::next(it), fundecs.end());
  tail.Print(out, d + 1);
  fprintf(out, ")");
}

void FunDec::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "fundec(%s,\n", SymName(name));
  params->Print(out, d + 1);
  fprintf(out, ",\n");
  if (result) {
    Indent(out, d + 1);
    fprintf(out, "%s,\n", SymName(result));
  }
  body->Print(out, d + 1);
  fprintf(out, ")");
}

void NameAndTyList::Print(FILE *out, int d) const {
  Indent(out, d);
  if (nametys.empty()) {
    fprintf(out, "nameAndTyList()");
    return;
  }
  fprintf(out, "nameAndTyList(\n");
  auto it = nametys.begin();
  (*it)->Print(out, d + 1);
  fprintf(out, ",\n");
  NameAndTyList tail;
  tail.nametys.assign(std::next(it), nametys.end());
  tail.Print(out, d + 1);
  fprintf(out, ")");
}

void NameAndTy::Print(FILE *out, int d) const {
  Indent(out, d);
  fprintf(out, "nameAndTy(%s,\n", SymName(name));
  ty->Print(out, d + 1);
  fprintf(out, ")");
}

}  // namespace absyn
