#include "tiger/symbol/symbol.h"

namespace sym {

Symbol *Symbol::UniqueSymbol(const std::string &name) {
  // A process-wide interning table. `Symbol` pointers stay valid forever, which
  // is fine for a batch compiler.
  static std::unordered_map<std::string, Symbol *> *interned =
      new std::unordered_map<std::string, Symbol *>();
  auto it = interned->find(name);
  if (it != interned->end()) return it->second;
  auto *sym = new Symbol(name);
  (*interned)[name] = sym;
  return sym;
}

}  // namespace sym
