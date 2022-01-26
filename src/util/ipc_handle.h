#ifndef SRC_UTIL_BASIC_IPC_H_
#define SRC_UTIL_BASIC_IPC_H_

#include <stddef.h>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <basetsd.h>
#include <windows.h>
using ssize_t = SSIZE_T;
#else
#include <signal.h>
#endif

// Support for basic inter-process communication.
//
// All communications are synchronous and a
// server instance can only handle one client
// at a time, which keeps the implementation
// drastically simple.
//
// Usage is the following:
//
//   1) On the server side, use IpcServiceHandle::BindTo()
//      to bind to a specific named service. When this
//      succeeds, call IpcService::AcceptClient to accept
//      the next client connection.
//
//   2) On the client side, use IpcHandle::ConnecTo()
//      to connect to the service (passing the same name
//      as the one used by the server).
//
//   3) Use IpcHandle::Read()/Write()/etc.. to send
//      and receive data, and IpcHandle::SendNativeHandle()
//      and IpcHandle::ReceiveNativeHandle to receive
//      a native file handle/descriptor.
//

// Wrapper for a local Unix socket or Win32 named pipe
// used for inter-process communication.
class IpcHandle {
 public:
#ifdef _WIN32
  using HandleType = HANDLE;
  static const HandleType kInvalid;
#else
  using HandleType = int;
  static constexpr int kInvalid = -1;
#endif

  IpcHandle() = default;
  IpcHandle(HandleType handle) : handle_(handle) {}

  // Disallow copy operations.
  IpcHandle(const IpcHandle&) = delete;
  IpcHandle& operator=(const IpcHandle&) = delete;

  // Allow move operations.
  IpcHandle(IpcHandle&& other) noexcept : handle_(other.handle_) {
    other.handle_ = kInvalid;
  }
  IpcHandle& operator=(IpcHandle&& other) noexcept {
    if (this != &other) {
      this->~IpcHandle();
      new (this) IpcHandle(std::move(other));
    }
    return *this;
  }

  ~IpcHandle() {
    Close();
  }

  // bool conversion allows easy checks for valid handles with:
  //  if (!handle) {  ... handle is invalid };
  explicit operator bool() const noexcept {
    return handle_ != kInvalid;
  }

  // Try to read |buffer_size| bytes into |buffer|. On success
  // return the number of bytes actually read into the buffer,
  // which will be 0 if the connection was closed by the peer.
  // On failure, return -1 and sets |*error_message|.
  ssize_t Read(void* buffer,
               size_t buffer_size,
               std::string* error_message) const;

  // Read |buffer_size| bytes exactly into |buffer|. On success
  // return true. On failure, return false and set |*error_message|.
  bool ReadFull(void* buffer,
                size_t buffer_size,
                std::string* error_message) const;

  // Try to write |buffer_size| bytes form |buffer|. On success
  // return the number of bytes actually written to the handle,
  // which will be 0 if the connection was called by the peer.
  // On failure, return -1 and sets |*error_message|.
  ssize_t Write(const void* buffer,
                size_t buffer_size,
                std::string* error_message) const;

  // Write |buffer_size| bytes exactly from |buffer|. On success
  // return true. On failure, return false and set |*error_message|.
  bool WriteFull(const void* buffer,
                 size_t buffer_size,
                 std::string* error_message) const;

  // Send a |native| handle to the peer. On success return true.
  // On failure, return false and sets |*error_message|.
  bool SendNativeHandle(HandleType native, std::string* error_message) const;

  // Receive a |native| handle from the peer. On success return true
  // and sets |*handle|. On failure, return false and sets |*error_message|.
  bool ReceiveNativeHandle(IpcHandle* handle, std::string* error_message) const;

  // Connect to local |service_name|. On success, return
  // a valid IpcHandle, which can be used to communicate with the server.
  // On failure, return an invalid handle, and sets |*error_message|.
  static IpcHandle ConnectTo(std::string_view service_name,
                             std::string* error_message);

  // Create anonymous bi-directional pipe. On success return true and
  // sets |*read| and |*write|. On failure, return false and sets
  // |*error_message|.
  static bool CreatePipe(IpcHandle* read,
                         IpcHandle* write,
                         std::string* error_message);

  // Close the handle, making it invalid.
  void Close();

  // Return native handle value.
  HandleType native_handle() const {
    return handle_;
  }

 protected:
  HandleType handle_ = kInvalid;
};

// Models an IpcHandle used to bind to specific service.
// Only one process can bind to a specific named service at
// a time on a given machine.
class IpcServiceHandle : public IpcHandle {
 public:
  IpcServiceHandle() = default;
  ~IpcServiceHandle();

  // Create a server handle for |service_name|. On success, return
  // a valid IpcHandle, that can be used with AcceptClient().
  // on failure, return an invalid handle, and sets |*error_message|.
  //
  // This will fail if another server is already running with the
  // same name on the current machine (for the same user). Closing
  // the service handle will release the corresponding socket or
  // named pipe immediately, and of course the destructor closes
  // automatically.
  //
  // Note that the implementation should be resilient to program
  // crashes as well, i.e. on Linux and Win32, it uses kernel features
  // that ensure proper socket/pipe cleanup on process exit. On
  // other Unix systems, a Unix-domain socket and associated PID file
  // are used to detect stale socket files and remove them properly.
  static IpcServiceHandle BindTo(std::string_view service_name,
                                 std::string* error_message);

  // Accept one client connection. This is only valid for instances
  // returned from BindTo().
  IpcHandle AcceptClient(std::string* error_message) const;

  void Close();

 private:
#ifdef _WIN32
  IpcServiceHandle(IpcHandle::HandleType handle) : IpcHandle(handle) {}
#else
  IpcServiceHandle(IpcHandle::HandleType handle, const std::string& path)
      : IpcHandle(handle), socket_path_(path) {}

  std::string socket_path_;
#endif
};

#ifndef _WIN32

// Helper class to temporarily disable SIGPIPE, which halts
// the current process by default. These happen when IPC pipes
// are broken by the client.
class SigPipeIgnore {
 public:
  SigPipeIgnore() {
    struct sigaction new_handler;
    new_handler.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &new_handler, &prev_handler_);
  }

  ~SigPipeIgnore() { sigaction(SIGPIPE, &prev_handler_, nullptr); }

 private:
  struct sigaction prev_handler_;
};

#endif  // !_WIN32

#ifdef _WIN32
// On Win32, the standard output and error handles can be duplicated into
// other processes, but trying to use them there will error. So this class
// provides a way to get a native handle that can be used with
// IpcHandle::SendNativeHandle(). Usage is:
//
//  1) Create instance.
//  2) Call Init()
//  3) Pass the value returned by handle() to IpcHandle::SendNativeHandle().
//
class Win32StdHandleBridge {
 public:
  Win32StdHandleBridge() = default;
  ~Win32StdHandleBridge();

  // Create bridge for stdout or stderr, depending on |is_stderr| value.
  // On success return true, on failure, return false and sets |*error_message|.
  bool Init(int channel, std::string* error_message);

  // Return the handle value to use for SendNativeHandle()
  HANDLE handle() const { return pipe_write_; }

 private:
  void ThreadMain();

  static DWORD Trampoline(LPVOID arg) {
    reinterpret_cast<Win32StdHandleBridge*>(arg)->ThreadMain();
    return 0;
  }

  HANDLE std_handle_ = INVALID_HANDLE_VALUE;
  HANDLE pipe_read_ = INVALID_HANDLE_VALUE;
  HANDLE pipe_write_ = INVALID_HANDLE_VALUE;
  HANDLE event_read_ = INVALID_HANDLE_VALUE;
  HANDLE thread_ = INVALID_HANDLE_VALUE;
};

#endif  // _WIN32

#endif  // SRC_UTIL_BASIC_IPC_H_
