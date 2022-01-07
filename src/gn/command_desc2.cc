// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "gn/commands.h"
#include "gn/standard_out.h"

namespace commands {

const char kDesc2[] = "desc2";
const char kDesc2_HelpShort[] = "desc2 help short TBA";
const char kDesc2_Help[] = "desc2 help TBA";

int RunDesc2(const std::vector<std::string>& args) {
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock == -1) {
    Err(Location(),
        std::string("Failed to create client socket: ") + strerror(errno))
        .PrintToStdout();
    return 1;
  }

  printf("Connecting ...\n");

  struct sockaddr_un saddr = {};
  saddr.sun_family = AF_UNIX;
  memcpy(saddr.sun_path, kServerSockPath, sizeof(kServerSockPath));
  if (connect(sock, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
    Err(Location(),
        std::string("Failed to connect to GN query daemon: ") + strerror(errno))
        .PrintToStdout();
    return 1;
  }

  const size_t kBufSize = 4096;
  char buf[kBufSize];

  int tot = 0;
  int len;
  for (const auto& arg : args) {
    len = arg.length();
    memcpy(buf + tot, arg.c_str(), len);
    buf[tot + len] = '\0';
    tot += len + 1;
  }
  printf("Sending concatenated args '%s'\n", buf);
  if (send(sock, buf, tot, 0) == -1) {
    Err(Location(),
        std::string("Failed to send arguments to daemon: ") + strerror(errno))
        .PrintToStdout();
    return 1;
  }

  int num_bytes_read = recv(sock, buf, kBufSize, 0);
  if (num_bytes_read == -1) {
    Err(Location(), std::string("Failed to receive response from daemon: ") +
                        strerror(errno))
        .PrintToStdout();
    return 1;
  }
  buf[num_bytes_read] = '\0';
  OutputString("\n================ Server Response ================\n\n");
  OutputString(buf);

  return 0;
}

}  // namespace commands
