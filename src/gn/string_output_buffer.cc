// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/string_output_buffer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "gn/err.h"
#include "gn/filesystem_utils.h"
#include "util/build_config.h"

#include <fstream>

#if defined(OS_WIN)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

// FileWriter is a helper class that is used to write to a file with
// multiple Write() calls. Usage is the following:
//
//   1) Create instance, passing the file path.
//   2) Call Create() method to create the file, this returns true on success.
//   3) Call Write() one or more times to append data to the file.
//   4) Call Close() to close the file and verify that no error occured.
//
// The logic is similar to the one used in WriteFile(), in particular for
// Windows (see comment below).
//
#if defined(OS_WIN)

class FileWriter {
 public:
  bool Create(const base::FilePath& file_path) {
    // On Windows, provide a custom implementation of base::WriteFile. Sometimes
    // the base version fails, especially on the bots. The guess is that Windows
    // Defender or other antivirus programs still have the file open (after
    // checking for the read) when the write happens immediately after. This
    // version opens with FILE_SHARE_READ (normally not what you want when
    // replacing the entire contents of the file) which lets us continue even if
    // another program has the file open for reading. See
    // http://crbug.com/468437
    file_ = base::win::ScopedHandle(::CreateFile(
        reinterpret_cast<LPCWSTR>(file_path_.value().c_str()), GENERIC_WRITE,
        FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL));

    valid_ = file_.IsValid();
    if (!valid_) {
      PLOG(ERROR) << "CreateFile failed for path "
                  << base::UTF16ToUTF8(file_path.value());
    }

    return valid_;
  }

  bool Write(std::string_view str) {
    if (!valid_)
      return false;

    DWORD written;
    BOOL result =
        ::WriteFile(file_.Get(), str.data(), str.size(), &written, nullptr);
    if (!result) {
      PLOG(ERROR) << "writing file " << base::UTF16ToUTF8(file_path_.value())
                  << "failed";
      valid_ = false;
      return false;
    }
    if (static_cast<size_t>(written) != str.size()) {
      PLOG(ERROR) << "wrote " << written << " bytes to "
                  << base::UTF16ToUTF8(file_path_.value()) << " expected "
                  << str.size();
      valid_ = false;
      return false;
    }
    return true;
  }

  bool Close() {
    // NOTE: ScopedFileHandle::Close() does not return a success flag.
    HANDLE handle = file_.Take();
    if (handle && !::CloseHandle(handle))
      return false;

    return valid_;
  }

 private:
  base::win::ScopedHandle file_;
  bool valid_ = true;
};

#else   // !OS_WIN

class FileWriter {
 public:
  ~FileWriter() {
    if (fd_ >= 0) {
      IGNORE_EINTR(::close(fd_));
    }
  }

  bool Create(const base::FilePath& file_path) {
    fd_ = HANDLE_EINTR(creat(file_path.value().c_str(), 0666));
    valid_ = (fd_ >= 0);
    return valid_;
  }

  bool Write(std::string_view str) {
    if (!valid_)
      return false;

    while (!str.empty()) {
      ssize_t written = HANDLE_EINTR(write(fd_, str.data(), str.size()));
      if (written <= 0) {
        valid_ = false;
        return false;
      }
      str.remove_prefix(static_cast<size_t>(written));
    }
    return true;
  }

  bool Close() {
    int fd = fd_;
    fd_ = -1;
    if (fd >= 0 && (IGNORE_EINTR(::close(fd)) < 0))
      return false;

    return valid_;
  }

 private:
  int fd_ = -1;
  bool valid_ = true;
};
#endif  // !OS_WIN

}  // namespace

std::string StringOutputBuffer::str() const {
  std::string result;
  size_t data_size = size();
  result.reserve(data_size);
  for (size_t nn = 0; nn < pages_.size(); ++nn) {
    size_t wanted_size = std::min(kPageSize, data_size - nn * kPageSize);
    result.append(pages_[nn]->data(), wanted_size);
  }
  return result;
}

void StringOutputBuffer::Append(const char* str, size_t len) {
  Append(std::string_view(str, len));
}

void StringOutputBuffer::Append(std::string_view str) {
  while (str.size() > 0) {
    if (page_free_size() == 0) {
      // Allocate a new page.
      pages_.push_back(std::make_unique<Page>());
      pos_ = 0;
    }
    size_t size = std::min(page_free_size(), str.size());
    memcpy(pages_.back()->data() + pos_, str.data(), size);
    pos_ += size;
    str.remove_prefix(size);
  }
}

void StringOutputBuffer::Append(char c) {
  if (page_free_size() == 0) {
    // Allocate a new page.
    pages_.push_back(std::make_unique<Page>());
    pos_ = 0;
  }
  pages_.back()->data()[pos_] = c;
  pos_ += 1;
}

bool StringOutputBuffer::ContentsEqual(const base::FilePath& file_path) const {
  // Compare file and stream sizes first. Quick and will save us some time if
  // they are different sizes.
  size_t data_size = size();
  int64_t file_size;
  if (!base::GetFileSize(file_path, &file_size) ||
      static_cast<size_t>(file_size) != data_size) {
    return false;
  }

  // Open the file in binary mode.
  std::ifstream file(file_path.As8Bit().c_str(), std::ios::binary);
  if (!file.is_open())
    return false;

  size_t page_count = pages_.size();
  Page file_page;
  for (size_t nn = 0; nn < page_count; ++nn) {
    size_t wanted_size = std::min(data_size - nn * kPageSize, kPageSize);
    file.read(file_page.data(), wanted_size);
    if (!file.good())
      return false;

    if (memcmp(file_page.data(), pages_[nn]->data(), wanted_size) != 0)
      return false;
  }
  return true;
}

// Write the contents of this instance to a file at |file_path|.
bool StringOutputBuffer::WriteToFile(const base::FilePath& file_path,
                                     Err* err) const {
  // Create the directory if necessary.
  if (!base::CreateDirectory(file_path.DirName())) {
    if (err) {
      *err =
          Err(Location(), "Unable to create directory.",
              "I was using \"" + FilePathToUTF8(file_path.DirName()) + "\".");
    }
    return false;
  }

  size_t data_size = size();
  size_t page_count = pages_.size();

  FileWriter writer;
  bool success = writer.Create(file_path);
  if (success) {
    for (size_t nn = 0; nn < page_count; ++nn) {
      size_t wanted_size = std::min(data_size - nn * kPageSize, kPageSize);
      success = writer.Write(std::string_view(pages_[nn]->data(), wanted_size));
      if (!success)
        break;
    }
  }
  if (!writer.Close())
    success = false;

  if (!success && err) {
    *err = Err(Location(), "Unable to write file.",
               "I was writing \"" + FilePathToUTF8(file_path) + "\".");
  }
  return success;
}
