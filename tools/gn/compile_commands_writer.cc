// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/compile_commands_writer.h"

#include <sstream>
//
#include "tools/gn/builder.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/escape.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/path_output.h"
#include "tools/gn/substitution_writer.h"

// Structure of JSON output file
// [
//   {
//      "directory": "The build directory."
//      "file": "The main source file processed by this compilation step.
//               Must be absolute or relative to the above build directory."
//      "command": "The compile command executed."
//   }
//   ...
// ]

namespace {

// Writers and precompiled header functions are taken from the
// NinjaBinaryTargetWriter implementation.

struct DefineWriter {
  DefineWriter() { options.mode = ESCAPE_NINJA_COMMAND; }

  void operator()(const std::string& s, std::ostream& out) const {
    out << " -D";
    EscapeStringToStream(out, s, options);
  }

  EscapeOptions options;
};

struct IncludeWriter {
  explicit IncludeWriter(PathOutput& path_output) : path_output_(path_output) {}
  ~IncludeWriter() = default;

  void operator()(const SourceDir& d, std::ostream& out) const {
    std::ostringstream path_out;
    path_output_.WriteDir(path_out, d, PathOutput::DIR_NO_LAST_SLASH);
    const std::string& path = path_out.str();
    if (path[0] == '"')
      out << " \"-I" << path.substr(1);
    else
      out << " -I" << path;
  }

  PathOutput& path_output_;
};

// Returns the language-specific suffix for precompiled header files.
const char* GetPCHLangSuffixForToolType(Toolchain::ToolType type) {
  switch (type) {
    case Toolchain::TYPE_CC:
      return "c";
    case Toolchain::TYPE_CXX:
      return "cc";
    case Toolchain::TYPE_OBJC:
      return "m";
    case Toolchain::TYPE_OBJCXX:
      return "mm";
    default:
      NOTREACHED() << "Not a valid PCH tool type: " << type;
      return "";
  }
}

std::string GetWindowsPCHObjectExtension(Toolchain::ToolType tool_type,
                                         const std::string& obj_extension) {
  const char* lang_suffix = GetPCHLangSuffixForToolType(tool_type);
  std::string result = ".";
  // For MSVC, annotate the obj files with the language type. For example:
  //   obj/foo/target_name.precompile.obj ->
  //   obj/foo/target_name.precompile.cc.obj
  result += lang_suffix;
  result += obj_extension;
  return result;
}

std::string GetGCCPCHOutputExtension(Toolchain::ToolType tool_type) {
  const char* lang_suffix = GetPCHLangSuffixForToolType(tool_type);
  std::string result = ".";
  // For GCC, the output name must have a .gch suffix and be annotated with
  // the language type. For example:
  //   obj/foo/target_name.header.h ->
  //   obj/foo/target_name.header.h-cc.gch
  // In order for the compiler to pick it up, the output name (minus the .gch
  // suffix MUST match whatever is passed to the -include flag).
  result += "h-";
  result += lang_suffix;
  result += ".gch";
  return result;
}

// Fills |outputs| with the object or gch file for the precompiled header of the
// given type (flag type and tool type must match).
void GetPCHOutputFiles(const Target* target,
                       Toolchain::ToolType tool_type,
                       std::vector<OutputFile>* outputs) {
  outputs->clear();

  // Compute the tool. This must use the tool type passed in rather than the
  // detected file type of the precompiled source file since the same
  // precompiled source file will be used for separate C/C++ compiles.
  const Tool* tool = target->toolchain()->GetTool(tool_type);
  if (!tool)
    return;
  SubstitutionWriter::ApplyListToCompilerAsOutputFile(
      target, target->config_values().precompiled_source(), tool->outputs(),
      outputs);

  if (outputs->empty())
    return;
  if (outputs->size() > 1)
    outputs->resize(1);  // Only link the first output from the compiler tool.

  std::string& output_value = (*outputs)[0].value();
  size_t extension_offset = FindExtensionOffset(output_value);
  if (extension_offset == std::string::npos) {
    // No extension found.
    return;
  }
  DCHECK(extension_offset >= 1);
  DCHECK(output_value[extension_offset - 1] == '.');

  std::string output_extension;
  Tool::PrecompiledHeaderType header_type = tool->precompiled_header_type();
  switch (header_type) {
    case Tool::PCH_MSVC:
      output_extension = GetWindowsPCHObjectExtension(
          tool_type, output_value.substr(extension_offset - 1));
      break;
    case Tool::PCH_GCC:
      output_extension = GetGCCPCHOutputExtension(tool_type);
      break;
    case Tool::PCH_NONE:
      NOTREACHED() << "No outputs for no PCH type.";
      break;
  }
  output_value.replace(extension_offset - 1, std::string::npos,
                       output_extension);
}

OutputFile GetWindowsPCHFile(const Target* target,
                             Toolchain::ToolType tool_type) {
  // Use "obj/{dir}/{target_name}_{lang}.pch" which ends up
  // looking like "obj/chrome/browser/browser_cc.pch"
  OutputFile ret = GetBuildDirForTargetAsOutputFile(target, BuildDirType::OBJ);
  ret.value().append(target->label().name());
  ret.value().push_back('_');
  ret.value().append(GetPCHLangSuffixForToolType(tool_type));
  ret.value().append(".pch");

  return ret;
}

std::string WriteFlag(const Target* target,
                      SubstitutionType subst_enum,
                      PathOutput& path_output,
                      bool has_precompiled_headers,
                      Toolchain::ToolType tool_type,
                      const std::vector<std::string>& (ConfigValues::*getter)()
                          const,
                      EscapeOptions flag_escape_options) {
  std::stringstream out;
  if (!target->toolchain()->substitution_bits().used[subst_enum])
    return std::string();

  if (has_precompiled_headers) {
    const Tool* tool = target->toolchain()->GetTool(tool_type);
    if (tool && tool->precompiled_header_type() == Tool::PCH_MSVC) {
      // Name the .pch file.
      out << " /Fp";
      path_output.WriteFile(out, GetWindowsPCHFile(target, tool_type));

      // Enables precompiled headers and names the .h file. It's a string
      // rather than a file name (so no need to rebase or use path_output).
      out << " /Yu" << target->config_values().precompiled_header();
      RecursiveTargetConfigStringsToStream(target, getter, flag_escape_options,
                                           out);
    } else if (tool && tool->precompiled_header_type() == Tool::PCH_GCC) {
      // The targets to build the .gch files should omit the -include flag
      // below. To accomplish this, each substitution flag is overwritten in the
      // target rule and these values are repeated. The -include flag is omitted
      // in place of the required -x <header lang> flag for .gch targets.
      RecursiveTargetConfigStringsToStream(target, getter, flag_escape_options,
                                           out);

      // Compute the gch file (it will be language-specific).
      std::vector<OutputFile> outputs;
      GetPCHOutputFiles(target, tool_type, &outputs);
      if (!outputs.empty()) {
        // Trim the .gch suffix for the -include flag.
        // e.g. for gch file foo/bar/target.precompiled.h.gch:
        //          -include foo/bar/target.precompiled.h
        std::string pch_file = outputs[0].value();
        pch_file.erase(pch_file.length() - 4);
        out << " -include " << pch_file;
      }
    } else {
      RecursiveTargetConfigStringsToStream(target, getter, flag_escape_options,
                                           out);
    }
  } else {
    RecursiveTargetConfigStringsToStream(target, getter, flag_escape_options,
                                         out);
  }
  return out.str();
}

void RenderJSON(const BuildSettings* build_settings,
                       const Builder& builder,
                       std::vector<const Target*>& all_targets,
                       std::string *compile_commands) {
  // TODO: figure out an appropriate size to reserve.
  compile_commands->reserve(all_targets.size()*10);
  compile_commands->append("[\n");
  bool first = true;
  auto build_dir = build_settings->GetFullPath(build_settings->build_dir());
  std::vector<OutputFile> tool_outputs;  // Prevent reallocation in loop.

  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA_PREFORMATTED_COMMAND;
  EscapeOptions no_quoting(opts);
  no_quoting.inhibit_quoting = true;

  for (const auto* target : all_targets) {
    if (!target->IsBinary())
      continue;

    // Precompute values that are the same for all sources in a target to avoid
    // computing for every source.

    PathOutput path_output(
        target->settings()->build_settings()->build_dir(),
        target->settings()->build_settings()->root_path_utf8(),
        ESCAPE_NINJA_COMMAND);

    bool has_precompiled_headers =
        target->config_values().has_precompiled_headers();

    std::ostringstream defines_out;
    RecursiveTargetConfigToStream<std::string>(target, &ConfigValues::defines,
                                               DefineWriter(), defines_out);
    std::string defines = defines_out.str();

    std::ostringstream includes_out;
    RecursiveTargetConfigToStream<SourceDir>(
        target, &ConfigValues::include_dirs, IncludeWriter(path_output),
        includes_out);
    std::string includes = includes_out.str();

    std::string cflags =
        WriteFlag(target, SUBSTITUTION_CFLAGS, path_output, false,
                  Toolchain::TYPE_NONE, &ConfigValues::cflags, opts);

    std::string cflags_c = WriteFlag(
        target, SUBSTITUTION_CFLAGS_C, path_output, has_precompiled_headers,
        Toolchain::TYPE_CC, &ConfigValues::cflags_c, opts);

    std::string cflags_cc = WriteFlag(
        target, SUBSTITUTION_CFLAGS_CC, path_output, has_precompiled_headers,
        Toolchain::TYPE_CXX, &ConfigValues::cflags_cc, opts);

    std::string cflags_objc = WriteFlag(
        target, SUBSTITUTION_CFLAGS_OBJC, path_output, has_precompiled_headers,
        Toolchain::TYPE_OBJC, &ConfigValues::cflags_objc, opts);

    std::string cflags_objcc = WriteFlag(
        target, SUBSTITUTION_CFLAGS_OBJCC, path_output, has_precompiled_headers,
        Toolchain::TYPE_OBJCXX, &ConfigValues::cflags_objcc, opts);

    for (const auto& source : target->sources()) {
      // If this source is not a C/C++/ObjC/ObjC++ source (not header) file,
      // continue as it does not belong in the compilation database.
      SourceFileType source_type = GetSourceFileType(source);
      if (source_type != SOURCE_CPP && source_type != SOURCE_C &&
          source_type != SOURCE_M && source_type != SOURCE_MM)
        continue;

      Toolchain::ToolType tool_type = Toolchain::TYPE_NONE;
      if (!target->GetOutputFilesForSource(source, &tool_type, &tool_outputs))
        continue;

      if (!first)
        compile_commands->append(",\n");
      first = false;
      compile_commands->append("  {\n");

      std::ostringstream rel_source_path;
      path_output.WriteFile(rel_source_path, source);
      compile_commands->append("    \"file\": \"");
      compile_commands->append(rel_source_path.str());
      compile_commands->append("\",\n    \"directory\": \"");
      compile_commands->append(build_dir.value());
      compile_commands->append("\",");

      const Tool* tool = target->toolchain()->GetTool(tool_type);

      // Build the compilation command.
      std::ostringstream command_out;

      for (const auto& range : tool->command().ranges()) {
        // TODO: this is emitting a bonus space prior to each substitution.
        switch (range.type) {
          case SUBSTITUTION_LITERAL:
            EscapeStringToStream(command_out, range.literal, no_quoting);
            break;
          case SUBSTITUTION_OUTPUT:
            path_output.WriteFiles(command_out, tool_outputs);
            break;
          case SUBSTITUTION_DEFINES:
            command_out << defines;
            break;
          case SUBSTITUTION_INCLUDE_DIRS:
            command_out << includes;
            break;
          case SUBSTITUTION_CFLAGS:
            command_out << cflags;
            break;
          case SUBSTITUTION_CFLAGS_C:
            if (source_type == SOURCE_C)
              command_out << cflags_c;
            break;
          case SUBSTITUTION_CFLAGS_CC:
            if (source_type == SOURCE_CPP)
              command_out << cflags_cc;
            break;
          case SUBSTITUTION_CFLAGS_OBJC:
            if (source_type == SOURCE_M)
              command_out << cflags_objc;
            break;
          case SUBSTITUTION_CFLAGS_OBJCC:
            if (source_type == SOURCE_MM)
              command_out << cflags_objcc;
            break;
          case SUBSTITUTION_LABEL:
          case SUBSTITUTION_LABEL_NAME:
          case SUBSTITUTION_ROOT_GEN_DIR:
          case SUBSTITUTION_ROOT_OUT_DIR:
          case SUBSTITUTION_TARGET_GEN_DIR:
          case SUBSTITUTION_TARGET_OUT_DIR:
          case SUBSTITUTION_TARGET_OUTPUT_NAME:
          case SUBSTITUTION_SOURCE:
          case SUBSTITUTION_SOURCE_NAME_PART:
          case SUBSTITUTION_SOURCE_FILE_PART:
          case SUBSTITUTION_SOURCE_DIR:
          case SUBSTITUTION_SOURCE_ROOT_RELATIVE_DIR:
          case SUBSTITUTION_SOURCE_GEN_DIR:
          case SUBSTITUTION_SOURCE_OUT_DIR:
          case SUBSTITUTION_SOURCE_TARGET_RELATIVE:
            EscapeStringToStream(command_out,
                                 SubstitutionWriter::GetCompilerSubstitution(
                                     target, source, range.type),
                                 opts);
            break;

          // Other flags shouldn't be relevant to compiling C/C++/ObjC/ObjC++
          // source files.
          default:
            NOTREACHED()
                << "Unsupported substitution for this type of target : "
                << kSubstitutionNames[range.type];
            continue;
        }
      }
      compile_commands->append("\n    \"command\": \"");
      compile_commands->append(command_out.str());
      compile_commands->append("\"\n  }");
    }
  }

  compile_commands->append("\n]\n");

  // std::string s;
  // base::JSONWriter::WriteWithOptions(
  //     compile_commands, base::JSONWriter::OPTIONS_PRETTY_PRINT, &s);
  // return s;
}

}  // namespace

bool CompileCommandsWriter::RunAndWriteFiles(
    const BuildSettings* build_settings,
    const Builder& builder,
    const std::string& file_name,
    bool quiet,
    Err* err) {
  SourceFile output_file = build_settings->build_dir().ResolveRelativeFile(
      Value(nullptr, file_name), err);
  if (output_file.is_null()) {
    return false;
  }

  base::FilePath output_path = build_settings->GetFullPath(output_file);

  std::vector<const Target*> all_targets = builder.GetAllResolvedTargets();

  std::string json;
  RenderJSON(build_settings, builder, all_targets, &json);
  if (!WriteFileIfChanged(output_path, json, err))
    return false;
  return true;
}
