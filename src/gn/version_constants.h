// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_VERSION_CONSTANTS_H_
#define TOOLS_GN_VERSION_CONSTANTS_H_

#if defined(__clang__) && defined(_UNICODE)
 #pragma code_page(65001) // UTF-8
#endif

// Macro to convert to string
#if !defined(_STRINGIZER_)
 #define _STRINGIZER_
 #define _STRINGIZER(in) #in
 #define STRINGIZE(in) _STRINGIZER(in)
  // Wide-string variant: L ## "x" -> L"x". Two levels so the argument expands
 // before the L## paste widens the resulting narrow literal.
 #define _WIDEN(in) L ## in
 #define WIDEN(in) _WIDEN(in)
#endif // !defined(_STRINGIZER_)

// Product/version metadata for this GN fork. This header is intentionally
// nothing but preprocessor macros so it can be #included from both C++ and
// from the Windows resource script (src/gn/gn.rc), which is run through the
// C preprocessor by windres/rc before being compiled into gn.exe.
//
// Edit the values below to taste; gn.rc pulls everything from here so there is
// a single source of truth for the VERSIONINFO block.

// Numeric version, used for the binary FILEVERSION / PRODUCTVERSION fields
// (each field is a 16-bit integer: major, minor, patch, build).
#define GN_VERSION_MAJOR 1
#define GN_VERSION_MINOR 0
// The build field auto-syncs to the commit position printed by `gn --version`.
// gn.rc #includes the generated last_commit_position.h before this header, so
// LAST_COMMIT_POSITION_NUM is defined during the resource compile; standalone
// includes (e.g. from C++) fall back to 0.
#ifdef LAST_COMMIT_POSITION_NUM
#define GN_VERSION_PATCH LAST_COMMIT_POSITION_NUM
#else
#define GN_VERSION_PATCH 0
#endif
#define GN_VERSION_BUILD 0

// Human-readable dotted version, e.g. "1.0.0.2359".
#define GN_VERSION_STRING WIDEN(STRINGIZE(GN_VERSION_MAJOR.GN_VERSION_MINOR.GN_VERSION_PATCH.GN_VERSION_BUILD))

// String fields shown by the Windows shell (right-click -> Properties ->
// Details) via the StringFileInfo block in gn.rc.
#define GN_PRODUCT_NAME L"GN XP"
#define GN_INTERNAL_NAME L"gn-xp"
#define GN_COMPANY_NAME L"Alex313031"
#define GN_FILE_DESCRIPTION L"GN meta-build system - Windows XP/Vista fork."
#define GN_LEGAL_COPYRIGHT L"\251 2013-2026 The Chromium Authors and Alex313031."
#define GN_TRADEMARKS L"BSD-3 License"
#define GN_COMMENTS L"https://github.com/Alex313031/gn-xp"

#endif  // TOOLS_GN_VERSION_CONSTANTS_H_
