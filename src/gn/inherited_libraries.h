// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_INHERITED_LIBRARIES_H_
#define TOOLS_GN_INHERITED_LIBRARIES_H_

#include <stddef.h>

#include <utility>
#include <vector>

#include "gn/immutable_vector.h"
#include "gn/tagged_pointer.h"
#include "gn/unique_vector.h"

class Target;

// A Compact representation of a read-only (target, is_public) pair.
class TargetPublicFlagPair {
 public:
  TargetPublicFlagPair(const Target* target, bool is_public)
      : tagged_(target, is_public ? 1 : 0) {}

  const Target* target() const { return tagged_.ptr(); }
  bool is_public() const { return tagged_.tag() != 0; }
  void set_is_public(bool value) { tagged_.set_tag(value ? 1 : 0); }

 private:
  TaggedPointer<const Target, 1u> tagged_;
};

class ImmutableInheritedLibraries
    : public ImmutableVector<TargetPublicFlagPair> {
 public:
  ImmutableInheritedLibraries() = default;

  // These two methods are only used by unit-tests. TODO: Remove them.
  std::vector<const Target*> GetOrdered() const;
  std::vector<std::pair<const Target*, bool>> GetOrderedAndPublicFlag() const;

  class Builder {
   public:
    Builder& Append(TargetPublicFlagPair pair);

    Builder& Append(const Target* target, bool is_public) {
      return Append(TargetPublicFlagPair(target, is_public));
    }

    Builder& AppendInherited(const ImmutableInheritedLibraries& other,
                             bool is_public);

    Builder& AppendPublicSharedLibraries(
        const ImmutableInheritedLibraries& other,
        bool is_public);

    Builder& Reset() {
      pairs_.clear();
      return *this;
    }

    ImmutableInheritedLibraries Build() const {
      return ImmutableInheritedLibraries(*this);
    }

    size_t size() const { return pairs_.size(); }

   private:
    friend ImmutableInheritedLibraries;

    struct PairHash {
      size_t operator()(TargetPublicFlagPair pair) const {
        return std::hash<const Target*>()(pair.target());
      }
    };
    struct PairEqualTo {
      bool operator()(TargetPublicFlagPair a, TargetPublicFlagPair b) const {
        return a.target() == b.target();
      }
    };
    UniqueVector<TargetPublicFlagPair, PairHash, PairEqualTo> pairs_;
  };

 private:
  // Used by the Builder class only.
  ImmutableInheritedLibraries(const Builder& builder)
      : ImmutableVector<TargetPublicFlagPair>(builder.pairs_) {}
};

// Represents an ordered uniquified set of all shared/static libraries for
// a given target. These are pushed up the dependency tree.
//
// Maintaining the order is important so GN links all libraries in the same
// order specified in the build files.
//
// Since this list is uniquified, appending to the list will not actually
// append a new item if the target already exists. However, the existing one
// may have its is_public flag updated. "Public" always wins, so is_public will
// be true if any dependency with that name has been set to public.
class InheritedLibraries {
 public:
  InheritedLibraries();
  ~InheritedLibraries();

  // Returns the list of dependencies in order, optionally with the flag
  // indicating whether the dependency is public.
  std::vector<const Target*> GetOrdered() const { return targets_.vector(); }
  std::vector<std::pair<const Target*, bool>> GetOrderedAndPublicFlag() const;

  // Adds a single dependency to the end of the list. See note on adding above.
  void Append(const Target* target, bool is_public);

  // Appends all items from the "other" list to the current one. The is_public
  // parameter indicates how the current target depends on the items in
  // "other". If is_public is true, the existing public flags of the appended
  // items will be preserved (propagating the public-ness up the dependency
  // chain). If is_public is false, all deps will be added as private since
  // the current target isn't forwarding them.
  void AppendInherited(const InheritedLibraries& other, bool is_public);

  // Like AppendInherited but only appends the items in "other" that are of
  // type SHARED_LIBRARY and only when they're marked public. This is used
  // to push shared libraries up the dependency chain, following only public
  // deps, to dependent targets that need to use them.
  void AppendPublicSharedLibraries(const InheritedLibraries& other,
                                   bool is_public);

 private:
  UniqueVector<const Target*> targets_;
  std::vector<bool> public_flags_;

  InheritedLibraries(const InheritedLibraries&) = delete;
  InheritedLibraries& operator=(const InheritedLibraries&) = delete;
};

#endif  // TOOLS_GN_INHERITED_LIBRARIES_H_
