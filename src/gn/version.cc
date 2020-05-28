// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/version.h"
#include <iostream>

#include "base/strings/string_number_conversions.h"

const std::string kDot = ".";

Version::Version(int major, int minor, int patch)
    : major_(major), minor_(minor), patch_(patch) {}

// static
std::optional<Version> Version::FromString(std::string s) {
  int major = 0, minor = 0, patch = 0;
  // First, parse the major version.
  size_t major_pos = 0;
  if (size_t pos = s.find(kDot, major_pos); pos != std::string::npos) {
    if (!base::StringToInt(s.substr(major_pos, pos - major_pos), &major))
      return {};
    // Then, parse the minor version.
    size_t minor_pos = pos + kDot.size();
    if (size_t pos = s.find(kDot, minor_pos); pos != std::string::npos) {
      if (!base::StringToInt(s.substr(minor_pos, pos - minor_pos), &minor))
        return {};
      // Finally, parse the patch version.
      size_t patch_pos = pos + kDot.size();
      if (!base::StringToInt(s.substr(patch_pos, std::string::npos), &patch))
        return {};
      return Version(major, minor, patch);
    }
  }
  return {};
}

bool Version::operator==(const Version& other) const {
  return other.major_ == major_ && other.minor_ == minor_ &&
         other.patch_ == patch_;
}

bool Version::operator<(const Version& other) const {
  return std::tie(major_, minor_, patch_) <
         std::tie(other.major_, other.minor_, other.patch_);
}

bool Version::operator!=(const Version& other) const {
  return !(*this == other);
}

bool Version::operator>=(const Version& other) const {
  return !(*this < other);
}

bool Version::operator>(const Version& other) const {
  return other < *this;
}

bool Version::operator<=(const Version& other) const {
  return !(*this > other);
}

std::string Version::Describe() const {
  std::string ret;
  ret += base::IntToString(major_);
  ret += kDot;
  ret += base::IntToString(minor_);
  ret += kDot;
  ret += base::IntToString(patch_);
  return ret;
}
