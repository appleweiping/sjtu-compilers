#include "tiger/semant/semant.h"

#include <string>

#include "tiger/env/env.h"
#include "tiger/types/types.h"

// Semantic analysis / type-checking for Tiger (Appel §5). Errors are reported
// through ErrorMsg with the exact wording the course reference compiler uses.
namespace semant {

namespace {

using absyn::Pos;
using type::Ty;

err::ErrorMsg *EM = nullptr;

Ty *IntT() { return type::IntTy::Instance(); }
Ty *StrT() { return type::StringTy::Instance(); }
Ty *VoidT() { return type::VoidTy::Instance(); }
Ty *NilT() { return type::NilTy::Instance(); }

// Structural type equality after resolving NAME indirections. `nil` matches any
// record type; VOID matches VOID.
bool SameType(Ty *a, Ty *b) {
  if (!a || !b) return false;
  a = a->ActualTy();
  b = b->ActualTy();
  if (a == b) return true;
  // nil is assignable to any record.
  if (dynamic_cast<type::NilTy *>(a) && dynamic_cast<type::RecordTy *>(b))
    return true;
  if (dynamic_cast<type::NilTy *>(b) && dynamic_cast<type::RecordTy *>(a))
    return true;
  return false;
}

// Forward decls for the mutually-recursive checkers.
Ty *TransExp(env::VEnv *venv, env::TEnv *tenv, absyn::Exp *exp);
Ty *TransVar(env::VEnv *venv, env::TEnv *tenv, absyn::Var *var);
void TransDec(env::VEnv *venv, env::TEnv *tenv, absyn::Dec *dec);
Ty *TransTy(env::TEnv *tenv, absyn::Ty *ty);

// ------------------------------------------------------------------ variables
Ty *TransVar(env::VEnv *venv, env::TEnv *tenv, absyn::Var *var) {
  if (auto *v = dynamic_cast<absyn::SimpleVar *>(var)) {
    env::EnvEntry *e = venv->Look(v->sym);
    auto *ve = dynamic_cast<env::VarEntry *>(e);
    if (!ve) {
      EM->Error(v->pos, "undefined variable " + v->sym->Name());
      return IntT();
    }
    return ve->ty->ActualTy();
  }
  if (auto *v = dynamic_cast<absyn::FieldVar *>(var)) {
    Ty *base = TransVar(venv, tenv, v->var)->ActualTy();
    auto *rec = dynamic_cast<type::RecordTy *>(base);
    if (!rec) {
      EM->Error(v->pos, "not a record type");
      return IntT();
    }
    for (auto *f : rec->fields->fields)
      if (f->name == v->sym) return f->ty->ActualTy();
    EM->Error(v->pos, "field " + v->sym->Name() + " doesn't exist");
    return IntT();
  }
  if (auto *v = dynamic_cast<absyn::SubscriptVar *>(var)) {
    Ty *base = TransVar(venv, tenv, v->var)->ActualTy();
    auto *arr = dynamic_cast<type::ArrayTy *>(base);
    Ty *sub = TransExp(venv, tenv, v->subscript);
    if (!SameType(sub, IntT()))
      EM->Error(v->subscript->pos, "integer required");
    if (!arr) {
      EM->Error(v->pos, "array type required");
      return IntT();
    }
    return arr->ty->ActualTy();
  }
  return IntT();
}

// ---------------------------------------------------------------- expressions
Ty *TransExp(env::VEnv *venv, env::TEnv *tenv, absyn::Exp *exp) {
  if (auto *e = dynamic_cast<absyn::VarExp *>(exp))
    return TransVar(venv, tenv, e->var);
  if (dynamic_cast<absyn::NilExp *>(exp)) return NilT();
  if (dynamic_cast<absyn::IntExp *>(exp)) return IntT();
  if (dynamic_cast<absyn::StringExp *>(exp)) return StrT();
  if (dynamic_cast<absyn::VoidExp *>(exp)) return VoidT();

  if (auto *e = dynamic_cast<absyn::CallExp *>(exp)) {
    env::EnvEntry *entry = venv->Look(e->func);
    auto *fe = dynamic_cast<env::FunEntry *>(entry);
    if (!fe) {
      EM->Error(e->pos, "undefined function " + e->func->Name());
      return VoidT();
    }
    auto ait = e->args->exps.begin();
    auto fit = fe->formals->tys.begin();
    for (; ait != e->args->exps.end() && fit != fe->formals->tys.end();
         ++ait, ++fit) {
      Ty *at = TransExp(venv, tenv, *ait);
      if (!SameType(at, *fit))
        EM->Error((*ait)->pos, "para type mismatch");
    }
    if (ait != e->args->exps.end())
      EM->Error(e->pos, "too many params in function " + e->func->Name());
    else if (fit != fe->formals->tys.end())
      EM->Error(e->pos, "para type mismatch");
    return fe->result ? fe->result->ActualTy() : VoidT();
  }

  if (auto *e = dynamic_cast<absyn::OpExp *>(exp)) {
    Ty *lt = TransExp(venv, tenv, e->left);
    Ty *rt = TransExp(venv, tenv, e->right);
    switch (e->oper) {
      case absyn::PLUS_OP:
      case absyn::MINUS_OP:
      case absyn::TIMES_OP:
      case absyn::DIVIDE_OP:
        if (!SameType(lt, IntT())) EM->Error(e->left->pos, "integer required");
        if (!SameType(rt, IntT())) EM->Error(e->right->pos, "integer required");
        return IntT();
      case absyn::EQ_OP:
      case absyn::NEQ_OP:
      case absyn::LT_OP:
      case absyn::LE_OP:
      case absyn::GT_OP:
      case absyn::GE_OP:
        if (!SameType(lt, rt)) EM->Error(e->pos, "same type required");
        return IntT();
    }
    return IntT();
  }

  if (auto *e = dynamic_cast<absyn::RecordExp *>(exp)) {
    Ty *t = tenv->Look(e->typ);
    if (!t) {
      EM->Error(e->pos, "undefined type " + e->typ->Name());
      return VoidT();
    }
    auto *rec = dynamic_cast<type::RecordTy *>(t->ActualTy());
    if (!rec) {
      EM->Error(e->pos, "not a record type");
      return t;
    }
    auto eit = e->fields->efields.begin();
    auto fit = rec->fields->fields.begin();
    for (; eit != e->fields->efields.end() && fit != rec->fields->fields.end();
         ++eit, ++fit) {
      Ty *ft = TransExp(venv, tenv, (*eit)->exp);
      if (!SameType(ft, (*fit)->ty))
        EM->Error((*eit)->exp->pos, "type mismatch");
    }
    return t;
  }

  if (auto *e = dynamic_cast<absyn::SeqExp *>(exp)) {
    Ty *t = VoidT();
    for (auto *sub : e->seq->exps) t = TransExp(venv, tenv, sub);
    return t;
  }

  if (auto *e = dynamic_cast<absyn::AssignExp *>(exp)) {
    // A for-loop variable is read-only.
    if (auto *sv = dynamic_cast<absyn::SimpleVar *>(e->var)) {
      auto *ve = dynamic_cast<env::VarEntry *>(venv->Look(sv->sym));
      if (ve && ve->readonly)
        EM->Error(e->pos, "loop variable can't be assigned");
    }
    Ty *vt = TransVar(venv, tenv, e->var);
    Ty *et = TransExp(venv, tenv, e->exp);
    if (!SameType(vt, et)) EM->Error(e->pos, "unmatched assign exp");
    return VoidT();
  }

  if (auto *e = dynamic_cast<absyn::IfExp *>(exp)) {
    TransExp(venv, tenv, e->test);
    Ty *tt = TransExp(venv, tenv, e->then);
    if (e->elsee) {
      Ty *et = TransExp(venv, tenv, e->elsee);
      if (!SameType(tt, et))
        EM->Error(e->pos, "then exp and else exp type mismatch");
      return tt;
    }
    if (!SameType(tt, VoidT()))
      EM->Error(e->pos, "if-then exp's body must produce no value");
    return VoidT();
  }

  if (auto *e = dynamic_cast<absyn::WhileExp *>(exp)) {
    TransExp(venv, tenv, e->test);
    Ty *bt = TransExp(venv, tenv, e->body);
    if (!SameType(bt, VoidT()))
      EM->Error(e->body->pos, "while body must produce no value");
    return VoidT();
  }

  if (auto *e = dynamic_cast<absyn::ForExp *>(exp)) {
    Ty *lo = TransExp(venv, tenv, e->lo);
    Ty *hi = TransExp(venv, tenv, e->hi);
    if (!SameType(lo, IntT()) || !SameType(hi, IntT()))
      EM->Error(e->lo->pos, "for exp's range type is not integer");
    venv->BeginScope();
    venv->Enter(e->var, new env::VarEntry(IntT(), /*readonly=*/true));
    TransExp(venv, tenv, e->body);
    venv->EndScope();
    return VoidT();
  }

  if (dynamic_cast<absyn::BreakExp *>(exp)) return VoidT();

  if (auto *e = dynamic_cast<absyn::LetExp *>(exp)) {
    venv->BeginScope();
    tenv->BeginScope();
    for (auto *d : e->decs->decs) TransDec(venv, tenv, d);
    Ty *t = TransExp(venv, tenv, e->body);
    tenv->EndScope();
    venv->EndScope();
    return t;
  }

  if (auto *e = dynamic_cast<absyn::ArrayExp *>(exp)) {
    Ty *t = tenv->Look(e->typ);
    if (!t) {
      EM->Error(e->pos, "undefined type " + e->typ->Name());
      return VoidT();
    }
    auto *arr = dynamic_cast<type::ArrayTy *>(t->ActualTy());
    if (!arr) {
      EM->Error(e->pos, "array type required");
      return t;
    }
    Ty *st = TransExp(venv, tenv, e->size);
    if (!SameType(st, IntT())) EM->Error(e->size->pos, "integer required");
    Ty *it = TransExp(venv, tenv, e->init);
    if (!SameType(it, arr->ty)) EM->Error(e->init->pos, "type mismatch");
    return t;
  }

  return VoidT();
}

// ------------------------------------------------------------------ type exprs
Ty *TransTy(env::TEnv *tenv, absyn::Ty *ty) {
  if (auto *t = dynamic_cast<absyn::NameTy *>(ty)) {
    Ty *named = tenv->Look(t->name);
    if (!named) {
      EM->Error(t->pos, " undefined type " + t->name->Name());
      return IntT();
    }
    return named;
  }
  if (auto *t = dynamic_cast<absyn::RecordTy *>(ty)) {
    auto *fl = new type::FieldList();
    for (auto *f : t->record->fields) {
      Ty *ft = tenv->Look(f->typ);
      if (!ft) {
        EM->Error(f->pos, " undefined type " + f->typ->Name());
        ft = IntT();
      }
      fl->fields.push_back(new type::Field(f->name, ft));
    }
    return new type::RecordTy(fl);
  }
  if (auto *t = dynamic_cast<absyn::ArrayTy *>(ty)) {
    Ty *et = tenv->Look(t->array);
    if (!et) {
      EM->Error(t->pos, " undefined type " + t->array->Name());
      et = IntT();
    }
    return new type::ArrayTy(et);
  }
  return IntT();
}

// ---------------------------------------------------------------- declarations
void TransDec(env::VEnv *venv, env::TEnv *tenv, absyn::Dec *dec) {
  if (auto *d = dynamic_cast<absyn::VarDec *>(dec)) {
    Ty *initT = TransExp(venv, tenv, d->init);
    if (d->typ) {
      Ty *declared = tenv->Look(d->typ);
      if (!declared) {
        EM->Error(d->pos, "undefined type " + d->typ->Name());
        declared = initT;
      } else if (!SameType(declared, initT)) {
        EM->Error(d->init->pos, "type mismatch");
      }
      venv->Enter(d->var, new env::VarEntry(declared));
    } else {
      if (dynamic_cast<type::NilTy *>(initT->ActualTy()))
        EM->Error(d->pos, "init should not be nil without type specified");
      venv->Enter(d->var, new env::VarEntry(initT));
    }
    return;
  }

  if (auto *d = dynamic_cast<absyn::TypeDec *>(dec)) {
    // Pass 1: enter empty NAME types so recursion resolves.
    for (auto *nt : d->types->nametys) {
      if (tenv->Look(nt->name)) {
        // Redeclaration inside the same batch.
        bool dup = false;
        for (auto *other : d->types->nametys) {
          if (other == nt) break;
          if (other->name == nt->name) dup = true;
        }
        if (dup) EM->Error(d->pos, "two types have the same name");
      }
      tenv->Enter(nt->name, new type::NameTy(nt->name, nullptr));
    }
    // Pass 2: resolve each body.
    for (auto *nt : d->types->nametys) {
      auto *placeholder = dynamic_cast<type::NameTy *>(tenv->Look(nt->name));
      Ty *resolved = TransTy(tenv, nt->ty);
      if (placeholder) placeholder->ty = resolved;
    }
    // Pass 3: detect illegal cycles (a NAME chain with no record/array).
    for (auto *nt : d->types->nametys) {
      auto *cur = dynamic_cast<type::NameTy *>(tenv->Look(nt->name));
      type::NameTy *slow = cur, *fast = cur;
      while (fast && fast->ty &&
             dynamic_cast<type::NameTy *>(fast->ty)) {
        fast = dynamic_cast<type::NameTy *>(fast->ty);
        if (fast && fast->ty && dynamic_cast<type::NameTy *>(fast->ty))
          fast = dynamic_cast<type::NameTy *>(fast->ty);
        else
          break;
        slow = dynamic_cast<type::NameTy *>(slow->ty);
        if (slow == fast) {
          EM->Error(d->pos, "illegal type cycle");
          break;
        }
      }
    }
    return;
  }

  if (auto *d = dynamic_cast<absyn::FunctionDec *>(dec)) {
    // Pass 1: enter each function header.
    for (auto *f : d->functions->fundecs) {
      for (auto *other : d->functions->fundecs) {
        if (other == f) break;
        if (other->name == f->name)
          EM->Error(f->pos, "two functions have the same name");
      }
      auto *formals = new type::TyList();
      for (auto *p : f->params->fields) {
        Ty *pt = tenv->Look(p->typ);
        if (!pt) {
          EM->Error(p->pos, " undefined type " + p->typ->Name());
          pt = IntT();
        }
        formals->tys.push_back(pt);
      }
      Ty *result = VoidT();
      if (f->result) {
        result = tenv->Look(f->result);
        if (!result) {
          EM->Error(f->pos, "undefined type " + f->result->Name());
          result = VoidT();
        }
      }
      venv->Enter(f->name, new env::FunEntry(formals, result));
    }
    // Pass 2: check each body against its declared result type.
    for (auto *f : d->functions->fundecs) {
      auto *fe = dynamic_cast<env::FunEntry *>(venv->Look(f->name));
      venv->BeginScope();
      auto fit = fe->formals->tys.begin();
      for (auto *p : f->params->fields) {
        venv->Enter(p->name, new env::VarEntry(*fit));
        ++fit;
      }
      Ty *bodyT = TransExp(venv, tenv, f->body);
      venv->EndScope();
      if (dynamic_cast<type::VoidTy *>(fe->result)) {
        if (!SameType(bodyT, VoidT()))
          EM->Error(f->body->pos, "procedure returns value");
      } else if (!SameType(bodyT, fe->result)) {
        EM->Error(f->body->pos, "type mismatch");
      }
    }
    return;
  }
}

}  // namespace

void Analyze(absyn::Exp *exp, err::ErrorMsg *em) {
  EM = em;
  auto *venv = new env::VEnv();
  auto *tenv = new env::TEnv();
  venv->BeginScope();
  tenv->BeginScope();
  env::FillBaseTEnv(tenv);
  env::FillBaseVEnv(venv);
  TransExp(venv, tenv, exp);
  tenv->EndScope();
  venv->EndScope();
}

}  // namespace semant
