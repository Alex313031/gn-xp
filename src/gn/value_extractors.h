// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_VALUE_EXTRACTORS_H_
#define TOOLS_GN_VALUE_EXTRACTORS_H_

#include <string>
#include <vector>
#include <type_traits>

#include "gn/label_ptr.h"
#include "gn/lib_file.h"
#include "gn/unique_vector.h"

class BuildSettings;
class Err;
class Label;
class LabelPattern;
class SourceDir;
class SourceFile;
class Value;

// string
bool IsEqual(const std::string& lhs, const std::string& rhs);
// SourceDir or SourceFile
bool IsEqual(const SourceDir& lhs, const SourceDir& rhs);
bool IsEqual(const SourceFile& lhs, const SourceFile& rhs);
// LibFile
bool IsEqual(const LibFile& lhs, const LibFile& rhs);
// LabelPattern
bool IsEqual(const LabelPattern& lhs, const LabelPattern& rhs);

// LabelPtrPair or LabelTargetPair
bool IsEqual(const LabelConfigPair& lhs, const LabelConfigPair& rhs);
bool IsEqual(const LabelTargetPair& lhs, const LabelTargetPair& rhs);

template <typename T>
struct TypeTraits
{ 
  static bool findValueInList(const T& value, const std::vector<T>& list) {
    for (const auto& item : list) {
      if (IsEqual(item, value)) {
        return true;
      }
    }
    return false;
  }
  static bool findValueInList(const T& value, const UniqueVector<T>& list) {
    for (const auto& item : list) {
      if (IsEqual(item, value)) {
        return true;
      }
    }
    return false;
  }
};

template <typename T>
void VectorExclude(std::vector<T>* from_vector,
                  const std::vector<T>& exclude_vector) {
  if (exclude_vector.empty())
    return;
  size_t new_size = from_vector->size() - exclude_vector.size();
  std::vector<T> new_vector(new_size);
  size_t i = 0;
  for (const auto& item : *from_vector) {
    if (!TypeTraits<T>::findValueInList(item, exclude_vector)) {
      if (i < new_size) {
        new_vector[i++] = std::move(item);
      } else {
        new_vector.emplace_back(std::move(item));
      }
    }
  }

  *from_vector = std::move(new_vector);
}

template <typename T>
void VectorExclude(UniqueVector<T>* from_vector,
                  const UniqueVector<T>& exclude_vector) {
  if (exclude_vector.empty())
    return;
  UniqueVector<T> new_vector;
  for (const auto& item : *from_vector) {
    if (!TypeTraits<T>::findValueInList(item, exclude_vector)) {
        new_vector.emplace_back(std::move(item));
    }
  }
  *from_vector = std::move(new_vector);
}

// On failure, returns false and sets the error.
bool ExtractListOfStringValues(const Value& value,
                               std::vector<std::string>* dest,
                               Err* err);

// Looks for a list of source files relative to a given current dir.
bool ExtractListOfRelativeFiles(const BuildSettings* build_settings,
                                const Value& value,
                                const SourceDir& current_dir,
                                std::vector<SourceFile>* files,
                                Err* err);

// Extracts a list of libraries. When they contain a "/" they are treated as
// source paths and are otherwise treated as plain strings.
bool ExtractListOfLibs(const BuildSettings* build_settings,
                       const Value& value,
                       const SourceDir& current_dir,
                       std::vector<LibFile>* libs,
                       Err* err);

// Looks for a list of source directories relative to a given current dir.
bool ExtractListOfRelativeDirs(const BuildSettings* build_settings,
                               const Value& value,
                               const SourceDir& current_dir,
                               std::vector<SourceDir>* dest,
                               Err* err);

// Extracts the list of labels and their origins to the given vector. Only the
// labels are filled in, the ptr for each pair in the vector will be null.
bool ExtractListOfLabels(const BuildSettings* build_settings,
                         const Value& value,
                         const SourceDir& current_dir,
                         const Label& current_toolchain,
                         LabelTargetVector* dest,
                         Err* err);

// Extracts the list of labels and their origins to the given vector. For the
// version taking Label*Pair, only the labels are filled in, the ptr for each
// pair in the vector will be null. Sets an error and returns false if a label
// is maformed or there are duplicates.
bool ExtractListOfUniqueLabels(const BuildSettings* build_settings,
                               const Value& value,
                               const SourceDir& current_dir,
                               const Label& current_toolchain,
                               UniqueVector<Label>* dest,
                               Err* err);
bool ExtractListOfUniqueLabels(const BuildSettings* build_settings,
                               const Value& value,
                               const SourceDir& current_dir,
                               const Label& current_toolchain,
                               UniqueVector<LabelConfigPair>* dest,
                               Err* err);
bool ExtractListOfUniqueLabels(const BuildSettings* build_settings,
                               const Value& value,
                               const SourceDir& current_dir,
                               const Label& current_toolchain,
                               UniqueVector<LabelTargetPair>* dest,
                               Err* err);

bool ExtractRelativeFile(const BuildSettings* build_settings,
                         const Value& value,
                         const SourceDir& current_dir,
                         SourceFile* file,
                         Err* err);

bool ExtractListOfLabelPatterns(const BuildSettings* build_settings,
                                const Value& value,
                                const SourceDir& current_dir,
                                std::vector<LabelPattern>* patterns,
                                Err* err);

bool ExtractListOfExterns(const BuildSettings* build_settings,
                          const Value& value,
                          const SourceDir& current_dir,
                          std::vector<std::pair<std::string, LibFile>>* externs,
                          Err* err);

#endif  // TOOLS_GN_VALUE_EXTRACTORS_H_
