// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/script_runners.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "gn/build_settings.h"
#include "gn/filesystem_utils.h"
#include "gn/value.h"

using namespace std::literals;

namespace {

#if defined(OS_WIN)

std::u16string SysMultiByteTo16(std::string_view mb) {
  if (mb.empty())
    return std::u16string();

  int mb_length = static_cast<int>(mb.length());
  // Compute the length of the buffer.
  int charcount = MultiByteToWideChar(CP_ACP, 0, mb.data(), mb_length, NULL, 0);
  if (charcount == 0)
    return std::u16string();

  std::u16string wide;
  wide.resize(charcount);
  MultiByteToWideChar(CP_ACP, 0, mb.data(), mb_length, base::ToWCharT(&wide[0]),
                      charcount);

  return wide;
}

// Given the path to a batch file that runs Python, extracts the name of the
// executable actually implementing Python. Generally people write a batch file
// to put something named "python" on the path, which then just redirects to
// a python.exe somewhere else. This step decodes that setup. On failure,
// returns empty path.
base::FilePath PythonBatToExe(const base::FilePath& bat_path) {
  // Note exciting double-quoting to allow spaces. The /c switch seems to check
  // for quotes around the whole thing and then deletes them. If you want to
  // quote the first argument in addition (to allow for spaces in the Python
  // path, you need *another* set of quotes around that, likewise, we need
  // two quotes at the end.
  std::u16string command = u"cmd.exe /c \"\"";
  command.append(bat_path.value());
  command.append(u"\" -c \"import sys; print sys.executable\"\"");

  std::string python_path;
  std::string std_err;
  int exit_code;
  base::FilePath cwd;
  GetCurrentDirectory(&cwd);
  if (internal::ExecProcess(command, cwd, &python_path, &std_err, &exit_code) &&
      exit_code == 0 && std_err.empty()) {
    base::TrimWhitespaceASCII(python_path, base::TRIM_ALL, &python_path);

    // Python uses the system multibyte code page for sys.executable.
    base::FilePath exe_path(SysMultiByteTo16(python_path));

    // Check for reasonable output, cmd may have output an error message.
    if (base::PathExists(exe_path))
      return exe_path;
  }
  return base::FilePath();
}

base::FilePath FindWindowsPythonRunner(std::u16string_view exe_name,
                                       std::u16string_view bat_name) {
  char16_t current_directory[MAX_PATH];
  ::GetCurrentDirectory(MAX_PATH, reinterpret_cast<LPWSTR>(current_directory));

  // First search for python.exe in the current directory.
  base::FilePath cur_dir_candidate_exe =
      base::FilePath(current_directory).Append(exe_name);
  if (base::PathExists(cur_dir_candidate_exe))
    return cur_dir_candidate_exe;

  // Get the path.
  const char16_t kPathEnvVarName[] = u"Path";
  DWORD path_length = ::GetEnvironmentVariable(
      reinterpret_cast<LPCWSTR>(kPathEnvVarName), nullptr, 0);
  if (path_length == 0)
    return base::FilePath();
  std::unique_ptr<char16_t[]> full_path(new char16_t[path_length]);
  DWORD actual_path_length = ::GetEnvironmentVariable(
      reinterpret_cast<LPCWSTR>(kPathEnvVarName),
      reinterpret_cast<LPWSTR>(full_path.get()), path_length);
  CHECK_EQ(path_length, actual_path_length + 1);

  // Search for python.exe in the path.
  for (const auto& component : base::SplitStringPiece(
           std::u16string_view(full_path.get(), path_length), u";",
           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath candidate_exe = base::FilePath(component).Append(exe_name);
    if (base::PathExists(candidate_exe))
      return candidate_exe;

    // Also allow python.bat, but convert into the .exe.
    base::FilePath candidate_bat = base::FilePath(component).Append(bat_name);
    if (base::PathExists(candidate_bat)) {
      base::FilePath python_exe = PythonBatToExe(candidate_bat);
      if (!python_exe.empty())
        return python_exe;
    }
  }
  return base::FilePath();
}

base::FilePath FindWindowsRunner(std::u16string_view exe_name) {
  char16_t current_directory[MAX_PATH];
  ::GetCurrentDirectory(MAX_PATH, reinterpret_cast<LPWSTR>(current_directory));

  // First search for exe_name in the current directory.
  base::FilePath cur_dir_candidate_exe =
      base::FilePath(current_directory).Append(exe_name);
  if (base::PathExists(cur_dir_candidate_exe))
    return cur_dir_candidate_exe;

  // Get the path.
  const char16_t kPathEnvVarName[] = u"Path";
  DWORD path_length = ::GetEnvironmentVariable(
      reinterpret_cast<LPCWSTR>(kPathEnvVarName), nullptr, 0);
  if (path_length == 0)
    return base::FilePath();
  std::unique_ptr<char16_t[]> full_path(new char16_t[path_length]);
  DWORD actual_path_length = ::GetEnvironmentVariable(
      reinterpret_cast<LPCWSTR>(kPathEnvVarName),
      reinterpret_cast<LPWSTR>(full_path.get()), path_length);
  CHECK_EQ(path_length, actual_path_length + 1);

  // Search for exe_name in the path.
  for (const auto& component : base::SplitStringPiece(
           std::u16string_view(full_path.get(), path_length), u";",
           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath candidate_exe = base::FilePath(component).Append(exe_name);
    if (base::PathExists(candidate_exe))
      return candidate_exe;
  }
  return base::FilePath();
}

base::FilePath ResolveWindowsBareRunnerPath(const std::string& value) {
  base::u16string value_u16 = base::UTF8ToUTF16(value);
  base::u16string exe_name = value_u16 + u".exe";
  base::FilePath exe_path

      if (value == "python" || value == "python3") {
    // For Python 2 or 3 on Windows, in addition to looking for python.exe or
    // python3.exe, we also look for python2.bat or python3.bat
    base::u16string bat_name = value_u16 + u".bat";
    exe_path = FindWindowsPythonRunner(exe_name, bat_name);
  }
  else {
    exe_path = FindWindowsRunner(exe_name);
  }

  if (exe_path.empty()) {
    g_scheduler_.Log("WARNING", "Could not find " + value +
                                    " on path, using "
                                    "just \"" +
                                    value + ".exe\"");
    exe_path = base::FilePath(exe_name);
  }
}
#endif

base::FilePath ResolveBareRunnerPath(const std::string& value) {
  return base::FilePath(value);
}

}  // namespace

ScriptRunners::ScriptRunners() : explicitly_defined_(false) {
#if defined(OS_WIN)
  // Under Windows we perform a path search up front to get an absolute path.
  resolve_runner_path_callback_ = &ResolveWindowsBareRunnerPath;
#else
  // Elsewhere we leave the path search until later.
  resolve_runner_path_callback_ = &ResolveBareRunnerPath;
#endif
}

ScriptRunners::ScriptRunners(const ScriptRunners& other) = default;

ScriptRunners::~ScriptRunners() = default;

bool ScriptRunners::DefineScriptRunnersFromScope(
    const Scope::KeyValueMap& runners,
    const Scope& scope,
    Err* err) {
  path_map_.clear();

  for (const auto& [name, value] : runners) {
    if (!value.VerifyTypeIs(Value::STRING, err))
      return false;

    base::FilePath runner_path = ResolveRunnerPath(value, scope, err);
    if (err->has_error())
      return false;

    AddScriptRunner(name, runner_path);
  }

  return true;
}

base::FilePath ScriptRunners::ResolveRunnerPath(const Value& value,
                                                const Scope& scope,
                                                Err* err) {
  // Check to see if the value was specified without any kind of path
  if (FindDir(&value.string_value()).empty()) {
    DCHECK(resolve_runner_path_callback_);
    return resolve_runner_path_callback_(value.string_value());
  }

  // Otherwise resolve it to an absolute path using the scope directory and the
  // root source path.
  return base::FilePath(scope.GetSourceDir().ResolveRelativeAs(
      true, value, err, scope.settings()->build_settings()->root_path_utf8()));
}

void ScriptRunners::AddScriptRunner(const std::string_view name,
                                    const base::FilePath& runner_path) {
  path_map_.emplace(name, runner_path);
}

base::FilePath ScriptRunners::GetPathForRunner(const Value& name,
                                               Err* err) const {
  if (!name.VerifyTypeIs(Value::STRING, err))
    return base::FilePath();

  auto it = path_map_.find(name.string_value());
  if (it == path_map_.end()) {
    *err = Err(name.origin(), "Runner name not recognized.",
               "The script runner name \"" + name.string_value() +
                   "\" was not "
                   "registered\nas a script runner in the build config with "
                   "\"script_runners()\".");
    return base::FilePath();
  }

  return it->second;
}
