#include "tiger/errormsg/errormsg.h"

#include <algorithm>
#include <cstdio>

namespace err {

ErrorMsg::ErrorMsg(std::string filename) : filename_(std::move(filename)) {
  // Line 1 starts at character offset 1 (positions are 1-based, matching the
  // reference lexer whose first token in `merge.tig` is LET at position 1).
  line_starts_.push_back(1);
}

void ErrorMsg::Newline(int pos) {
  // The next line begins one character after the newline.
  line_starts_.push_back(pos + 1);
}

void ErrorMsg::Error(int pos, const std::string &message) {
  any_errors_ = true;
  // Find the last line whose start is <= pos.
  auto it = std::upper_bound(line_starts_.begin(), line_starts_.end(), pos);
  int line = static_cast<int>(it - line_starts_.begin());  // 1-based line
  int col = pos - line_starts_[line - 1] + 1;              // 1-based column
  printf("%s:%d.%d:%s\n", filename_.c_str(), line, col, message.c_str());
}

}  // namespace err
