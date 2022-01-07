// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "gn/commands.h"
#include "gn/standard_out.h"

namespace commands {

const char kQuery[] = "query";
const char kQuery_HelpShort[] = "query help short TBA";
const char kQuery_Help[] = "query help TBA";

int RunQuery(const std::vector<std::string>& args) {
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock == -1) {
    Err(Location(),
        std::string("Failed to create client socket: ") + strerror(errno))
        .PrintToStdout();
    return 1;
  }

  struct sockaddr_un saddr = {};
  saddr.sun_family = AF_UNIX;
  memcpy(saddr.sun_path, kServerSockPath, sizeof(kServerSockPath));
  if (connect(sock, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
    Err(Location(),
        std::string("Failed to connect to GN query daemon: ") + strerror(errno))
        .PrintToStdout();
    return 1;
  }

  struct msghdr msgh = {};
  // Don't need to specificy address because we'll establish a connection below.
  msgh.msg_name = NULL;
  msgh.msg_namelen = 0;

  // Construct args payload.
  //
  // TODO(jayzhuang): maybe come up with a better data structure.
  const size_t kBufSize = 4096;
  struct args_data {
    size_t len;
    // args, separated by \0
    char buf[kBufSize];
  } data = {};
  for (const auto& arg : args) {
    const int len = arg.length();
    memcpy(data.buf + data.len, arg.c_str(), len);
    data.buf[data.len + len] = '\0';
    data.len += len + 1;
  }

  struct iovec iov;
  msgh.msg_iov = &iov;
  msgh.msg_iovlen = 1;
  iov.iov_base = &data;
  iov.iov_len = sizeof(data);

  union {
    char buf[CMSG_SPACE(2 * sizeof(int))];
    struct cmsghdr align;
  } control_msg;

  msgh.msg_control = control_msg.buf;
  msgh.msg_controllen = sizeof(control_msg.buf);
  memset(control_msg.buf, 0, sizeof(control_msg.buf));

  struct cmsghdr* cmsgp = CMSG_FIRSTHDR(&msgh);
  cmsgp->cmsg_len = CMSG_LEN(2 * sizeof(int));
  cmsgp->cmsg_level = SOL_SOCKET;
  cmsgp->cmsg_type = SCM_RIGHTS;
  ((int*)CMSG_DATA(cmsgp))[0] = STDOUT_FILENO;
  ((int*)CMSG_DATA(cmsgp))[1] = STDERR_FILENO;

  if (sendmsg(sock, &msgh, 0) == -1) {
    Err(Location(),
        std::string("Failed to send arguments to daemon: ") + strerror(errno))
        .PrintToStdout();
    return 1;
  }

  // There's nothing to receive, wait for server to close the connection to
  // synchronize.
  if (recv(sock, NULL, 0, 0) == -1) {
    Err(Location(),
        std::string("Failed to wait for close signal from daemon: ") +
            strerror(errno))
        .PrintToStdout();
    return 1;
  };

  return 0;
}

}  // namespace commands
