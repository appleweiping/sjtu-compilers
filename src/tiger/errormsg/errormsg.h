#ifndef TIGER_ERRORMSG_ERRORMSG_H_
#define TIGER_ERRORMSG_ERRORMSG_H_

#include <string>
#include <vector>

namespace err {

// Tracks newline offsets in the input so that a character position can be
// rendered as `file:line.column`, matching the Tiger reference compiler.
class ErrorMsg {
 public:
  explicit ErrorMsg(std::string filename);

  // Record a newline at character offset `pos` (called by the lexer).
  void Newline(int pos);

  // Emit an error at character position `pos`. Sets AnyErrors().
  void Error(int pos, const std::string &message);

  [[nodiscard]] bool AnyErrors() const { return any_errors_; }
  [[nodiscard]] const std::string &FileName() const { return filename_; }

 private:
  std::string filename_;
  std::vector<int> line_starts_;  // char offset of the start of each line
  bool any_errors_ = false;
};

}  // namespace err

#endif  // TIGER_ERRORMSG_ERRORMSG_H_
