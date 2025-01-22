// Copyright (c) 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_OUTPUT_STREAM_H_
#define TOOLS_GN_OUTPUT_STREAM_H_

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

class OutputStream {
 public:
  virtual ~OutputStream() {}
  virtual void write(const char* str, size_t len) = 0;
  virtual void put(char ch) = 0;

  void write(const char* str) { write(str, ::strlen(str)); }
  void write(const std::string& str) { write(str.data(), str.size()); }

  OutputStream& operator<<(char ch) {
    put(ch);
    return *this;
  }
  OutputStream& operator<<(const char* str) {
    write(str);
    return *this;
  }
  OutputStream& operator<<(const std::string& str) {
    write(str);
    return *this;
  }
  OutputStream& operator<<(const std::string_view& str) {
    write(str.data(), str.size());
    return *this;
  }
  OutputStream& operator<<(int value);
  OutputStream& operator<<(unsigned value);
  OutputStream& operator<<(int64_t value);
  OutputStream& operator<<(uint64_t value);
};

class StringOutputStream : public OutputStream {
 public:
  StringOutputStream() {}
  virtual ~StringOutputStream() {}
  void write(const char* str, size_t len) override { str_.append(str, len); }
  void put(char ch) override { str_.push_back(ch); }
  const std::string str() const { return str_; }

 protected:
  std::string str_;
};

class FileOutputStream : public OutputStream {
 public:
  FileOutputStream(const char* utf8_path);
  virtual ~FileOutputStream();
  bool fail() const;
  void write(const char* str, size_t len) override;
  void put(char ch) override;

 protected:
  FILE* file_ = nullptr;
};

#endif  // TOOLS_GN_OUTPUT_STREAM_H_
