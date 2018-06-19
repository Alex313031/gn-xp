// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "file_util.h"

#include <sys/stat.h>

#include "base/logging.h"
#include "build_config.h"

#if defined(OS_WIN)
#include <windows.h>

#include <direct.h>
#else
#include <unistd.h>
#endif

bool CreateDirectory(const base::FilePath& full_path) {
#if defined(OS_WIN)
  return _mkdir(full_path.value().c_str());
#else
  return mkdir(full_path.value().c_str(), 0777);
#endif
}

bool CreateNewTempDirectory(const base::FilePath::StringType& prefix,
                            base::FilePath* new_temp_path) {
  CHECK(false);
  return false;
}

bool CreateTemporaryDirInDir(const base::FilePath& base_dir,
                             const base::FilePath::StringType& prefix,
                             base::FilePath* new_dir) {
  CHECK(false);
  return false;
}

bool DeleteFile(const base::FilePath& path, bool recursive) {
  CHECK(false);
  return false;
}

bool DirectoryExists(const base::FilePath& path) {
  CHECK(false);
  return false;
}

bool GetCurrentDirectory(base::FilePath* path) {
  CHECK(false);
  return false;
}

bool GetFileSize(const base::FilePath& file_path, int64_t* file_size) {
  CHECK(false);
  return false;
}

bool GetLastModified(const base::FilePath& file_path, time_t* mtime) {
  CHECK(false);
  return false;
}

base::FilePath MakeAbsoluteFilePath(const base::FilePath& input) {
  CHECK(false);
  return base::FilePath();
}

bool PathExists(const base::FilePath& path) {
  CHECK(false);
  return false;
}

bool ReadFileToString(const base::FilePath& path, std::string* contents) {
  FILE* f = fopen(path.value().c_str(), "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    contents->resize(ftell(f));
    rewind(f);
    fread(&(*contents)[0], 1, contents->size(), f);
    fclose(f);
    return true;
  }
  return false;
}

#if defined(OS_POSIX)
bool ReadSymbolicLink(const base::FilePath& symlink_path,
                      base::FilePath* target_path) {
  DCHECK(!symlink_path.empty());
  DCHECK(target_path);
  char buf[260];
  ssize_t count = ::readlink(symlink_path.value().c_str(), buf, arraysize(buf));

  if (count <= 0) {
    target_path->clear();
    return false;
  }

  *target_path = base::FilePath(base::FilePath::StringType(buf, count));
  return true;
}
#endif

bool SetCurrentDirectory(const base::FilePath& path) {
#if defined(OS_WIN)
  return ::SetCurrentDirectory(directory.value().c_str()) != 0;
#else
  return chdir(path.value().c_str()) == 0;
#endif
}

int WriteFile(const base::FilePath& filename, const char* data, int size) {
  CHECK(false);
  return 0;
}
