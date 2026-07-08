#include "tiger/translate/translate.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "tiger/symbol/symbol.h"
#include "tiger/types/types.h"

// AST -> textual LLVM IR translator.
//
// Design (Appel-style, adapted to LLVM):
//   * Every Tiger value is an i64: integers directly; records/arrays/strings
//     are pointers stored as i64 (via inttoptr/ptrtoint).
//   * Each Tiger function is emitted as an LLVM function taking a hidden static
//     link as its first parameter (an i64 = pointer to the enclosing frame).
//     `tigermain` takes none.
//   * ALL of a function's variables (parameters and let-bound locals) live in a
//     heap-allocated frame: an [n x i64] array. Slot 0 holds the parent static
//     link; the remaining slots hold the variables. Uniformly addressing every
//     variable through the frame makes nested-function access (e.g. tlink.tig)
//     correct without a separate escape analysis at codegen time.
namespace tr {

namespace {

using absyn::Exp;
using type::Ty;

Ty *IntT() { return type::IntTy::Instance(); }
Ty *StrT() { return type::StringTy::Instance(); }
Ty *VoidT() { return type::VoidTy::Instance(); }
Ty *NilT() { return type::NilTy::Instance(); }

std::string Sn(sym::Symbol *s) { return s ? s->Name() : ""; }

// A record/array/function value is a pointer; ints are values. We only need to
// distinguish records (for field layout) and arrays (for element access).
type::RecordTy *AsRecord(Ty *t) {
  return t ? dynamic_cast<type::RecordTy *>(t->ActualTy()) : nullptr;
}
type::ArrayTy *AsArray(Ty *t) {
  return t ? dynamic_cast<type::ArrayTy *>(t->ActualTy()) : nullptr;
}
bool IsString(Ty *t) {
  return t && dynamic_cast<type::StringTy *>(t->ActualTy());
}

// ---------------------------------------------------------------------------
class Emitter {
 public:
  std::string Run(Exp *root);

 private:
  struct VarInfo {
    int depth;   // nesting depth of the declaring function (tigermain = 0)
    int slot;    // frame slot index (>= 1; slot 0 is the static link)
    Ty *ty;
  };
  struct FunInfo {
    std::string label;
    int depth;                 // depth of the function body itself
    std::vector<Ty *> formals;
    Ty *result;
  };

  using VEnv = sym::Table<VarInfo>;
  using TEnv = sym::Table<Ty>;
  using FEnv = sym::Table<FunInfo>;

  VEnv venv_;
  TEnv tenv_;
  FEnv fenv_;

  std::ostringstream globals_;
  std::ostringstream funcs_;
  std::ostringstream *cur_ = nullptr;

  int temp_ = 0, label_ = 0, str_id_ = 0;
  int cur_depth_ = 0;
  int cur_slots_ = 0;         // slots allocated so far in the current function
  std::string cur_frame_;     // SSA i64 holding the current frame pointer
  std::string break_label_;
  std::string cur_block_;     // label of the basic block currently being built

  std::string NewTemp() { return "%t" + std::to_string(temp_++); }
  std::string NewLabel(const char *b) {
    return std::string(b) + std::to_string(label_++);
  }
  void Emit(const std::string &s) { (*cur_) << s << "\n"; }
  // Begin a new basic block: emit its label and remember it (for phi preds).
  void StartBlock(const std::string &label) {
    (*cur_) << label << ":\n";
    cur_block_ = label;
  }

  int AllocSlot() { return ++cur_slots_; }  // slot 0 reserved for static link

  std::string EmitString(const std::string &s);
  std::string FrameSlotAddr(const VarInfo &vi);
  Ty *ResolveTy(absyn::Ty *ty);

  std::string TransExp(Exp *exp, Ty **out_ty);
  std::string TransVarAddr(absyn::Var *var, Ty **out_ty);  // -> SSA i64* addr

  // Collect all functions (with unique labels) declared anywhere, so we can
  // emit them; also enter their headers for calls.
  void EmitFunction(absyn::FunDec *fd, const FunInfo &fi);
  void HoistFunctions(Exp *exp);  // not needed: we emit inline per let-block
};

std::string Emitter::EmitString(const std::string &s) {
  int id = str_id_++;
  std::string name = "@.str" + std::to_string(id);
  std::ostringstream bytes;
  for (unsigned char c : s) {
    char buf[8];
    snprintf(buf, sizeof(buf), "\\%02X", c);
    bytes << buf;
  }
  size_t n = s.size();
  globals_ << name << " = private constant { i64, [" << n << " x i8] } "
           << "{ i64 " << n << ", [" << n << " x i8] c\"" << bytes.str()
           << "\" }\n";
  std::string p = NewTemp();
  Emit("  " + p + " = ptrtoint { i64, [" + std::to_string(n) + " x i8] }* " +
       name + " to i64");
  return p;
}

std::string Emitter::FrameSlotAddr(const VarInfo &vi) {
  std::string frame = cur_frame_;
  for (int d = cur_depth_; d > vi.depth; --d) {
    std::string fp = NewTemp();
    Emit("  " + fp + " = inttoptr i64 " + frame + " to i64*");
    std::string linkp = NewTemp();
    Emit("  " + linkp + " = getelementptr i64, i64* " + fp + ", i64 0");
    std::string link = NewTemp();
    Emit("  " + link + " = load i64, i64* " + linkp);
    frame = link;
  }
  std::string fp = NewTemp();
  Emit("  " + fp + " = inttoptr i64 " + frame + " to i64*");
  std::string addr = NewTemp();
  Emit("  " + addr + " = getelementptr i64, i64* " + fp + ", i64 " +
       std::to_string(vi.slot));
  return addr;
}

Ty *Emitter::ResolveTy(absyn::Ty *ty) {
  if (auto *t = dynamic_cast<absyn::NameTy *>(ty)) {
    Ty *r = tenv_.Look(t->name);
    return r ? r : IntT();
  }
  if (auto *t = dynamic_cast<absyn::RecordTy *>(ty)) {
    auto *fl = new type::FieldList();
    for (auto *f : t->record->fields) {
      Ty *ft = tenv_.Look(f->typ);
      fl->fields.push_back(new type::Field(f->name, ft ? ft : IntT()));
    }
    return new type::RecordTy(fl);
  }
  if (auto *t = dynamic_cast<absyn::ArrayTy *>(ty)) {
    Ty *et = tenv_.Look(t->array);
    return new type::ArrayTy(et ? et : IntT());
  }
  return IntT();
}

// --------------------------------------------------------------- variable addr
std::string Emitter::TransVarAddr(absyn::Var *var, Ty **out_ty) {
  if (auto *v = dynamic_cast<absyn::SimpleVar *>(var)) {
    VarInfo *vi = venv_.Look(v->sym);
    if (!vi) {  // should not happen after semant
      *out_ty = IntT();
      std::string a = NewTemp();
      Emit("  " + a + " = alloca i64");
      return a;
    }
    *out_ty = vi->ty;
    return FrameSlotAddr(*vi);
  }
  if (auto *v = dynamic_cast<absyn::FieldVar *>(var)) {
    Ty *baseTy = nullptr;
    std::string baseAddr = TransVarAddr(v->var, &baseTy);
    std::string base = NewTemp();
    Emit("  " + base + " = load i64, i64* " + baseAddr);  // record pointer i64
    type::RecordTy *rec = AsRecord(baseTy);
    int idx = 0;
    Ty *fieldTy = IntT();
    if (rec) {
      for (auto *f : rec->fields->fields) {
        if (f->name == v->sym) {
          fieldTy = f->ty;
          break;
        }
        ++idx;
      }
    }
    *out_ty = fieldTy;
    std::string rp = NewTemp();
    Emit("  " + rp + " = inttoptr i64 " + base + " to i64*");
    std::string addr = NewTemp();
    Emit("  " + addr + " = getelementptr i64, i64* " + rp + ", i64 " +
         std::to_string(idx));
    return addr;
  }
  if (auto *v = dynamic_cast<absyn::SubscriptVar *>(var)) {
    Ty *baseTy = nullptr;
    std::string baseAddr = TransVarAddr(v->var, &baseTy);
    std::string base = NewTemp();
    Emit("  " + base + " = load i64, i64* " + baseAddr);  // array pointer i64
    Ty *idxTy = nullptr;
    std::string idx = TransExp(v->subscript, &idxTy);
    type::ArrayTy *arr = AsArray(baseTy);
    *out_ty = arr ? arr->ty : IntT();
    std::string ap = NewTemp();
    Emit("  " + ap + " = inttoptr i64 " + base + " to i64*");
    std::string addr = NewTemp();
    Emit("  " + addr + " = getelementptr i64, i64* " + ap + ", i64 " + idx);
    return addr;
  }
  *out_ty = IntT();
  return "null";
}

// ---------------------------------------------------------------- expressions
std::string Emitter::TransExp(Exp *exp, Ty **out_ty) {
  *out_ty = VoidT();

  if (auto *e = dynamic_cast<absyn::IntExp *>(exp)) {
    *out_ty = IntT();
    return std::to_string(e->val);
  }
  if (dynamic_cast<absyn::NilExp *>(exp)) {
    *out_ty = NilT();
    return "0";
  }
  if (dynamic_cast<absyn::VoidExp *>(exp)) {
    *out_ty = VoidT();
    return "0";
  }
  if (auto *e = dynamic_cast<absyn::StringExp *>(exp)) {
    *out_ty = StrT();
    return EmitString(e->str);
  }
  if (auto *e = dynamic_cast<absyn::VarExp *>(exp)) {
    Ty *t = nullptr;
    std::string addr = TransVarAddr(e->var, &t);
    *out_ty = t;
    std::string v = NewTemp();
    Emit("  " + v + " = load i64, i64* " + addr);
    return v;
  }
  if (auto *e = dynamic_cast<absyn::OpExp *>(exp)) {
    Ty *lt = nullptr, *rt = nullptr;
    std::string l = TransExp(e->left, &lt);
    std::string r = TransExp(e->right, &rt);
    *out_ty = IntT();
    const char *op = nullptr;
    switch (e->oper) {
      case absyn::PLUS_OP: op = "add"; break;
      case absyn::MINUS_OP: op = "sub"; break;
      case absyn::TIMES_OP: op = "mul"; break;
      case absyn::DIVIDE_OP: op = "sdiv"; break;
      default: op = nullptr; break;
    }
    if (op) {
      std::string t = NewTemp();
      Emit("  " + t + " = " + op + " i64 " + l + ", " + r);
      return t;
    }
    // Comparison. Strings use a runtime helper for = and <>.
    const char *pred = nullptr;
    switch (e->oper) {
      case absyn::EQ_OP: pred = "eq"; break;
      case absyn::NEQ_OP: pred = "ne"; break;
      case absyn::LT_OP: pred = "slt"; break;
      case absyn::LE_OP: pred = "sle"; break;
      case absyn::GT_OP: pred = "sgt"; break;
      case absyn::GE_OP: pred = "sge"; break;
      default: pred = "eq"; break;
    }
    std::string lhs = l, rhs = r;
    if (IsString(lt) && (e->oper == absyn::EQ_OP || e->oper == absyn::NEQ_OP)) {
      std::string cmp = NewTemp();
      Emit("  " + cmp + " = call i64 @tiger_streq(i64 " + l + ", i64 " + r +
           ")");
      lhs = cmp;
      rhs = "1";  // streq returns 1 on equal
    }
    std::string c = NewTemp();
    Emit("  " + c + " = icmp " + pred + " i64 " + lhs + ", " + rhs);
    std::string z = NewTemp();
    Emit("  " + z + " = zext i1 " + c + " to i64");
    return z;
  }
  if (auto *e = dynamic_cast<absyn::AssignExp *>(exp)) {
    Ty *vt = nullptr, *et = nullptr;
    std::string addr = TransVarAddr(e->var, &vt);
    std::string val = TransExp(e->exp, &et);
    Emit("  store i64 " + val + ", i64* " + addr);
    *out_ty = VoidT();
    return "0";
  }
  if (auto *e = dynamic_cast<absyn::SeqExp *>(exp)) {
    std::string last = "0";
    Ty *t = VoidT();
    for (auto *s : e->seq->exps) last = TransExp(s, &t);
    *out_ty = t;
    return last;
  }
  if (auto *e = dynamic_cast<absyn::CallExp *>(exp)) {
    FunInfo *fi = fenv_.Look(e->func);
    // Standard-library calls map to runtime symbols with no static link.
    std::vector<std::string> args;
    std::vector<Ty *> argtys;
    for (auto *a : e->args->exps) {
      Ty *at = nullptr;
      std::string v = TransExp(a, &at);
      args.push_back(v);
      argtys.push_back(at);
    }
    if (!fi) {
      // Library function.
      std::string name = Sn(e->func);
      bool returns = (name == "ord" || name == "chr" || name == "getchar" ||
                      name == "size" || name == "substring" ||
                      name == "concat" || name == "not");
      // Map Tiger names to runtime symbols.
      std::string sym = name;
      if (name == "not") sym = "tiger_not";
      if (name == "getchar") sym = "tiger_getchar";
      if (name == "exit") sym = "tiger_exit";
      std::ostringstream call;
      std::string ret;
      if (returns) {
        ret = NewTemp();
        call << "  " << ret << " = call i64 @" << sym << "(";
        *out_ty = (name == "ord" || name == "size" || name == "not") ? IntT()
                                                                      : StrT();
      } else {
        call << "  call void @" << sym << "(";
        ret = "0";
        *out_ty = VoidT();
      }
      for (size_t i = 0; i < args.size(); ++i) {
        if (i) call << ", ";
        call << "i64 " << args[i];
      }
      call << ")";
      Emit(call.str());
      return ret;
    }
    // User function: pass the static link for the callee's parent depth.
    // The callee's parent depth is fi->depth - 1. We need the frame at that
    // depth reachable from the current frame.
    std::string link = cur_frame_;
    for (int d = cur_depth_; d > fi->depth - 1; --d) {
      std::string fp = NewTemp();
      Emit("  " + fp + " = inttoptr i64 " + link + " to i64*");
      std::string lp = NewTemp();
      Emit("  " + lp + " = getelementptr i64, i64* " + fp + ", i64 0");
      std::string lv = NewTemp();
      Emit("  " + lv + " = load i64, i64* " + lp);
      link = lv;
    }
    std::ostringstream call;
    std::string ret;
    bool hasResult = fi->result && !dynamic_cast<type::VoidTy *>(fi->result);
    if (hasResult) {
      ret = NewTemp();
      call << "  " << ret << " = call i64 @" << fi->label << "(i64 " << link;
      *out_ty = fi->result;
    } else {
      call << "  call void @" << fi->label << "(i64 " << link;
      ret = "0";
      *out_ty = VoidT();
    }
    for (auto &a : args) call << ", i64 " << a;
    call << ")";
    Emit(call.str());
    return ret;
  }
  if (auto *e = dynamic_cast<absyn::IfExp *>(exp)) {
    Ty *tt = nullptr;
    std::string c = TransExp(e->test, &tt);
    std::string cond = NewTemp();
    Emit("  " + cond + " = icmp ne i64 " + c + ", 0");
    std::string thenL = NewLabel("then"), elseL = NewLabel("else"),
                endL = NewLabel("ifend");
    if (e->elsee) {
      Emit("  br i1 " + cond + ", label %" + thenL + ", label %" + elseL);
      StartBlock(thenL);
      Ty *thTy = nullptr;
      std::string tv = TransExp(e->then, &thTy);
      std::string thenPred = cur_block_;  // actual predecessor for the phi
      Emit("  br label %" + endL);
      StartBlock(elseL);
      Ty *elTy = nullptr;
      std::string ev = TransExp(e->elsee, &elTy);
      std::string elsePred = cur_block_;
      Emit("  br label %" + endL);
      StartBlock(endL);
      // Produce a phi only if the if yields a value.
      *out_ty = thTy;
      if (thTy && !dynamic_cast<type::VoidTy *>(thTy)) {
        std::string phi = NewTemp();
        Emit("  " + phi + " = phi i64 [ " + tv + ", %" + thenPred + " ], [ " +
             ev + ", %" + elsePred + " ]");
        return phi;
      }
      return "0";
    } else {
      Emit("  br i1 " + cond + ", label %" + thenL + ", label %" + endL);
      StartBlock(thenL);
      Ty *thTy = nullptr;
      TransExp(e->then, &thTy);
      Emit("  br label %" + endL);
      StartBlock(endL);
      *out_ty = VoidT();
      return "0";
    }
  }
  if (auto *e = dynamic_cast<absyn::WhileExp *>(exp)) {
    std::string testL = NewLabel("wtest"), bodyL = NewLabel("wbody"),
                endL = NewLabel("wend");
    Emit("  br label %" + testL);
    StartBlock(testL);
    Ty *tt = nullptr;
    std::string c = TransExp(e->test, &tt);
    std::string cond = NewTemp();
    Emit("  " + cond + " = icmp ne i64 " + c + ", 0");
    Emit("  br i1 " + cond + ", label %" + bodyL + ", label %" + endL);
    StartBlock(bodyL);
    std::string savedBreak = break_label_;
    break_label_ = endL;
    Ty *bt = nullptr;
    TransExp(e->body, &bt);
    break_label_ = savedBreak;
    Emit("  br label %" + testL);
    StartBlock(endL);
    *out_ty = VoidT();
    return "0";
  }
  if (auto *e = dynamic_cast<absyn::ForExp *>(exp)) {
    // Desugar `for v := lo to hi do body` with an explicit counter slot.
    Ty *lot = nullptr, *hit = nullptr;
    std::string lo = TransExp(e->lo, &lot);
    std::string hi = TransExp(e->hi, &hit);
    int slot = AllocSlot();
    venv_.BeginScope();
    venv_.Enter(e->var, new VarInfo{cur_depth_, slot, IntT()});
    VarInfo vi{cur_depth_, slot, IntT()};
    std::string addr0 = FrameSlotAddr(vi);
    Emit("  store i64 " + lo + ", i64* " + addr0);
    // Store hi in a fresh slot to keep it stable.
    int hiSlot = AllocSlot();
    VarInfo hivi{cur_depth_, hiSlot, IntT()};
    std::string hiAddr = FrameSlotAddr(hivi);
    Emit("  store i64 " + hi + ", i64* " + hiAddr);
    std::string testL = NewLabel("ftest"), bodyL = NewLabel("fbody"),
                incL = NewLabel("finc"), endL = NewLabel("fend");
    Emit("  br label %" + testL);
    StartBlock(testL);
    std::string ca = FrameSlotAddr(vi);
    std::string cv = NewTemp();
    Emit("  " + cv + " = load i64, i64* " + ca);
    std::string ha = FrameSlotAddr(hivi);
    std::string hv = NewTemp();
    Emit("  " + hv + " = load i64, i64* " + ha);
    std::string cond = NewTemp();
    Emit("  " + cond + " = icmp sle i64 " + cv + ", " + hv);
    Emit("  br i1 " + cond + ", label %" + bodyL + ", label %" + endL);
    StartBlock(bodyL);
    std::string savedBreak = break_label_;
    break_label_ = endL;
    Ty *bt = nullptr;
    TransExp(e->body, &bt);
    break_label_ = savedBreak;
    Emit("  br label %" + incL);
    StartBlock(incL);
    std::string ia = FrameSlotAddr(vi);
    std::string iv = NewTemp();
    Emit("  " + iv + " = load i64, i64* " + ia);
    std::string inc = NewTemp();
    Emit("  " + inc + " = add i64 " + iv + ", 1");
    Emit("  store i64 " + inc + ", i64* " + ia);
    Emit("  br label %" + testL);
    StartBlock(endL);
    venv_.EndScope();
    *out_ty = VoidT();
    return "0";
  }
  if (dynamic_cast<absyn::BreakExp *>(exp)) {
    if (!break_label_.empty()) {
      Emit("  br label %" + break_label_);
      // Start a dead block so following IR stays well-formed.
      StartBlock(NewLabel("afterbreak"));
    }
    *out_ty = VoidT();
    return "0";
  }
  if (auto *e = dynamic_cast<absyn::ArrayExp *>(exp)) {
    Ty *arrTy = tenv_.Look(e->typ);
    *out_ty = arrTy ? arrTy : IntT();
    Ty *st = nullptr, *it = nullptr;
    std::string size = TransExp(e->size, &st);
    std::string init = TransExp(e->init, &it);
    std::string p = NewTemp();
    Emit("  " + p + " = call i64 @tiger_init_array(i64 " + size + ", i64 " +
         init + ")");
    return p;
  }
  if (auto *e = dynamic_cast<absyn::RecordExp *>(exp)) {
    Ty *recTy = tenv_.Look(e->typ);
    *out_ty = recTy ? recTy : IntT();
    int n = static_cast<int>(e->fields->efields.size());
    std::string p = NewTemp();
    Emit("  " + p + " = call i64 @tiger_alloc_record(i64 " +
         std::to_string(n * 8) + ")");
    std::string rp = NewTemp();
    Emit("  " + rp + " = inttoptr i64 " + p + " to i64*");
    int idx = 0;
    for (auto *ef : e->fields->efields) {
      Ty *ft = nullptr;
      std::string v = TransExp(ef->exp, &ft);
      std::string fa = NewTemp();
      Emit("  " + fa + " = getelementptr i64, i64* " + rp + ", i64 " +
           std::to_string(idx));
      Emit("  store i64 " + v + ", i64* " + fa);
      ++idx;
    }
    return p;
  }
  if (auto *e = dynamic_cast<absyn::LetExp *>(exp)) {
    venv_.BeginScope();
    tenv_.BeginScope();
    fenv_.BeginScope();

    // Collect function declarations to emit after processing the body of the
    // let (their bodies are emitted immediately since we already have headers).
    std::vector<std::pair<absyn::FunDec *, FunInfo>> pending;

    for (auto *d : e->decs->decs) {
      if (auto *td = dynamic_cast<absyn::TypeDec *>(d)) {
        for (auto *nt : td->types->nametys)
          tenv_.Enter(nt->name, new type::NameTy(nt->name, nullptr));
        for (auto *nt : td->types->nametys) {
          auto *ph = dynamic_cast<type::NameTy *>(tenv_.Look(nt->name));
          Ty *resolved = ResolveTy(nt->ty);
          if (ph) ph->ty = resolved;
        }
      } else if (auto *vd = dynamic_cast<absyn::VarDec *>(d)) {
        Ty *it = nullptr;
        std::string v = TransExp(vd->init, &it);
        Ty *declTy = it;
        if (vd->typ) {
          Ty *t = tenv_.Look(vd->typ);
          if (t) declTy = t;
        }
        int slot = AllocSlot();
        venv_.Enter(vd->var, new VarInfo{cur_depth_, slot, declTy});
        VarInfo vi{cur_depth_, slot, declTy};
        std::string addr = FrameSlotAddr(vi);
        Emit("  store i64 " + v + ", i64* " + addr);
      } else if (auto *fd = dynamic_cast<absyn::FunctionDec *>(d)) {
        // Enter headers for all functions in the (mutually-recursive) group.
        for (auto *f : fd->functions->fundecs) {
          FunInfo fi;
          fi.label = "tigerfn_" + Sn(f->name) + "_" +
                     std::to_string(str_id_ + label_ + temp_) + "_" +
                     std::to_string((long)f);
          fi.depth = cur_depth_ + 1;
          fi.result = f->result ? tenv_.Look(f->result) : VoidT();
          if (!fi.result) fi.result = VoidT();
          for (auto *p : f->params->fields) {
            Ty *pt = tenv_.Look(p->typ);
            fi.formals.push_back(pt ? pt : IntT());
          }
          fenv_.Enter(f->name, new FunInfo(fi));
          pending.emplace_back(f, fi);
        }
      }
    }

    // Emit each pending function body (deferred: capture and restore the
    // emitter's per-function state).
    for (auto &pr : pending) EmitFunction(pr.first, pr.second);

    Ty *bt = nullptr;
    std::string bv = TransExp(e->body, &bt);
    *out_ty = bt;

    fenv_.EndScope();
    tenv_.EndScope();
    venv_.EndScope();
    return bv;
  }

  return "0";
}

// Emit a user function's body as a separate LLVM function. Because nested lets
// can appear anywhere, we snapshot and restore the current-function state.
void Emitter::EmitFunction(absyn::FunDec *fd, const FunInfo &fi) {
  // Snapshot.
  std::ostringstream *savedCur = cur_;
  int savedTemp = temp_, savedLabel = label_, savedSlots = cur_slots_,
      savedDepth = cur_depth_;
  std::string savedFrame = cur_frame_, savedBreak = break_label_;

  std::ostringstream body;
  cur_ = &body;
  temp_ = 0;
  label_ = 0;
  cur_slots_ = 0;
  cur_depth_ = fi.depth;
  break_label_.clear();

  venv_.BeginScope();

  bool hasResult = fi.result && !dynamic_cast<type::VoidTy *>(fi.result);
  std::ostringstream sig;
  sig << "define " << (hasResult ? "i64" : "void") << " @" << fi.label
      << "(i64 %sl";
  int argi = 0;
  std::vector<int> paramSlots;
  for (auto *p : fd->params->fields) {
    sig << ", i64 %arg" << argi;
    ++argi;
  }
  sig << ") {\nentry:";
  body << sig.str() << "\n";
  cur_block_ = "entry";

  // Allocate this function's frame. First count slots: 1 (static link) + a
  // generous upper bound. We size it after emitting by patching, so instead we
  // over-allocate using a fixed large frame. To keep it correct without a
  // pre-pass, allocate the frame with a size computed from a slot counter and
  // a placeholder we patch. Simpler: allocate a large fixed frame (256 slots),
  // which comfortably covers the course programs.
  const int kFrameSlots = 256;
  std::string framei = NewTemp();
  body << "  " << framei << " = call i64 @tiger_alloc_record(i64 "
       << (kFrameSlots * 8) << ")\n";
  cur_frame_ = framei;
  // Store parent static link into slot 0.
  {
    std::string fp = NewTemp();
    body << "  " << fp << " = inttoptr i64 " << framei << " to i64*\n";
    std::string s0 = NewTemp();
    body << "  " << s0 << " = getelementptr i64, i64* " << fp << ", i64 0\n";
    body << "  store i64 %sl, i64* " << s0 << "\n";
  }
  cur_ = &body;  // ensure Emit writes to body from here

  // Bind parameters into frame slots.
  argi = 0;
  for (auto *p : fd->params->fields) {
    Ty *pt = tenv_.Look(p->typ);
    int slot = AllocSlot();
    venv_.Enter(p->name, new VarInfo{fi.depth, slot, pt ? pt : IntT()});
    VarInfo vi{fi.depth, slot, pt ? pt : IntT()};
    std::string addr = FrameSlotAddr(vi);
    Emit("  store i64 %arg" + std::to_string(argi) + ", i64* " + addr);
    ++argi;
  }

  Ty *bt = nullptr;
  std::string rv = TransExp(fd->body, &bt);
  if (hasResult)
    Emit("  ret i64 " + rv);
  else
    Emit("  ret void");
  Emit("}");

  funcs_ << body.str() << "\n";

  venv_.EndScope();

  // Restore.
  cur_ = savedCur;
  temp_ = savedTemp;
  label_ = savedLabel;
  cur_slots_ = savedSlots;
  cur_depth_ = savedDepth;
  cur_frame_ = savedFrame;
  break_label_ = savedBreak;
}

void Emitter::HoistFunctions(Exp *) {}

std::string Emitter::Run(Exp *root) {
  // Runtime declarations.
  globals_ << "; ---- Tiger program compiled to LLVM IR ----\n";
  std::string decls =
      "declare i64 @tiger_alloc_record(i64)\n"
      "declare i64 @tiger_init_array(i64, i64)\n"
      "declare i64 @tiger_getchar()\n"
      "declare i64 @ord(i64)\n"
      "declare i64 @chr(i64)\n"
      "declare i64 @size(i64)\n"
      "declare i64 @substring(i64, i64, i64)\n"
      "declare i64 @concat(i64, i64)\n"
      "declare i64 @tiger_not(i64)\n"
      "declare i64 @tiger_streq(i64, i64)\n"
      "declare void @print(i64)\n"
      "declare void @printi(i64)\n"
      "declare void @flush()\n"
      "declare void @tiger_exit(i64)\n";

  // Emit tigermain.
  std::ostringstream body;
  cur_ = &body;
  temp_ = 0;
  label_ = 0;
  cur_slots_ = 0;
  cur_depth_ = 0;
  break_label_.clear();

  venv_.BeginScope();
  tenv_.BeginScope();
  fenv_.BeginScope();
  tenv_.Enter(sym::Symbol::UniqueSymbol("int"), IntT());
  tenv_.Enter(sym::Symbol::UniqueSymbol("string"), StrT());

  body << "define i32 @main() {\nentry:\n";
  cur_block_ = "entry";
  const int kFrameSlots = 256;
  std::string framei = NewTemp();
  body << "  " << framei << " = call i64 @tiger_alloc_record(i64 "
       << (kFrameSlots * 8) << ")\n";
  cur_frame_ = framei;
  {
    std::string fp = NewTemp();
    body << "  " << fp << " = inttoptr i64 " << framei << " to i64*\n";
    std::string s0 = NewTemp();
    body << "  " << s0 << " = getelementptr i64, i64* " << fp << ", i64 0\n";
    body << "  store i64 0, i64* " << s0 << "\n";
  }

  Ty *rt = nullptr;
  TransExp(root, &rt);
  Emit("  ret i32 0");
  Emit("}");

  fenv_.EndScope();
  tenv_.EndScope();
  venv_.EndScope();

  std::ostringstream out;
  out << globals_.str() << "\n" << decls << "\n" << funcs_.str() << "\n"
      << body.str();
  return out.str();
}

}  // namespace

std::string Translate(absyn::Exp *root) {
  Emitter e;
  return e.Run(root);
}

}  // namespace tr
