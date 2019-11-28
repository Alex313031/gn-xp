// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_UNIQUE_KEY_H_
#define TOOLS_GN_UNIQUE_KEY_H_

#include <functional>
#include <string>

// A UniqueKey models a pointer to a globally unique constant string.
//
// They are useful as key types for sets and map container types, especially
// when a program uses multiple instances that tend to use the same strings
// (as happen very frequently in GN).
//
// Note that default equality and comparison functions will compare the
// string content, not the pointers, ensuring that the behaviour of
// standard containers using UniqueKey key types is the same as if
// std::string was used.
//
// In addition, _ordered_ containers support heterogneous lookups (i.e.
// using an std::string_view, and by automatic conversion, a const char*
// of const char[] literal) as a key type.
//
// Additionally, it is also possible to implement very fast _unordered_
// containers by using the UniqueKey::Fast{Hash,Equal,Compare} structs,
// which will force containers to hash/compare pointer values instead,
// for example:
//
//     // A fast unordered set of unique strings.
//     //
//     // Implementation uses a hash table so performance will be bounded
//     // by the string hash function. Does not support heterogneous lookups.
//     //
//     using FastStringSet = std::unordered_set<UniqueKey,
//                                              UniqueKey::PtrHash,
//                                              UniqueKey::PtrEqual>;
//
//     // A fast unordered set of unique strings.
//     //
//     // Implementation uses a balanced binary tree so performance will
//     // be bounded by string comparisons. Does support heterogneous lookups,
//     // but not that this does not extend to the [] operator, only to the
//     // find() method.
//     //
//     using FastStringSet = std::set<UniqueKey, UniqueKey::PtrCompare>
//
//     // A fast unordered { string -> VALUE } map.
//     //
//     // Implementation uses a balanced binary tree. Supports heterogneous
//     // lookups.
//     template <typename VALUE>
//     using FastStringMap = std::map<UniqueKey, VALUE, UniqueKey::PtrCompare>
//
class UniqueKey {
 public:
  // Default constructor. Value points to a globally unique empty string.
  UniqueKey();

  // Destructor should do nothing at all.
  ~UniqueKey() = default;

  // Non-explicit constructors.
  UniqueKey(std::string_view str) noexcept;

  // Copy and move operations.
  UniqueKey(const UniqueKey& other) noexcept : value_(other.value_) {}
  UniqueKey& operator=(const UniqueKey& other) noexcept {
    if (this != &other) {
      this->~UniqueKey();  // really a no-op
      new (this) UniqueKey(other);
    }
    return *this;
  }

  UniqueKey(UniqueKey&& other) noexcept : value_(other.value_) {}
  UniqueKey& operator=(const UniqueKey&& other) noexcept {
    if (this != &other) {
      this->~UniqueKey();  // really a no-op
      new (this) UniqueKey(std::move(other));
    }
    return *this;
  }

  bool empty() const { return value_.empty(); }

  // Explicit conversions.
  const std::string& str() const { return value_; }

  // Implicit conversions.
  operator std::string_view() const { return {value_}; }

  // Returns true iff this is the same key.
  // Note that the default comparison functions compare the value instead
  // in order to use them in standard containers without surprises by
  // default.
  bool SameAs(const UniqueKey& other) const { return &value_ == &other.value_; }

  // Default comparison functions.
  bool operator==(const UniqueKey& other) const {
    return value_ == other.value_;
  }

  bool operator!=(const UniqueKey& other) const {
    return value_ != other.value_;
  }

  bool operator<(const UniqueKey& other) const { return value_ < other.value_; }

  size_t hash() const { return std::hash<std::string>()(value_); }

  // Use the following structs to implement containers that use UniqueKey
  // values as keys, but only compare/hash the pointer values for speed.
  // E.g.:
  //    using FastSet = std::unordered_set<UniqueKey, PtrHash, PtrEqual>;
  //    using FastMap = std::map<UniqueKey, Value, PtrCompare>;
  //
  // IMPORTANT: Note that FastMap above is ordered based in the UniqueKey
  //            pointer value, not the string content.
  //
  struct PtrHash {
    size_t operator()(const UniqueKey& key) const noexcept {
      return std::hash<const std::string*>()(&key.value_);
    }
  };

  struct PtrEqual {
    bool operator()(const UniqueKey& a, const UniqueKey& b) const noexcept {
      return &a.value_ == &b.value_;
    }
  };

  struct PtrCompare {
    bool operator()(const UniqueKey& a, const UniqueKey& b) const noexcept {
      return &a.value_ < &b.value_;
    }
  };

 protected:
  const std::string& value_;
};

namespace std {

// Ensure default heterogneous lookups with other types like std::string_view.
template <>
struct less<UniqueKey> {
  using is_transparent = int;

  bool operator()(const UniqueKey& a, const UniqueKey& b) const noexcept {
    return a.str() < b.str();
  }
  template <typename U>
  bool operator()(const UniqueKey& a, const U& b) const noexcept {
    return a.str() < b;
  }
  template <typename U>
  bool operator()(const U& a, const UniqueKey& b) const noexcept {
    return a < b.str();
  };
};

template <>
struct hash<UniqueKey> {
  size_t operator()(const UniqueKey& key) const noexcept { return key.hash(); }
};

}  // namespace std

#endif  // TOOLS_GN_UNIQUE_KEY_H_
