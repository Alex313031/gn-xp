#include "util/stdio_redirect.h"
#include "util/ipc_handle.h"
#include "util/test/test.h"

#include <stdlib.h>

#include "base/logging.h"

#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using StdType = StdioRedirect::StdType;

#ifdef _WIN32

#include <fcntl.h>
#include <io.h>

// Return standard HANDLE
HANDLE GetStandardHandle(StdType std) {
  switch (std) {
    case StdType::STD_TYPE_IN:
      return GetStdHandle(STD_INPUT_HANDLE);
    case StdType::STD_TYPE_OUT:
      return GetStdHandle(STD_OUTPUT_HANDLE);
    case StdType::STD_TYPE_ERR:
      return GetStdHandle(STD_ERROR_HANDLE);
    default:
      NOTREACHED();
      return 0;
  }
}

#else
int GetStandardHandle(StdType std) {
  return static_cast<int>(std);
}
#endif

// Implement a buffering output handle.
// Usage is:
//   1) Create instance.
//
//   2) Pass write_handle() to a function that will use it to
//      write things into it.
//
//   3) Call data() to retrieve the written data.
//
class BufferStream {
 public:
  BufferStream() {
    std::string error_message;
    CHECK(IpcHandle::CreatePipe(&read_, &write_, &error_message))
        << error_message;
    thread_ = std::thread(&BufferStream::ThreadMain, this);
  }

  ~BufferStream() {
    write_.Close();
    read_.Close();
    if (thread_.joinable())
      thread_.join();
  }

  IpcHandle::HandleType write_handle() { return write_.native_handle(); }

  std::string GetData() {
    StopThread();
    std::lock_guard<std::mutex> lock(lock_);
    return data_;
  }

 private:
  void ThreadMain() {
    std::string error_message;
    // All reads are synchronous with IpcHandle(),  and the
    // write handle may have been duplicated, so closing it is
    // not enough to ensure that the Read() below returns.
    // Instead, the magic char value 0x7f is sent in StopThread()
    // to signal the end. This forces reading one char at a time
    // but keeps this class simple.
    for (;;) {
      char ch;
      ssize_t read_count = read_.Read(&ch, 1, &error_message);
      if (read_count <= 0 || ch == '\x7f')
        break;

      std::lock_guard<std::mutex> lock(lock_);
      data_.push_back(ch);
    }
  }

  void StopThread() {
    // Send magic byte to stop the thread.
    std::string dummy;
    char ch = '\x7f';
    write_.Write(&ch, 1, &dummy);
    thread_.join();
  }

  std::mutex lock_;
  std::string data_;
  IpcHandle read_;
  IpcHandle write_;
  std::thread thread_;
};

}  // namespace

TEST(StdioRedirect, TestBufferStream) {
  BufferStream out;
  IpcHandle out_handle(out.write_handle());
  std::string error_message;
  EXPECT_TRUE(out_handle.Write("42", 2, &error_message));
  std::string data = out.GetData();
  EXPECT_EQ(std::string("42"), data) << "[" << data << "]";
}

TEST(StdioRedirect, ConstructionDestruction) {
  StdioRedirect redirect_stdin(StdType::STD_TYPE_IN,
                               GetStandardHandle(StdType::STD_TYPE_IN));
  StdioRedirect redirect_stdout(StdType::STD_TYPE_OUT,
                                GetStandardHandle(StdType::STD_TYPE_OUT));
  StdioRedirect redirect_stderr(StdType::STD_TYPE_ERR,
                                GetStandardHandle(StdType::STD_TYPE_ERR));
}

#ifdef _WIN32
#define NEWLINE "\r\n"
#else
#define NEWLINE "\n"
#endif

TEST(StdioRedirect, SimpleTest) {
  BufferStream out;
  BufferStream err;
  {
    StdioRedirect redirect_stderr(StdType::STD_TYPE_ERR, err.write_handle());
    StdioRedirect redirect_stdout(StdType::STD_TYPE_OUT, out.write_handle());
    fprintf(stdout, "Foo!\n");
    fprintf(stderr, "Bar?\n");
    fprintf(stdout, "Bar!\n");
    fprintf(stderr, "Foo?\n");
  }
  EXPECT_EQ(std::string("Foo!" NEWLINE "Bar!" NEWLINE), out.GetData());
  EXPECT_EQ(std::string("Bar?" NEWLINE "Foo?" NEWLINE), err.GetData());
}
