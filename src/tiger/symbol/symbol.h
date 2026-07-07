#ifndef TIGER_SYMBOL_SYMBOL_H_
#define TIGER_SYMBOL_SYMBOL_H_

#include <string>
#include <unordered_map>
#include <vector>

namespace sym {

// An interned symbol: two Symbols are identical iff they come from the same
// name, so they can be compared by pointer.
class Symbol {
 public:
  [[nodiscard]] const std::string &Name() const { return name_; }

  // Intern `name`, returning the unique Symbol for it.
  static Symbol *UniqueSymbol(const std::string &name);

 private:
  std::string name_;
  explicit Symbol(std::string name) : name_(std::move(name)) {}
};

// A scoped symbol table (association-list based), used both as the environment
// during semantic analysis and as a generic symbol->value map.
template <typename ValueType>
class Table {
 public:
  Table() = default;

  // Bind `sym` to `value` in the current scope.
  void Enter(Symbol *sym, ValueType *value) {
    stack_.push_back(sym);
    auto &bucket = table_[sym];
    bucket.push_back(value);
  }

  // Look up the innermost binding of `sym`, or nullptr if unbound.
  ValueType *Look(Symbol *sym) const {
    auto it = table_.find(sym);
    if (it == table_.end() || it->second.empty()) return nullptr;
    return it->second.back();
  }

  // Open a new scope.
  void BeginScope() { stack_.push_back(nullptr); }

  // Close the current scope, undoing every binding made since BeginScope.
  void EndScope() {
    while (!stack_.empty() && stack_.back() != nullptr) {
      Symbol *sym = stack_.back();
      stack_.pop_back();
      table_[sym].pop_back();
    }
    if (!stack_.empty()) stack_.pop_back();  // pop the scope marker
  }

 private:
  std::unordered_map<Symbol *, std::vector<ValueType *>> table_;
  std::vector<Symbol *> stack_;  // nullptr entries mark scope boundaries
};

}  // namespace sym

#endif  // TIGER_SYMBOL_SYMBOL_H_
