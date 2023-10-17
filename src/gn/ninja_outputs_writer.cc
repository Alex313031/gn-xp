// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_outputs_writer.h"

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/string_escape.h"
#include "gn/builder.h"
#include "gn/commands.h"
#include "gn/filesystem_utils.h"
#include "gn/settings.h"
#include "gn/string_output_buffer.h"

// NOTE: Intentional macro definition allows compile-time string concatenation.
// (see usage below).
#if defined(OS_WINDOWS)
#define LINE_ENDING "\r\n"
#else
#define LINE_ENDING "\n"
#endif

namespace {

using MapType = NinjaOutputsWriter::MapType;

// Sort the targets according to their human visible labels first.
struct TargetLabelPair {
  TargetLabelPair(const Target* target, const Label& default_toolchain_label)
      : target(target),
        label(std::make_unique<std::string>(
            target->label().GetUserVisibleName(default_toolchain_label))) {}

  const Target* target;
  std::unique_ptr<std::string> label;

  bool operator<(const TargetLabelPair& other) const {
    return *label < *other.label;
  }

  using List = std::vector<TargetLabelPair>;

  // Create list of TargetLabelPairs sorted by their target labels.
  static List CreateSortedList(const MapType& outputs_map,
                               const Label& default_toolchain_label) {
    List result;
    result.reserve(outputs_map.size());

    for (const auto& output_pair : outputs_map)
      result.emplace_back(output_pair.first, default_toolchain_label);

    std::sort(result.begin(), result.end());
    return result;
  }
};

size_t ShellEscapeSize(char ch) {
  switch (ch) {
    case '\a':
    case '\b':
    case '\x1c':  // a.k.a. \e, which is not supported by MSVC.
    case '\f':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
    case '\\':
    case '\'':
    case '\"':
    case '?':
    case '*':
      return 2;  // \<char>
    default:
      if (ch < 32 || ch > 127)
        return 4;  // \x00
      else
        return 0;
  }
}

void AppendShellEscape(std::string& out, char ch) {
  switch (ch) {
    case '\a':
      out.append("\\a");
      break;
    case '\b':
      out.append("\\b");
      break;
    case '\x1c':
      out.append("\\e");
      break;
    case '\f':
      out.append("\\f");
      break;
    case '\n':
      out.append("\\n");
      break;
    case '\r':
      out.append("\\r");
      break;
    case '\t':
      out.append("\\t");
      break;
    case '\v':
      out.append("\\v");
      break;
    case '\\':
      out.append("\\\\");
      break;
    case '\'':
      out.append("\\'");
      break;
    case '\"':
      out.append("\\\"");
      break;
    case '?':
      out.append("\\?");
      break;
    case '*':
      out.append("\\*");
      break;
    default:
      if (ch < 32 || ch > 127) {
        const char hex[] = "0123456789abcdef";
        out.append("\\x");
        out.push_back(hex[(ch >> 4) & 15]);
        out.push_back(hex[ch & 15]);
      } else {
        out.push_back(ch);
      }
  }
}

// Benchmarking shows that it is considerably faster to compute the
// size of the tabular output before writing it to the StringOutputBuffer
// (e.g. 175ms vs 442ms).
//
// On the other hand, doing the same for the JSON generator seems to make
// things go slower (606ms vs 577ms), so do not do it.
struct CountingOutput {
  void append_char(char) { size_ += 1; }
  void append(const std::string& str) { size_ += str.size(); }
  size_t size() const { return size_; }

 private:
  size_t size_ = 0;
};

struct StringOutput {
  StringOutput(size_t capacity) { str_.reserve(capacity); }
  void append_char(char ch) { str_.push_back(ch); }
  void append(const std::string& str) { str_ += str; }
  std::string take_string() { return std::move(str_); }

  std::string str_;
};

template <typename OUT>
void GenerateTabular(const MapType& outputs_map,
                     const TargetLabelPair::List& sorted_pairs,
                     OUT& out) {
  for (const auto& pair : sorted_pairs) {
    const Target* target = pair.target;
    const std::string& label = *pair.label;

    auto it = outputs_map.find(target);
    CHECK(it != outputs_map.end());

    out.append(label);
    for (const auto& output : it->second) {
      const std::string& path = output.value();
      size_t escapes = 0;
      for (char ch : path) {
        escapes += ShellEscapeSize(ch);
      }
      if (escapes == 0) {
        out.append_char('\t');
        out.append(path);
        return;
      }
      std::string escaped;
      escaped.reserve(path.size() + escapes);
      for (char ch : path) {
        AppendShellEscape(escaped, ch);
      }
      out.append_char('\t');
      out.append(escaped);
    }
    out.append_char('\n');
  }
}

StringOutputBuffer GenerateTabular(const MapType& outputs_map,
                                   const TargetLabelPair::List& sorted_pairs) {
  StringOutputBuffer out;

  CountingOutput counting;
  GenerateTabular(outputs_map, sorted_pairs, counting);

  StringOutput str_out(counting.size());
  GenerateTabular(outputs_map, sorted_pairs, str_out);

  out << str_out.take_string();
  return out;
}

StringOutputBuffer GenerateJson(const MapType& outputs_map,
                                const TargetLabelPair::List& sorted_pairs) {
  StringOutputBuffer out;

  out << "{";

  auto escape = [](std::string_view str) -> std::string {
    std::string result;
    base::EscapeJSONString(str, true, &result);
    return result;
  };

  bool first_label = true;
  for (const auto& pair : sorted_pairs) {
    const Target* target = pair.target;
    const std::string& label = *pair.label;

    auto it = outputs_map.find(target);
    CHECK(it != outputs_map.end());

    if (!first_label)
      out << ",";
    first_label = false;

    out << "\n  " << escape(label) << ": [";
    bool first_path = true;
    for (const auto& output : it->second) {
      if (!first_path)
        out << ",";
      first_path = false;
      out << "\n    " << escape(output.value());
    }
    out << "\n  ]";
  }

  out << "\n}";
  return out;
}

}  // namespace

bool NinjaOutputsWriter::RunAndWriteFiles(const MapType& outputs_map,
                                          const BuildSettings* build_settings,
                                          const std::string& file_name,
                                          FileFormat file_format,
                                          Err* err) {
  SourceFile output_file = build_settings->build_dir().ResolveRelativeFile(
      Value(nullptr, file_name), err);
  if (output_file.is_null()) {
    return false;
  }

  base::FilePath output_path = build_settings->GetFullPath(output_file);

  Label default_toolchain_label;
  if (!outputs_map.empty()) {
    default_toolchain_label =
        outputs_map.begin()->first->settings()->default_toolchain_label();
  }

  auto sorted_pairs =
      TargetLabelPair::CreateSortedList(outputs_map, default_toolchain_label);

  StringOutputBuffer outputs = file_format == kFileFormatJSON
                                   ? GenerateJson(outputs_map, sorted_pairs)
                                   : GenerateTabular(outputs_map, sorted_pairs);

  if (!outputs.ContentsEqual(output_path)) {
    if (!outputs.WriteToFile(output_path, err)) {
      return false;
    }
  }

  return true;
}
