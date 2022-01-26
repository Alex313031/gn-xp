#include "util/stdio_redirect.h"

#include "base/logging.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#else
#include <unistd.h>
#include "base/posix/eintr_wrapper.h"
#endif  // _WIN32

namespace {

using StdType = StdioRedirect::StdType;

FILE* StdTypeToStdFile(StdType std) {
  switch (std) {
    case StdType::STD_TYPE_IN:
      return stdin;
    case StdType::STD_TYPE_OUT:
      return stdout;
    case StdType::STD_TYPE_ERR:
      return stderr;
    default:
      NOTREACHED();
      return nullptr;
  }
}

#ifdef _WIN32

DWORD StdTypeToWin32Type(StdType std) {
  switch (std) {
    case StdType::STD_TYPE_IN:
      return STD_INPUT_HANDLE;
    case StdType::STD_TYPE_OUT:
      return STD_OUTPUT_HANDLE;
    case StdType::STD_TYPE_ERR:
      return STD_ERROR_HANDLE;
    default:
      NOTREACHED();
      return 0;
  }
}

// Duplicate Win32 handle.
HANDLE DupHandle(HANDLE handle) {
  HANDLE process = GetCurrentProcess();
  HANDLE new_handle;
  CHECK(DuplicateHandle(process, handle, process, &new_handle, 0, FALSE,
                        DUPLICATE_SAME_ACCESS));
  return new_handle;
}

void SetStandardHandle(HANDLE handle, HANDLE* prev_handle, StdType std) {
  // Important note: there are two handles to update for this to work
  // properly: The low-level Win32 handle value returned by GetStdHandle(),
  // as well as the handle value stored in the C runtime data structure
  // that maps file descriptors (integer values) to handles.
  //
  // The C runtime copies the GetStdHandle() value into its map at
  // program startup, so calling SetStdHandle() is not enough to ensure
  // that printf() et al are redirected.
  //
  // Updating the C runtime map requires using dup2() which will
  // do an implicit CloseHandle() on the previous handle value associated
  // with the destination file descriptor.

  // file_no is the C runtime file descriptor for this standard handle,
  // in practice, this will be 1 or 2, unless changed by something else
  // in the process, so use fileno() to cover all cases.
  int file_no = fileno(StdTypeToStdFile(std));

  // Duplicate the current HANDLE associated with |file_no|, because
  // the dup2() below will actually close it.
  if (prev_handle) {
    *prev_handle = DupHandle((HANDLE)_get_osfhandle(file_no));
  }
  // Create a new fd associated with a duplicate of |handle|, since
  // the descriptor is closed below after the dup2(), and the caller
  // expects |handle| to survive this call.
  int flags = O_TEXT;
  if (std == StdType::STD_TYPE_IN)
    flags |= O_RDONLY;
  else
    flags |= O_WRONLY;
  int fd = _open_osfhandle((intptr_t)DupHandle(handle), flags);
  CHECK(fd >= 0);

  // Ensure |file_no| is now associated with another duplicate of |handle|
  // Note that this closes the previous value implicitly.
  CHECK(!dup2(fd, file_no));

  // Get rid of our temporary file descriptor, which closes the first
  // |handle| duplicate. The second one is owned by the C runtime.
  close(fd);

  // Update the Win32 handle, since its value is now stale.
  CHECK(SetStdHandle(StdTypeToWin32Type(std), handle));
}

#else  // !_WIN32

void SetStandardHandle(int fd, int* prev_fd, StdType std) {
  // Use fileno() in case stdout/stderr were already redirect
  // in the current process.
  int dst_fd = fileno(StdTypeToStdFile(std));

  if (prev_fd) {
    *prev_fd = HANDLE_EINTR(dup(dst_fd));
    PCHECK(*prev_fd >= 0);
  }
  PCHECK(HANDLE_EINTR(dup2(fd, dst_fd)) == dst_fd);
}

#endif  // !_WIN32

}  // namespace

StdioRedirect::StdioRedirect(StdType std, HandleType new_handle) : std_(std) {
  fflush(StdTypeToStdFile(std));
  SetStandardHandle(new_handle, &prev_handle_, std);
}

StdioRedirect::~StdioRedirect() {
  fflush(StdTypeToStdFile(std_));
  SetStandardHandle(prev_handle_, nullptr, std_);
}
