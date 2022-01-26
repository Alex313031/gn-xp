#include "util/ipc_handle.h"

#include "base/strings/stringprintf.h"

#ifdef _WIN32

#include <stdio.h>

#include <fileapi.h>
#include <lmcons.h>
#include <stringapiset.h>

namespace {

std::string Win32ErrorMessage(const char* prefix, LONG error) {
  return base::StringPrintf("%s: %08lx", prefix, error);
}

std::string Win32ErrorMessage(const char* prefix) {
  return Win32ErrorMessage(prefix, GetLastError());
}

std::wstring Utf8ToWideChar(std::string_view str) {
  std::wstring result;
  int count =
      MultiByteToWideChar(CP_UTF8, 0, str.data(), str.size(), nullptr, 0);
  if (count > 1) {
    // count includes the terminating zero!
    result.resize(count - 1);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), str.size(), &result[0], count);
  }
  return result;
}

std::wstring CurrentUserName() {
  wchar_t user[UNLEN + 1];
  DWORD count = UNLEN + 1;
  if (!GetUserNameW(user, &count) || count < 2) {
    return std::wstring(L"unknown_user");
  }
  // count includes the terminating zero.
  return std::wstring(user, count - 1);
}

std::wstring GetNamedPipePath(std::string_view service_name) {
  std::wstring result = L"\\\\.\\pipe\\basic_ipc-";
  result += CurrentUserName();
  result += L'-';
  result += Utf8ToWideChar(service_name);
  return result;
}

HANDLE CreateNamedPipeHandle(const std::wstring& pipe_path,
                             std::string* error_message) {
  HANDLE handle =
      CreateNamedPipeW(pipe_path.data(),
                       PIPE_ACCESS_DUPLEX,       // bidirectional
                       PIPE_TYPE_BYTE |          // write bytes, not messages
                           PIPE_READMODE_BYTE |  // read bytes, not messages
                           PIPE_WAIT |           // synchronous
                           PIPE_REJECT_REMOTE_CLIENTS,  // local only
                       1,                               // max. instances
                       4096,                            // out buffer size
                       4096,                            // in buffer size
                       0,                               // default time out
                       nullptr);  // default security attribute
  if (handle == INVALID_HANDLE_VALUE)
    *error_message = Win32ErrorMessage("Could not create named pipe");

  return handle;
}

HANDLE ConnectToNamedPipe(const std::wstring& pipe_path,
                          bool non_blocking,
                          std::string* error_message) {
  HANDLE handle = CreateFileW(pipe_path.data(), GENERIC_READ | GENERIC_WRITE,
                              0,           // no sharing
                              nullptr,     // default security attributes
                              CREATE_NEW,  // fail if it already exists
                              non_blocking ? FILE_FLAG_OVERLAPPED : 0,
                              nullptr);  // no template file
  if (handle == INVALID_HANDLE_VALUE)
    *error_message = Win32ErrorMessage("Coult not connect pipe");

  return handle;
}

std::wstring GetUniqueNamedPipePath() {
  // Do not use CreatePipe because these are documented as only
  // unidirectional and synchronous only. Instead create a
  // named pipe with a unique name. This matches the current
  // implementation in Windows, though it may change in future
  // releases of the platform, see:
  // https://stackoverflow.com/questions/60645/overlapped-i-o-on-anonymous-pipe/51448441#51448441
  static LONG serial_number = 1;
  wchar_t pipe_path[64];
  swprintf(pipe_path, L"\\\\.\\pipe\\IpcHandle.%08x.%08x",
           GetCurrentProcessId(), InterlockedIncrement(&serial_number));
  return std::wstring(pipe_path);
}

}  // namespace

// INVALID_HANDLE_VALUE expands to an expression that includes
// a reinterpret_cast<>, which is why it cannot be used to
// define a constexpr static member.
const HANDLE IpcHandle::kInvalid = INVALID_HANDLE_VALUE;

void IpcHandle::Close() {
  if (handle_ != kInvalid) {
    CloseHandle(handle_);
    handle_ = kInvalid;
  }
}

ssize_t IpcHandle::Read(void* buff,
                        size_t buffer_size,
                        std::string* error_message) const {
  auto* buffer = static_cast<char*>(buff);
  DWORD count = 0;
  if (!ReadFile(handle_, buffer, buffer_size, &count, nullptr)) {
    DWORD error = GetLastError();
    if (error == ERROR_BROKEN_PIPE)  // Pipe was closed.
      return 0;
    *error_message = Win32ErrorMessage("Could not read from pipe");
    return -1;
  }
  return static_cast<ssize_t>(count);
}

ssize_t IpcHandle::Write(const void* buff,
                         size_t buffer_size,
                         std::string* error_message) const {
  const auto* buffer = static_cast<const char*>(buff);
  DWORD count = 0;
  if (!WriteFile(handle_, buffer, buffer_size, &count, nullptr)) {
    LONG error = GetLastError();
    if (error == ERROR_BROKEN_PIPE)  // Pipe was closed.
      return 0;
    *error_message = Win32ErrorMessage("Could not write to pipe", error);
    return -1;
  }
  return static_cast<ssize_t>(count);
}

struct HandleMessage {
  HANDLE process_id;
  HANDLE handle;
};

bool IpcHandle::SendNativeHandle(HandleType native,
                                 std::string* error_message) const {
  // Send a message that contains the current process ID, and the handle
  // through the named pipe. The ReceiveNativeHandle() method will use them
  // to call DuplicateHandle(). Note that this does not work for console
  // input/output handles (the handles can be duplicated, but trying to use them
  // from a different process returns an error), so in addition to using this
  // method, the sender should use a specialized class to redirect stdout/stderr
  // using new named pipe handles. See the Win32StdOutputBridge class.
  HandleMessage msg;
  msg.process_id =
      OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
  msg.handle = native;

  ssize_t count = Write(&msg, sizeof(msg), error_message);
  if (count < 0)
    return false;
  if (count != static_cast<ssize_t>(sizeof(msg))) {
    *error_message = "Error when sending handle";
    return false;
  }
  return true;
}

bool IpcHandle::ReceiveNativeHandle(IpcHandle* handle,
                                    std::string* error_message) const {
  HandleMessage msg;
  ssize_t count = Read(&msg, sizeof(msg), error_message);
  if (count < 0)
    return false;
  if (count != static_cast<ssize_t>(sizeof(msg))) {
    *error_message = "Error when receiving handle";
    return false;
  }

  // Create a duplicate of the source handle in the current process.
  HANDLE native = INVALID_HANDLE_VALUE;
  if (!DuplicateHandle(msg.process_id,       // source process
                       msg.handle,           // source handle
                       GetCurrentProcess(),  // target process
                       &native,              // target handle pointer
                       0,      // ignored with DUPLICATE_SAME_ACCESS
                       FALSE,  // not inheritable
                       DUPLICATE_SAME_ACCESS)) {
    *error_message = Win32ErrorMessage("Could not duplicate handle");
    return false;
  }
  *handle = IpcHandle(native);
  return true;
}

IpcServiceHandle::~IpcServiceHandle() = default;

// static
IpcServiceHandle IpcServiceHandle::BindTo(std::string_view service_name,
                                          std::string* error_message) {
  std::wstring path = GetNamedPipePath(service_name);
  return IpcServiceHandle(
      CreateNamedPipeHandle(GetNamedPipePath(service_name), error_message));
}

IpcHandle IpcServiceHandle::AcceptClient(std::string* error_message) const {
  if (!ConnectNamedPipe(handle_, NULL)) {
    DWORD error = GetLastError();
    // ERROR_PIPE_CONNECTED is not an actual error and means that
    // a client is already connected, which happens during unit-testing.
    if (error != ERROR_PIPE_CONNECTED) {
      *error_message =
          Win32ErrorMessage("Could not accept named pipe client", error);
      return {};
    }
  }
  // Duplicate the handle to return it into a new IpcHandle. This
  // will allow the caller to communicate with the client, and the next
  // ConnectNamedPipe() call will wait for another client instead.
  HANDLE process = GetCurrentProcess();
  HANDLE peer = INVALID_HANDLE_VALUE;
  if (!DuplicateHandle(process, handle_, process, &peer, 0, FALSE,
                       DUPLICATE_SAME_ACCESS)) {
    DWORD error = GetLastError();
    *error_message =
        Win32ErrorMessage("Could not duplicate client pipe handle", error);
    return {};
  }
  return IpcHandle(peer);
}

// static
IpcHandle IpcHandle::ConnectTo(std::string_view service_name,
                               std::string* error_message) {
  return IpcHandle(
      ConnectToNamedPipe(GetNamedPipePath(service_name), false, error_message));
}

// static
bool IpcHandle::CreatePipe(IpcHandle* read,
                           IpcHandle* write,
                           std::string* error_message) {
  std::wstring pipe_path = GetUniqueNamedPipePath();
  *read =
      IpcHandle(CreateNamedPipeHandle(std::wstring(pipe_path), error_message));
  if (!*read)
    return false;

  *write = IpcHandle(ConnectToNamedPipe(pipe_path, false, error_message));
  if (!*write) {
    read->Close();
    return false;
  }
  return true;
}

// static
void Win32StdHandleBridge::ThreadMain() {
  char buffer[16384];
  std::string error_message;

  for (;;) {
    // Start asynchronous read operation.
    OVERLAPPED overlapped = {};
    overlapped.hEvent = event_read_;
    ResetEvent(event_read_);
    if (!ReadFile(pipe_read_, buffer, sizeof(buffer), nullptr, &overlapped))
      break;

    // Wait for some data.
    DWORD read_count = 0;
    if (!GetOverlappedResultEx(pipe_read_, &overlapped, &read_count, INFINITE,
                               FALSE))
      break;

    // Write it synchronously to the other end.
    DWORD write_count = 0;
    while (write_count < read_count) {
      DWORD count = 0;
      if (!WriteFile(std_handle_, buffer + write_count,
                     read_count - write_count, &count, nullptr)) {
        break;
      }
      write_count += count;
    }
  }
}

bool Win32StdHandleBridge::Init(int channel, std::string* error_message) {
  DWORD handle_type;
  switch (channel) {
    case 0:  // stdin
      handle_type = STD_INPUT_HANDLE;
      break;
    case 1:  // stdout
      handle_type = STD_OUTPUT_HANDLE;
      break;
    case 2:  // stderr
      handle_type = STD_ERROR_HANDLE;
      break;
    default:
      *error_message =
          base::StringPrintf("Invalid std channel number %d", channel);
      return false;
  }
  std_handle_ = GetStdHandle(handle_type);
  // Create pipe, with the read end in overlapped node, while the write
  // end will be synchronous.
  std::wstring pipe_path = GetUniqueNamedPipePath();
  pipe_write_ = CreateNamedPipeHandle(pipe_path, error_message);
  if (!pipe_write_)
    return false;

  pipe_read_ = ConnectToNamedPipe(pipe_path, true, error_message);
  if (!pipe_read_) {
    return false;
  }

  // Swap the read and write end for stdin.
  if (channel == 0) {
    HANDLE tmp = pipe_write_;
    pipe_write_ = pipe_read_;
    pipe_read_ = tmp;
  }

  // Create manual-reset event that will be used to signal reads
  // to our worker thread.
  event_read_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  if (event_read_ == INVALID_HANDLE_VALUE) {
    *error_message = Win32ErrorMessage("Could not create bridge event");
    return false;
  }

  // Start the thread.
  thread_ =
      CreateThread(nullptr,  // default security attribute
                   32768,    // stack size
                   &Win32StdHandleBridge::Trampoline,  // trampoline function.
                   this,                               // trampoline argument.
                   0,                                  // default flags
                   nullptr);                           // thread-id pointer
  if (thread_ == INVALID_HANDLE_VALUE) {
    *error_message = Win32ErrorMessage("Cannot create bridge thread");
    return false;
  }
  return true;
}

Win32StdHandleBridge::~Win32StdHandleBridge() {
  // Tear down the pipe, which should stop the thread.
  if (pipe_write_ != INVALID_HANDLE_VALUE)
    CloseHandle(pipe_write_);
  if (pipe_read_ != INVALID_HANDLE_VALUE)
    CloseHandle(pipe_read_);

  // Tear down the thread.
  if (thread_ != INVALID_HANDLE_VALUE) {
    WaitForSingleObject(thread_, 0);
    CloseHandle(thread_);
  }
  // Tear down the event.
  if (event_read_ != INVALID_HANDLE_VALUE)
    CloseHandle(event_read_);
}

#else  // !_WIN32

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"

namespace {

// Set USE_LINUX_NAMESPACE to 1 to use Linux abstract
// unix namespace, which do not require a filesystem
// entry point.
#ifdef __linux__
#define USE_LINUX_NAMESPACE 1
#else
#define USE_LINUX_NAMESPACE 0
#endif

#if !USE_LINUX_NAMESPACE
// Return runtime directory where to create a Unix socket.
// Only used for non-Linux systems. Callers can assume
// that the directory already exists (otherwise, the system
// is not configured properly, and an error on bind or
// connect operation is expected).
std::string GetRuntimeDirectory() {
  std::string result;
  // XDG_RUNTIME_DIR might be defined on BSDs and other operating
  // systems.
  const char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (xdg_runtime_dir) {
    result = xdg_runtime_dir;
  }
  if (result.empty()) {
    const char* tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0])
      tmp = "/tmp";
    result = tmp;
  }
  return result;
}
#endif  // !USE_LINUX_NAMESPACE

// Return the Unix socket path to be used for |service_name|.
std::string CreateUnixSocketPath(std::string_view service_name) {
  // On Linux, use the abstract namespace by creating a string with
  // a NUL byte at the front. On other platform, use the runtime
  // directory instead.
#if USE_LINUX_NAMESPACE
  std::string result(1, '\0');
#else
  std::string result = GetRuntimeDirectory() + "/";
#endif
  result += "basic_ipc-";
  const char* user = getenv("USER");
  if (!user || !user[0])
    user = "unknown_user";
  result += user;
  result += "-";
  result += service_name;
  return result;
}

// Convenience class to model a Unix socket address.
// Usage is:
//  1) Create instance, passing service name.
//  2) Call valid() to check the instance. If false the
//     service name was too long.
//  3) Use address() and size() to pass to sendmsg().
class LocalAddress {
 public:
  LocalAddress(std::string_view service_name) {
    memset(this, 0, sizeof(*this));
    local_.sun_family = AF_UNIX;
    std::string path = CreateUnixSocketPath(service_name);
    if (path.size() >= sizeof(local_.sun_path))
      return;  // Service name is too long.

    memcpy(local_.sun_path, path.data(), path.size());
    local_.sun_path[path.size()] = '\0';
    size_ = offsetof(sockaddr_un, sun_path) + path.size() + 1;
  }

  bool valid() const { return size_ > 0; }
  sockaddr* address() { return &generic_; }
  size_t size() const { return size_; }
  const char* path() const { return local_.sun_path; }

 private:
  size_t size_ = 0;
  union {
    sockaddr_un local_;
    sockaddr generic_;
  };
};

}  // namespace

void IpcHandle::Close() {
  if (handle_ != kInvalid) {
    IGNORE_EINTR(close(handle_));
    handle_ = kInvalid;
  }
}

ssize_t IpcHandle::Read(void* buff,
                        size_t buffer_size,
                        std::string* error_message) const {
  auto* buffer = static_cast<char*>(buff);
  ssize_t result = 0;
  while (buffer_size > 0) {
    ssize_t count = read(handle_, buffer, buffer_size);
    if (count < 0) {
      if (errno == EINTR)
        continue;
      if (result > 0) {
        // Ignore this error to return the current read result.
        // This assumes the error will repeat on the next call.
        break;
      }
      *error_message = std::string(strerror(errno));
      return -1;
    } else if (count == 0) {
      break;
    }
    buffer += count;
    buffer_size -= static_cast<size_t>(count);
    result += count;
  }
  return result;
}

ssize_t IpcHandle::Write(const void* buff,
                         size_t buffer_size,
                         std::string* error_message) const {
  auto* buffer = static_cast<const char*>(buff);
  ssize_t result = 0;
  while (buffer_size > 0) {
    ssize_t count = write(handle_, buffer, buffer_size);
    if (count < 0) {
      if (errno == EINTR)
        continue;
      if (result > 0) {
        break;
      }
      *error_message = std::string(strerror(errno));
      return -1;
    } else if (count == 0) {
      break;
    }
    buffer += count;
    buffer_size -= static_cast<size_t>(count);
    result += count;
  }
  return result;
}

bool IpcHandle::SendNativeHandle(HandleType native,
                                 std::string* error_message) const {
  char ch = 'x';
  iovec iov = {&ch, 1};
  union {
    char buf[CMSG_SPACE(sizeof(int))];
    cmsghdr align;
  } control;
  memset(control.buf, 0, sizeof(control.buf));

  msghdr header = {};
  header.msg_iov = &iov;
  header.msg_iovlen = 1;
  header.msg_control = control.buf;
  header.msg_controllen = sizeof(control.buf);

  cmsghdr* control_header = CMSG_FIRSTHDR(&header);
  control_header->cmsg_len = CMSG_LEN(sizeof(int));
  control_header->cmsg_level = SOL_SOCKET;
  control_header->cmsg_type = SCM_RIGHTS;
  reinterpret_cast<int*>(CMSG_DATA(control_header))[0] = native;

  ssize_t ret = sendmsg(handle_, &header, 0);
  if (ret == -1) {
    *error_message = strerror(errno);
    return false;
  }
  return true;
}

bool IpcHandle::ReceiveNativeHandle(IpcHandle* native,
                                    std::string* error_message) const {
  char ch = '\0';
  iovec iov = {&ch, 1};
  union {
    char buf[CMSG_SPACE(sizeof(int))];
    cmsghdr align;
  } control;
  memset(control.buf, 0, sizeof(control.buf));

  msghdr header = {};
  header.msg_iov = &iov;
  header.msg_iovlen = 1;
  header.msg_control = control.buf;
  header.msg_controllen = sizeof(control.buf);

  ssize_t ret = recvmsg(handle_, &header, 0);
  if (ret == -1) {
    *error_message = strerror(errno);
    return false;
  }

  cmsghdr* control_header = CMSG_FIRSTHDR(&header);
  if (!control_header || control_header->cmsg_len != CMSG_LEN(sizeof(int)) ||
      control_header->cmsg_level != SOL_SOCKET ||
      control_header->cmsg_type != SCM_RIGHTS) {
    *error_message =
        std::string("Invalid data when receiving file descriptor!");
    return false;
  }
  *native = IpcHandle(reinterpret_cast<int*>(CMSG_DATA(control_header))[0]);
  return true;
}

IpcServiceHandle::~IpcServiceHandle() {
  if (socket_path_.size() && socket_path_[0] != '\0') {
    // Remove socket and pid file.
    unlink(socket_path_.c_str());

    std::string pid_path = socket_path_;
    pid_path += ".pid";
    unlink(pid_path.c_str());
  }
}

// static
IpcServiceHandle IpcServiceHandle::BindTo(std::string_view service_name,
                                          std::string* error_message) {
  LocalAddress address(service_name);
  if (!address.valid()) {
    *error_message = std::string("Service name too long");
    return {};
  }
#if !USE_LINUX_NAMESPACE
  // Try to see if another server is already running. Use a .pid file
  // that contains the server's process PID to do that, and check whether
  // it is still alive.
  bool server_running = false;
  std::string pid_path = std::string(address.path()) + ".pid";
  // Try to open the pid file.
  FILE* pid_file = fopen(pid_path.c_str(), "r");
  if (pid_file) {
    int server_pid = 0;
    server_running = fscanf(pid_file, "%d", &server_pid) && server_pid > 0 &&
                     kill(server_pid, 0) == 0;
    fclose(pid_file);
    // Remove stale socket.
    if (!server_running) {
      unlink(address.path());
    }
  } else if (errno != ENOENT) {
    // Could not open due to different reason :-(
    *error_message = "Cannot open pid file: ";
    *error_message += strerror(errno);
    return {};
  }
  if (server_running) {
    *error_message = "already in use";
    return {};
  }

  // Create new temporary pid file before doing an atomic filesystem rename.
  int cur_pid = getpid();
  std::string temp_pid_path =
      base::StringPrintf("%s.temp.%d", pid_path.c_str(), cur_pid);
  {
    bool pid_file_error = false;
    FILE* pid_file = fopen(temp_pid_path.c_str(), "w");
    if (!pid_file) {
      pid_file_error = true;
    } else {
      if (fprintf(pid_file, "%d", cur_pid) <= 0)
        pid_file_error = true;
      fclose(pid_file);
    }
    if (pid_file_error) {
      *error_message = "Cannot create temporary pid file: ";
      *error_message += strerror(errno);
      return {};
    }
  }
  // atomically rename the temporary file.
  // Note that EINTR can happen in practice in rename() :-(
  if (HANDLE_EINTR(rename(temp_pid_path.c_str(), pid_path.c_str())) < 0) {
    *error_message = "Cannot rename pid file: ";
    *error_message += strerror(errno);
    return {};
  }
#endif

  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd == -1) {
    *error_message = strerror(errno);
    return {};
  }
  if (bind(server_fd, address.address(), address.size()) < 0 ||
      listen(server_fd, 1) < 0) {
    *error_message = strerror(errno);
    IGNORE_EINTR(close(server_fd));
    return {};
  }
  return IpcServiceHandle(server_fd, address.path());
}

IpcHandle IpcServiceHandle::AcceptClient(std::string* error_message) const {
  int client = HANDLE_EINTR(accept(handle_, nullptr, nullptr));
  if (client < 0) {
    *error_message = strerror(errno);
    return {};
  }
  return {client};
}

// static
IpcHandle IpcHandle::ConnectTo(std::string_view service_name,
                               std::string* error_message) {
  LocalAddress address(service_name);
  if (!address.valid()) {
    *error_message = std::string("Service name too long");
    return {};
  }
  int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (client_fd == -1) {
    *error_message = strerror(errno);
    return {};
  }
  if (HANDLE_EINTR(connect(client_fd, address.address(), address.size())) < 0) {
    *error_message = strerror(errno);
    IGNORE_EINTR(close(client_fd));
    return {};
  }
  return {client_fd};
}

// static
bool IpcHandle::CreatePipe(IpcHandle* read,
                           IpcHandle* write,
                           std::string* error_message) {
  int fds[2] = {-1, -1};
  if (pipe(fds) != 0) {
    *error_message = strerror(errno);
    return false;
  }
  *read = fds[0];
  *write = fds[1];
  return true;
}

#endif  // !_WIN32

bool IpcHandle::ReadFull(void* buffer,
                         size_t buffer_size,
                         std::string* error_message) const {
  ssize_t count = Read(buffer, buffer_size, error_message);
  if (count < 0)
    return false;
  if (count != static_cast<ssize_t>(buffer_size)) {
    *error_message = base::StringPrintf("Received %u bytes, expected %u",
                                        static_cast<unsigned>(count),
                                        static_cast<unsigned>(buffer_size));
    return false;
  }
  return true;
}

bool IpcHandle::WriteFull(const void* buffer,
                          size_t buffer_size,
                          std::string* error_message) const {
  ssize_t count = Write(buffer, buffer_size, error_message);
  if (count < 0)
    return false;
  if (count != static_cast<ssize_t>(buffer_size)) {
    *error_message = base::StringPrintf("Sent %u bytes, expected %u",
                                        static_cast<unsigned>(count),
                                        static_cast<unsigned>(buffer_size));
  }
  return true;
}
