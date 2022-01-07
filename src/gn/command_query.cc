// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "gn/commands.h"
#include "gn/err.h"
#include "gn/filesystem_utils.h"
#include "gn/setup.h"
#include "gn/standard_out.h"
#include "util/ipc_handle.h"
#include "util/stdio_redirect.h"

namespace commands {

namespace {

// Use GN's error formatter for printing error messages.
void PrintError(const std::string& message) {
  Err(Location(), message).PrintToStdout();
}

// Helper class to temporarily change the state of the "is_console"
// global flag for standard output.
class GlobalConsoleSettings {
 public:
  GlobalConsoleSettings(bool is_console)
      : previous_(SetStandardOutputConsole(is_console)) {}

  ~GlobalConsoleSettings() { SetStandardOutputConsole(previous_); }

 private:
  bool previous_;
};

// Joins a vector of strings into a character array using
// '\0' as the separator. Assumes that none of the strings
// contain a NUL char. This will be sent via IPC to the
// server process.
std::string JoinArgs(std::vector<std::string>::const_iterator begin,
                     std::vector<std::string>::const_iterator end) {
  std::string result;
  for (bool first = true; begin != end; ++begin, first = false) {
    if (!first)
      result.push_back('\0');
    result.append(*begin);
  }
  return result;
}

// Split a character array that uses '\0' as a separator into
// a vector of strings. This reverses JoinArgs().
std::vector<std::string> SplitArgs(std::string_view args) {
  std::vector<std::string> result;
  size_t zero;
  while ((zero = args.find('\0')) != args.npos) {
    result.push_back(std::string(args.substr(0, zero)));
    args.remove_prefix(zero + 1);
  }
  if (!args.empty())
    result.push_back(std::string(args));
  return result;
}

bool ReadString(const IpcHandle& handle,
                std::string* str,
                std::string* error_message) {
  size_t size;
  if (!handle.ReadFull(&size, sizeof(size), error_message))
    return false;
  str->resize(size);
  return handle.ReadFull(&(*str)[0], size, error_message);
}

bool WriteString(const IpcHandle& handle,
                 const std::string& str,
                 std::string* error_message) {
  size_t size = str.size();
  return handle.WriteFull(&size, sizeof(size), error_message) &&
         handle.WriteFull(str.data(), size, error_message);
}

// Return the service name to use for client/server connections.
// This uses a hash suffix of the output directory (as an
// absolute path), to allow several servers to run on the
// same machine, from different directories.
std::string GetServiceName(const std::string& output_dir) {
  base::FilePath absolute = MakeAbsoluteFilePath(UTF8ToFilePath(output_dir));
  size_t hash = std::hash<base::FilePath>()(absolute);
  return base::StringPrintf("gn-%08x", static_cast<unsigned>(hash));
}

// Type of a function that implements a query command with a
// pre-initialized Setup object. Return 0 on success, 1 on failure.
using QueryFunction = int(const std::vector<std::string>&, Setup*);

// The list of valid supported queries and their implementation function.
const struct {
  const char* name;
  QueryFunction* function;
} kValidQueries[] = {
    {"analyze", &RunAnalyze}, {"desc", &RunDesc}, {"refs", &RunRefs},
    {"path", &RunPath},       {"meta", &RunMeta}, {"outputs", &RunOutputs},
    {"ls", &RunLs},
};

// Called from the query server loop to serve one client
// request. On success return true. On failure return false
// and set |*error_message|. Note that these error messages
// are to be printed on the server's regular error output,
// while the query command errors themselves will be printed
// to the client's own stderr.
//
// As a special case, if the query command is "kill-server"
// then this returns false and sets |*error_message| to
// "KILL_SERVER".
bool HandleQueryClientRequest(IpcHandle client,
                              Setup* setup,
                              std::string* error_message) {
  IpcHandle client_stdin;
  IpcHandle client_stdout;
  IpcHandle client_stderr;
  bool is_console;
  if (!client.ReceiveNativeHandle(&client_stdin, error_message) ||
      !client.ReceiveNativeHandle(&client_stdout, error_message) ||
      !client.ReceiveNativeHandle(&client_stderr, error_message) ||
      !client.ReadFull(&is_console, sizeof(is_console), error_message)) {
    return false;
  }

  GlobalConsoleSettings console_settings(is_console);

  std::vector<std::string> args;
  {
    std::string message;
    if (!ReadString(client, &message, error_message))
      return false;

    args = SplitArgs(message);
  }

  if (args.empty()) {
    *error_message = "Missing query sub-command";
    return false;
  }

  // Special case for kill-server.
  if (args[0] == "kill-server") {
    *error_message = "KILL_SERVER";
    return false;
  }

  CommandSwitches switches;
  {
    std::string wire;
    if (!ReadString(client, &wire, error_message))
      return false;

    switches = CommandSwitches::FromWire(std::move(wire));
  }
  CommandSwitches::Set(std::move(switches));

  QueryFunction* query_func = nullptr;
  for (const auto& valid_query : kValidQueries) {
    if (args[0] == valid_query.name) {
      query_func = valid_query.function;
      break;
    }
  }
  // Execute the query function after ensuring that
  // stdout/stderr are properly redirected to the handles
  // provided by the client.
  StdioRedirect stderr_redirect(StdioRedirect::StdType::STD_TYPE_ERR,
                                client_stderr.native_handle());
  StdioRedirect stdout_redirect(StdioRedirect::StdType::STD_TYPE_OUT,
                                client_stdout.native_handle());
  StdioRedirect stdin_redirect(StdioRedirect::StdType::STD_TYPE_IN,
                               client_stdin.native_handle());

  if (!query_func) {
    PrintError(base::StringPrintf(
        "Invalid query sub-command '%s'. See `gn help query`.",
        args[0].c_str()));
    *error_message = "Invalid query command";
    return false;
  }

  args.erase(args.begin());
  if (query_func(args, setup) != 0) {
    *error_message = "Failed to run command";
    return false;
  }
  return true;
}

// Run `gn query start-server <output_dir>`
// On success return 0, on failure return 1.
int RunQueryStartServer(const std::string& output_dir,
                        const std::string& service_name) {
  // Bind to the service to ensure no one grabs it.
  std::string error_message;
  IpcServiceHandle server =
      IpcServiceHandle::BindTo(service_name, &error_message);
  if (!server) {
    PrintError(error_message);
    return 1;
  }

  // Load the input files and build the target graph.
  OutputString("Loading GN files...\n");
  auto setup = std::make_unique<Setup>();
  if (!setup->DoSetup(output_dir, false)) {
    return 1;
  }
  if (!setup->Run()) {
    return 1;
  }

#ifndef _WIN32
  SigPipeIgnore sigpipe_ignore;
#endif

  // Now wait for client connections. This never stops for now,
  // i.e. the user should force-quit the query server.
  // TODO(digit): Add stop-server command.
  OutputString("Server listening...\n");
  while (true) {
    IpcHandle client = server.AcceptClient(&error_message);
    if (!client) {
      PrintError(error_message);
      continue;
    }
    int status = 0;
    if (!HandleQueryClientRequest(std::move(client), setup.get(),
                                  &error_message)) {
      if (error_message == "KILL_SERVER")
        break;

      PrintError(error_message);
      status = 1;
    }
    client.WriteFull(&status, sizeof(status), &error_message);
  }
  return 0;
}

}  // namespace

const char kQuery[] = "query";
const char kQuery_HelpShort[] =
    "query: Perform fast queries over the build graph.";
const char kQuery_Help[] =
    R"(gn query

  gn query start-server <out_dir>
  gn query kill-server <out_dir>
  gn query <subcommand> <out_dir> ...

  A feature that allows very fast multiple queries over the same build graph.

  Use `gn query start-server <out_dir>` first to start a GN process that will
  load the build graph in memory (this make take a few seconds), then will
  wait for client connections.

  In another terminal, use `gn query <subcommand> <out_dir> ...` where
  <subcommand> is one of the GN readonly query commands (e.g. `desc`, `refs`),
  followed by the arguments you would pass. For example:

  Only one server per <out_dir> can run on the same machine. Each server
  responds to one client query at a time. Changes to input build files that
  happen after the initial load do not affect a server's in-memory graph.

  Servers can be stopped manually (e.g. with Ctrl-C), or by using
  the `gn query kill-server <out_dir>` command.

Example

    # In first terminal
    gn query start-server <out_dir>

    # In second terminal
    gn query desc <out_dir> //:default deps --tree
    gn query refs <out_dir> //some/dir:foo
)";

int RunQuery(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    PrintError(
        "query command requires at least two arguments. See `gn help query`.");
    return 1;
  }

  const std::string& command = args[0];
  const std::string& output_dir = args[1];
  std::string service_name = GetServiceName(output_dir);

  if (command == "start-server") {
    if (args.size() > 2) {
      PrintError(
          "The start-server sub-command only takes a single argument. See `gn "
          "help query`.");
      return 1;
    }
    return RunQueryStartServer(output_dir, service_name);
  }

  // A client query, send the sub-command to the server, if any.
  std::string error_message;
  IpcHandle client = IpcHandle::ConnectTo(service_name, &error_message);
  if (!client) {
    PrintError(std::string("Could not connect to query server, did you run `gn "
                           "query start-server ") +
               output_dir + "` ?");
    return 1;
  }

  // Ensure stdout/stderr handles are sent to the server process.
#ifdef _WIN32
  Win32StdHandleBridge stdin_bridge;
  Win32StdHandleBridge stdout_bridge;
  Win32StdHandleBridge stderr_bridge;
  if (!stdout_bridge.Init(0, &error_message) ||
      !stderr_bridge.Init(1, &error_message) ||
      !stderr_bridge.Init(2, &error_message) ||
      client.SendNativeHandle(stdin_bridge.handle(), &error_message) ||
      client.SendNativeHandle(stdout_bridge.handle(), &error_message) ||
      client.SendNativeHandle(stderr_bridge.handle(), &error_message)) {
    PrintError(std::string("Bad query server connection: ") + error_message);
    return 1;
  }
#else   // !_WIN32
  if (!client.SendNativeHandle(0, &error_message) ||
      !client.SendNativeHandle(1, &error_message) ||
      !client.SendNativeHandle(2, &error_message)) {
    PrintError(std::string("Bad query server connection: ") + error_message);
    return 1;
  }
#endif  // !_WIN32

  bool is_console = IsStandardOutputConsole();
  if (!client.WriteFull(&is_console, sizeof(is_console), &error_message)) {
    PrintError(std::string("Bad query server connection: ") + error_message);
    return 1;
  }

  std::string message = JoinArgs(args.begin(), args.end());
  if (!WriteString(client, message, &error_message)) {
    PrintError(std::string("Bad query server connection: ") + error_message);
    return 1;
  }

  std::string switches = CommandSwitches::Get().ToWire();
  if (!WriteString(client, switches, &error_message)) {
    PrintError(std::string("Bad query server connection: ") + error_message);
    return 1;
  }

  int status;
  if (!client.ReadFull(&status, sizeof(status), &error_message))
    status = 1;

  return status;
}

}  // namespace commands
