#ifndef SRC_UTIL_STDIO_REDIRECT_H_
#define SRC_UTIL_STDIO_REDIRECT_H_

#ifdef _WIN32
#include <windows.h>
#endif

// A class used to temporarily redirect stdout/stderr
// in the current process.
class StdioRedirect {
 public:
#ifdef _WIN32
  using HandleType = HANDLE;
#else
  using HandleType = int;
#endif
  enum class StdType {
    STD_TYPE_IN = 0,
    STD_TYPE_OUT = 1,
    STD_TYPE_ERR = 2,
  };

  // Constructor, redirects stdout/stderr to |new_out| and |new_err|
  // respectively.
  StdioRedirect(StdType std, HandleType new_hanle);

  // Destructor restores the previous stdout/stderr values.
  // Note that this does not close the handles passed to the constructor.
  ~StdioRedirect();

 private:
  StdType std_;
  HandleType prev_handle_;
};

#endif  // SRC_UTIL_STDIO_REDIRECT_H_
