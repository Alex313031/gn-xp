#include "util/ipc_handle.h"
#include "util/test/test.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

IpcHandle::HandleType CreateTestNativeHandle() {
#ifdef _WIN32
  return CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                     OPEN_EXISTING, 0, NULL);
#else
  return open("/dev/null", O_RDWR);
#endif
}

IpcHandle CreateTestHandle() {
  return {CreateTestNativeHandle()};
}

// Create a pipe for testing. On success return true and
// set |*read| and |*write| to be the read and write ends
// of the pipe. Note that the pipe is unidirectional due
// to portability with Windows.
bool CreateTestPipe(IpcHandle* read, IpcHandle* write) {
  std::string error;
  if (!IpcHandle::CreatePipe(read, write, &error)) {
    fprintf(stderr, "ERROR: when creating pipe: %s\n", error.c_str());
    return false;
  }
  return true;
}

}  // namespace

TEST(IpcHandle, Constructor) {
  // Default constructor makes invalid handle
  IpcHandle empty;
  EXPECT_FALSE(empty);
  EXPECT_EQ(IpcHandle::kInvalid, empty.native_handle());

  // Constructor gets test native handle
  IpcHandle::HandleType test_native_handle = CreateTestNativeHandle();
  IpcHandle test_handle(test_native_handle);
  EXPECT_TRUE(test_handle);
  EXPECT_EQ(test_native_handle, test_handle.native_handle());

  // Move constructor works.
  IpcHandle test_handle2 = std::move(test_handle);
  EXPECT_TRUE(test_handle2);
  EXPECT_EQ(test_native_handle, test_handle2.native_handle());
  EXPECT_FALSE(test_handle);
  EXPECT_EQ(IpcHandle::kInvalid, test_handle.native_handle());
}

TEST(IpcHandle, Close) {
  IpcHandle test_handle = CreateTestHandle();
  EXPECT_TRUE(test_handle);
  EXPECT_NE(IpcHandle::kInvalid, test_handle.native_handle());

  test_handle.Close();
  EXPECT_FALSE(test_handle);
  EXPECT_EQ(IpcHandle::kInvalid, test_handle.native_handle());

  // Ensure closing a closed handle doesn't crash.
  test_handle.Close();
  EXPECT_FALSE(test_handle);
  EXPECT_EQ(IpcHandle::kInvalid, test_handle.native_handle());
}

TEST(IpcHandle, ReadWrite) {
  const char kInputData[] = "Hello World!";
  const size_t kInputDataSize = sizeof(kInputData);  // includes terminating \0
  IpcHandle pipe_read;
  IpcHandle pipe_write;
  ASSERT_TRUE(CreateTestPipe(&pipe_read, &pipe_write));

  std::string error_message;
  ssize_t count = pipe_write.Write(kInputData, kInputDataSize, &error_message);
  EXPECT_EQ(static_cast<ssize_t>(kInputDataSize), count);
  EXPECT_TRUE(error_message.empty());

  char data[kInputDataSize] = {};
  count = pipe_read.Read(data, sizeof(data), &error_message);
  EXPECT_EQ(static_cast<ssize_t>(kInputDataSize), count);
  EXPECT_TRUE(error_message.empty());

  // Check transfered content.
  EXPECT_TRUE(!memcmp(data, kInputData, kInputDataSize));

  // Close the write pipe, trying to read should result in zero
  // being returned without an error (indicating closed pipe).
  pipe_write.Close();
  count = pipe_read.Read(data, kInputDataSize, &error_message);
  EXPECT_EQ(0, count);
  EXPECT_TRUE(error_message.empty());
}

TEST(IpcHandle, ReadWriteFull) {
  const char kInputData[] = "Hello World!";
  const size_t kInputDataSize = sizeof(kInputData);  // includes terminating \0
  IpcHandle pipe_read;
  IpcHandle pipe_write;
  ASSERT_TRUE(CreateTestPipe(&pipe_read, &pipe_write));

  std::string error_message;
  EXPECT_TRUE(pipe_write.WriteFull(kInputData, kInputDataSize, &error_message));
  EXPECT_TRUE(error_message.empty());

  char data[kInputDataSize] = {};
  EXPECT_TRUE(pipe_read.ReadFull(data, kInputDataSize, &error_message));
  EXPECT_TRUE(error_message.empty());

  // Check transfered content.
  EXPECT_TRUE(!memcmp(data, kInputData, kInputDataSize));

  // Close the write pipe, trying to read should result in an error.
  pipe_write.Close();
  EXPECT_FALSE(pipe_read.ReadFull(data, kInputDataSize, &error_message));
  EXPECT_FALSE(error_message.empty());
}

TEST(IpcHandle, BindConnectAndAccept) {
  std::string error_message;
  std::string_view service = "test_service";

  // Tring to connect to a service without a server should fail.
  IpcHandle no_client = IpcHandle::ConnectTo(service, &error_message);
  EXPECT_FALSE(no_client);
  EXPECT_FALSE(error_message.empty());

  // Create server
  IpcServiceHandle server = IpcServiceHandle::BindTo(service, &error_message);
  ASSERT_TRUE(server) << error_message;
  EXPECT_FALSE(error_message.empty());
  error_message.clear();

  // Trying to create a secondary server for the same service should fail.
  IpcServiceHandle no_server =
      IpcServiceHandle::BindTo(service, &error_message);
  ASSERT_FALSE(no_server) << error_message << " " << no_server.native_handle();
  EXPECT_FALSE(error_message.empty());
  error_message.clear();

  // Connecto the server, and receive a peer handle.
  IpcHandle client = IpcHandle::ConnectTo(service, &error_message);
  EXPECT_TRUE(client) << error_message;
  EXPECT_TRUE(error_message.empty());

  IpcHandle peer = server.AcceptClient(&error_message);
  EXPECT_TRUE(peer) << error_message;
  EXPECT_TRUE(error_message.empty());

  // Send data from the client to the peer.
  const char kInput[] = "sending data";
  const size_t kInputSize = sizeof(kInput);
  EXPECT_TRUE(client.WriteFull(kInput, kInputSize, &error_message));
  EXPECT_TRUE(error_message.empty());

  char output[kInputSize] = {};
  EXPECT_TRUE(peer.ReadFull(output, sizeof(output), &error_message));
  EXPECT_TRUE(error_message.empty());

  EXPECT_TRUE(!memcmp(output, kInput, kInputSize));
}

TEST(IpcHandle, SendAndReceiveNativeHandle) {
  IpcHandle pipe1_read;
  IpcHandle pipe1_write;
  ASSERT_TRUE(CreateTestPipe(&pipe1_read, &pipe1_write));

  std::string error_message;
  std::string_view service = "test_service";
  IpcServiceHandle server = IpcServiceHandle::BindTo(service, &error_message);
  ASSERT_TRUE(server) << error_message;
  EXPECT_TRUE(error_message.empty());

  IpcHandle client = IpcHandle::ConnectTo(service, &error_message);
  EXPECT_TRUE(client) << error_message;
  EXPECT_TRUE(error_message.empty());

  IpcHandle peer = server.AcceptClient(&error_message);
  EXPECT_TRUE(peer) << error_message;
  EXPECT_TRUE(error_message.empty());

  // Send pipe1_write.native_handle() from client to peer.
  EXPECT_TRUE(
      client.SendNativeHandle(pipe1_write.native_handle(), &error_message))
      << error_message;
  EXPECT_TRUE(error_message.empty());

  IpcHandle received;
  EXPECT_TRUE(peer.ReceiveNativeHandle(&received, &error_message))
      << error_message;
  EXPECT_TRUE(error_message.empty());

  // Write to |received|, this should send data to the first pipe.
  const char kInputData[] = "Bonjour monde!";
  const size_t kInputDataSize = sizeof(kInputData);

  EXPECT_TRUE(received.WriteFull(kInputData, kInputDataSize, &error_message));
  EXPECT_TRUE(error_message.empty()) << error_message;

  char data[kInputDataSize] = {};
  ssize_t count = pipe1_read.Read(data, sizeof(data), &error_message);
  EXPECT_EQ(static_cast<ssize_t>(sizeof(data)), count);
  EXPECT_TRUE(error_message.empty());

  // Verify content.
  EXPECT_TRUE(!memcmp(data, kInputData, kInputDataSize));
}
