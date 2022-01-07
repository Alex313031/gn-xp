// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "gn/commands.h"
#include "gn/setup.h"
#include "gn/standard_out.h"

namespace commands {

namespace {

std::vector<std::string> SplitArgs(const char* args, int len) {
  std::vector<std::string> str_args;
  std::string arg;
  for (int i = 0; i < len; i++) {
    if (args[i] == '\0') {
      str_args.push_back(arg);
      arg = "";
      continue;
    }
    arg += args[i];
  }
  return str_args;
}

void HandleClientRequest(int client_fd, Setup* setup) {
  if (client_fd == -1) {
    Err(Location(),
        std::string("Failed to accept client connection: ") + strerror(errno))
        .PrintToStdout();
    return;
  }

  const size_t kBufSize = 4096;
  char buf[kBufSize];
  int num_bytes_read = recv(client_fd, buf, kBufSize, 0);
  if (num_bytes_read == -1) {
    Err(Location(),
        std::string("Failed to receive args from client: ") + strerror(errno))
        .PrintToStdout();
    return;
  }

  std::vector<std::string> str_args = SplitArgs(buf, num_bytes_read);
  str_args.insert(str_args.begin(), "-");  // To match gn desc args format.

  // TODO(jayzhuang): fix error propagation.
  std::string res = RunDescWithSetup(str_args, setup);
  if (send(client_fd, res.c_str(), res.length(), 0) == -1) {
    Err(Location(),
        std::string("Failed to send end signal to client: ") + strerror(errno))
        .PrintToStdout();
    return;
  }
}

int StartServerSocket() {
  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd == -1) {
    Err(Location(),
        std::string("Failed to create server socket: ") + strerror(errno))
        .PrintToStdout();
    return -1;
  }

  struct sockaddr_un saddr = {};
  saddr.sun_family = AF_UNIX;
  memcpy(saddr.sun_path, kServerSockPath, sizeof(kServerSockPath));
  if (bind(server_fd, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
    Err(Location(), std::string("Failed to bind server socket to path ") +
                        kServerSockPath + ": " + strerror(errno))
        .PrintToStdout();
    return -1;
  }

  if (listen(server_fd, 10) == -1) {
    Err(Location(),
        std::string("Failed to listen with server socket at path ") +
            kServerSockPath + ": " + strerror(errno))
        .PrintToStdout();
    return -1;
  }

  return server_fd;
}

int StartServerLoop(int server_fd, Setup* setup) {
  OutputString("Server listening...\n");

  int client_fd;
  while (true) {
    client_fd = accept(server_fd, NULL, NULL);
    HandleClientRequest(client_fd, setup);
    if (close(client_fd) == -1) {
      // Report close failures but don't exit server.
      Err(Location(),
          std::string("Failed to close client connection: ") + strerror(errno))
          .PrintToStdout();
    }
  }

  return 0;
}
}  // namespace

const char kStartServer[] = "start_server";
const char kStartServer_HelpShort[] = "start_server short help TBA";
const char kStartServer_Help[] = "start_server help TBA";

int RunStartServer(const std::vector<std::string>& args) {
  printf("Loading GN files...\n");
  Setup* setup = new Setup;
  if (!setup->DoSetup(args[0], false)) {
    return 1;
  }
  if (!setup->Run()) {
    return 1;
  }

  // Do setup and stuff.
  int server_fd = StartServerSocket();
  if (server_fd == -1) {
    return 1;
  }
  return StartServerLoop(server_fd, setup);
}
}  // namespace commands
