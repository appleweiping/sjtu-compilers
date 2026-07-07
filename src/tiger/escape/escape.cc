#include "tiger/escape/escape.h"

#include "tiger/symbol/symbol.h"

// Escape analysis (Appel §6.2). A variable "escapes" if it is used at a nesting
// depth greater than the depth at which it was declared; such variables must
// live in the stack frame rather than in a register. We walk the AST tracking
// the current function-nesting depth and, for each variable, the depth and the
// escape flag to update.
namespace escape {

namespace {

struct EscapeEntry {
  int depth;
  bool *escape;  // points at the AST node's escape flag
};

using EEnv = sym::Table<EscapeEntry>;

void TraverseExp(EEnv *env, int depth, absyn::Exp *exp);
void TraverseVar(EEnv *env, int depth, absyn::Var *var);
void TraverseDec(EEnv *env, int depth, absyn::Dec *dec);

void TraverseVar(EEnv *env, int depth, absyn::Var *var) {
  if (auto *v = dynamic_cast<absyn::SimpleVar *>(var)) {
    EscapeEntry *e = env->Look(v->sym);
    if (e && depth > e->depth) *(e->escape) = true;
    return;
  }
  if (auto *v = dynamic_cast<absyn::FieldVar *>(var)) {
    TraverseVar(env, depth, v->var);
    return;
  }
  if (auto *v = dynamic_cast<absyn::SubscriptVar *>(var)) {
    TraverseVar(env, depth, v->var);
    TraverseExp(env, depth, v->subscript);
    return;
  }
}

void TraverseExp(EEnv *env, int depth, absyn::Exp *exp) {
  if (!exp) return;
  if (auto *e = dynamic_cast<absyn::VarExp *>(exp)) {
    TraverseVar(env, depth, e->var);
  } else if (auto *e = dynamic_cast<absyn::CallExp *>(exp)) {
    for (auto *a : e->args->exps) TraverseExp(env, depth, a);
  } else if (auto *e = dynamic_cast<absyn::OpExp *>(exp)) {
    TraverseExp(env, depth, e->left);
    TraverseExp(env, depth, e->right);
  } else if (auto *e = dynamic_cast<absyn::RecordExp *>(exp)) {
    for (auto *f : e->fields->efields) TraverseExp(env, depth, f->exp);
  } else if (auto *e = dynamic_cast<absyn::SeqExp *>(exp)) {
    for (auto *s : e->seq->exps) TraverseExp(env, depth, s);
  } else if (auto *e = dynamic_cast<absyn::AssignExp *>(exp)) {
    TraverseVar(env, depth, e->var);
    TraverseExp(env, depth, e->exp);
  } else if (auto *e = dynamic_cast<absyn::IfExp *>(exp)) {
    TraverseExp(env, depth, e->test);
    TraverseExp(env, depth, e->then);
    TraverseExp(env, depth, e->elsee);
  } else if (auto *e = dynamic_cast<absyn::WhileExp *>(exp)) {
    TraverseExp(env, depth, e->test);
    TraverseExp(env, depth, e->body);
  } else if (auto *e = dynamic_cast<absyn::ForExp *>(exp)) {
    TraverseExp(env, depth, e->lo);
    TraverseExp(env, depth, e->hi);
    env->BeginScope();
    e->escape = false;
    env->Enter(e->var, new EscapeEntry{depth, &e->escape});
    TraverseExp(env, depth, e->body);
    env->EndScope();
  } else if (auto *e = dynamic_cast<absyn::LetExp *>(exp)) {
    env->BeginScope();
    for (auto *d : e->decs->decs) TraverseDec(env, depth, d);
    TraverseExp(env, depth, e->body);
    env->EndScope();
  } else if (auto *e = dynamic_cast<absyn::ArrayExp *>(exp)) {
    TraverseExp(env, depth, e->size);
    TraverseExp(env, depth, e->init);
  }
  // NilExp, IntExp, StringExp, VoidExp, BreakExp: no variables.
}

void TraverseDec(EEnv *env, int depth, absyn::Dec *dec) {
  if (auto *d = dynamic_cast<absyn::VarDec *>(dec)) {
    TraverseExp(env, depth, d->init);
    d->escape = false;
    env->Enter(d->var, new EscapeEntry{depth, &d->escape});
    return;
  }
  if (auto *d = dynamic_cast<absyn::FunctionDec *>(dec)) {
    for (auto *f : d->functions->fundecs) {
      env->BeginScope();
      for (auto *p : f->params->fields) {
        p->escape = false;
        env->Enter(p->name, new EscapeEntry{depth + 1, &p->escape});
      }
      TraverseExp(env, depth + 1, f->body);
      env->EndScope();
    }
    return;
  }
  // TypeDec introduces no variables.
}

}  // namespace

void FindEscape(absyn::Exp *exp) {
  auto *env = new EEnv();
  env->BeginScope();
  TraverseExp(env, /*depth=*/0, exp);
  env->EndScope();
}

}  // namespace escape
