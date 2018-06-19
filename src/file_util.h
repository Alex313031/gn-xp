// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FILE_UTIL_H_
#define FILE_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "build_config.h"

bool CreateDirectory(const base::FilePath& full_path);

bool CreateNewTempDirectory(const base::FilePath::StringType& prefix,
                            base::FilePath* new_temp_path);

bool CreateTemporaryDirInDir(const base::FilePath& base_dir,
                             const base::FilePath::StringType& prefix,
                             base::FilePath* new_dir);

bool DeleteFile(const base::FilePath& path, bool recursive);

bool DirectoryExists(const base::FilePath& path);

bool GetCurrentDirectory(base::FilePath* path);

bool GetFileSize(const base::FilePath& file_path, int64_t* file_size);

bool GetLastModified(const base::FilePath& file_path, time_t* mtime);

base::FilePath MakeAbsoluteFilePath(const base::FilePath& input);

bool PathExists(const base::FilePath& path);

bool ReadFileToString(const base::FilePath& path, std::string* contents);

#if defined(OS_POSIX)
bool ReadSymbolicLink(const base::FilePath& symlink_path,
                      base::FilePath* target_path);
#endif

bool SetCurrentDirectory(const base::FilePath& path);

int WriteFile(const base::FilePath& filename, const char* data, int size);

#endif  // FILE_UTIL_H_
