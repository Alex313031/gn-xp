// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains defines and typedefs that allow popular Windows types to
// be used without the overhead of including windows.h.

#ifndef BASE_WIN_WINDOWS_TYPES_H
#define BASE_WIN_WINDOWS_TYPES_H

// Needed for function prototypes.
#include <inttypes.h>
#include <concurrencysal.h>
#include <sal.h>
#include <specstrings.h>

#ifdef __cplusplus
extern "C" {
#endif

// typedef and define the most commonly used Windows integer types.

typedef unsigned long DWORD;
typedef long LONG;
typedef __int64 LONGLONG;
typedef unsigned __int64 ULONGLONG;

#define VOID void
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
typedef int INT;
typedef unsigned int UINT;
typedef unsigned int* PUINT;
typedef void* LPVOID;
typedef void* PVOID;
typedef void* HANDLE;
typedef int BOOL;
typedef unsigned char BYTE;
typedef BYTE BOOLEAN;
typedef DWORD ULONG;
typedef unsigned short WORD;
typedef WORD UWORD;
typedef WORD ATOM;

#if defined(_WIN64)
typedef __int64 INT_PTR, *PINT_PTR;
typedef unsigned __int64 UINT_PTR, *PUINT_PTR;

typedef __int64 LONG_PTR, *PLONG_PTR;
typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;
#else
typedef __w64 int INT_PTR, *PINT_PTR;
typedef __w64 unsigned int UINT_PTR, *PUINT_PTR;

typedef __w64 long LONG_PTR, *PLONG_PTR;
typedef __w64 unsigned long ULONG_PTR, *PULONG_PTR;
#endif

typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
#define LRESULT LONG_PTR
typedef _Return_type_success_(return >= 0) long HRESULT;

typedef ULONG_PTR SIZE_T, *PSIZE_T;
typedef LONG_PTR SSIZE_T, *PSSIZE_T;

typedef DWORD ACCESS_MASK;
typedef ACCESS_MASK REGSAM;

// Forward declare Windows compatible handles.

#define CHROME_DECLARE_HANDLE(name) \
  struct name##__;                  \
  typedef struct name##__* name
CHROME_DECLARE_HANDLE(HGLRC);
CHROME_DECLARE_HANDLE(HICON);
CHROME_DECLARE_HANDLE(HINSTANCE);
CHROME_DECLARE_HANDLE(HKEY);
CHROME_DECLARE_HANDLE(HKL);
CHROME_DECLARE_HANDLE(HMENU);
CHROME_DECLARE_HANDLE(HWND);
typedef HINSTANCE HMODULE;
#undef CHROME_DECLARE_HANDLE

// Forward declare some Windows struct/typedef sets.

typedef struct _OVERLAPPED OVERLAPPED;
typedef struct tagMSG MSG, *PMSG, *NPMSG, *LPMSG;

typedef struct _RTL_SRWLOCK RTL_SRWLOCK;
typedef RTL_SRWLOCK SRWLOCK, *PSRWLOCK;

typedef struct _GUID GUID;
typedef GUID CLSID;

typedef struct tagLOGFONTW LOGFONTW, *PLOGFONTW, *NPLOGFONTW, *LPLOGFONTW;
typedef LOGFONTW LOGFONT;

typedef struct _FILETIME FILETIME;

typedef struct tagMENUITEMINFOW MENUITEMINFOW, MENUITEMINFO;

typedef struct tagNMHDR NMHDR;

// Declare Chrome versions of some Windows structures. These are needed for
// when we need a concrete type but don't want to pull in Windows.h. We can't
// declare the Windows types so we declare our types and cast to the Windows
// types in a few places.

struct CHROME_SRWLOCK {
  PVOID Ptr;
};

struct CHROME_CONDITION_VARIABLE {
  PVOID Ptr;
};

// Define some commonly used Windows constants. Note that the layout of these
// macros - including internal spacing - must be 100% consistent with windows.h.

// clang-format off

#ifndef INVALID_HANDLE_VALUE
// Work around there being two slightly different definitions in the SDK.
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#endif
#undef TLS_OUT_OF_INDEXES
#define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)
#undef HTNOWHERE
#define HTNOWHERE 0
#undef MAX_PATH
#define MAX_PATH 260
#undef CS_GLOBALCLASS
#define CS_GLOBALCLASS 0x4000

#undef ERROR_SUCCESS
#define ERROR_SUCCESS 0L
#undef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND 2L
#undef ERROR_ACCESS_DENIED
#define ERROR_ACCESS_DENIED 5L
#undef ERROR_INVALID_HANDLE
#define ERROR_INVALID_HANDLE 6L
#undef ERROR_SHARING_VIOLATION
#define ERROR_SHARING_VIOLATION 32L
#undef ERROR_LOCK_VIOLATION
#define ERROR_LOCK_VIOLATION 33L
#undef REG_BINARY
#define REG_BINARY ( 3ul )

#undef STATUS_PENDING
#define STATUS_PENDING ((DWORD   )0x00000103L)
#undef STILL_ACTIVE
#define STILL_ACTIVE STATUS_PENDING
#undef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#undef FAILED
#define FAILED(hr) (((HRESULT)(hr)) < 0)

#undef HKEY_CLASSES_ROOT
#define HKEY_CLASSES_ROOT (( HKEY ) (ULONG_PTR)((LONG)0x80000000) )
#undef HKEY_LOCAL_MACHINE
#define HKEY_LOCAL_MACHINE (( HKEY ) (ULONG_PTR)((LONG)0x80000002) )
#undef HKEY_CURRENT_USER
#define HKEY_CURRENT_USER (( HKEY ) (ULONG_PTR)((LONG)0x80000001) )
#undef KEY_QUERY_VALUE
#define KEY_QUERY_VALUE (0x0001)
#undef KEY_SET_VALUE
#define KEY_SET_VALUE (0x0002)
#undef KEY_CREATE_SUB_KEY
#define KEY_CREATE_SUB_KEY (0x0004)
#undef KEY_ENUMERATE_SUB_KEYS
#define KEY_ENUMERATE_SUB_KEYS (0x0008)
#undef KEY_NOTIFY
#define KEY_NOTIFY (0x0010)
#undef KEY_CREATE_LINK
#define KEY_CREATE_LINK (0x0020)
#undef KEY_WOW64_32KEY
#define KEY_WOW64_32KEY (0x0200)
#undef KEY_WOW64_64KEY
#define KEY_WOW64_64KEY (0x0100)
#undef KEY_WOW64_RES
#define KEY_WOW64_RES (0x0300)

#undef READ_CONTROL
#define READ_CONTROL (0x00020000L)
#undef SYNCHRONIZE
#define SYNCHRONIZE (0x00100000L)

#undef STANDARD_RIGHTS_READ
#define STANDARD_RIGHTS_READ (READ_CONTROL)
#undef STANDARD_RIGHTS_WRITE
#define STANDARD_RIGHTS_WRITE (READ_CONTROL)
#undef STANDARD_RIGHTS_ALL
#define STANDARD_RIGHTS_ALL (0x001F0000L)

#define KEY_READ                ((STANDARD_RIGHTS_READ       |\
                                  KEY_QUERY_VALUE            |\
                                  KEY_ENUMERATE_SUB_KEYS     |\
                                  KEY_NOTIFY)                 \
                                  &                           \
                                 (~SYNCHRONIZE))


#define KEY_WRITE               ((STANDARD_RIGHTS_WRITE      |\
                                  KEY_SET_VALUE              |\
                                  KEY_CREATE_SUB_KEY)         \
                                  &                           \
                                 (~SYNCHRONIZE))

#define KEY_ALL_ACCESS          ((STANDARD_RIGHTS_ALL        |\
                                  KEY_QUERY_VALUE            |\
                                  KEY_SET_VALUE              |\
                                  KEY_CREATE_SUB_KEY         |\
                                  KEY_ENUMERATE_SUB_KEYS     |\
                                  KEY_NOTIFY                 |\
                                  KEY_CREATE_LINK)            \
                                  &                           \
                                 (~SYNCHRONIZE))

// Define some macros needed when prototyping Windows functions.

#undef DECLSPEC_IMPORT
#define DECLSPEC_IMPORT __declspec(dllimport)
#undef WINBASEAPI
#define WINBASEAPI DECLSPEC_IMPORT
#undef WINUSERAPI
#define WINUSERAPI DECLSPEC_IMPORT
#undef WINAPI
#define WINAPI __stdcall
#undef CALLBACK
#define CALLBACK __stdcall

// clang-format on

// Needed for optimal lock performance.
WINBASEAPI _Releases_exclusive_lock_(*SRWLock) VOID WINAPI
    ReleaseSRWLockExclusive(_Inout_ PSRWLOCK SRWLock);

// Needed to support protobuf's GetMessage macro magic.
WINUSERAPI BOOL WINAPI GetMessageW(_Out_ LPMSG lpMsg,
                                   _In_opt_ HWND hWnd,
                                   _In_ UINT wMsgFilterMin,
                                   _In_ UINT wMsgFilterMax);

// Needed for thread_local_storage.h
WINBASEAPI LPVOID WINAPI TlsGetValue(_In_ DWORD dwTlsIndex);

// Needed for scoped_handle.h
WINBASEAPI _Check_return_ _Post_equals_last_error_ DWORD WINAPI
    GetLastError(VOID);

WINBASEAPI VOID WINAPI SetLastError(_In_ DWORD dwErrCode);

#ifdef __cplusplus
}
#endif

// These macros are all defined by windows.h and are also used as the names of
// functions in the Chromium code base. Add to this list as needed whenever
// there is a Windows macro which causes a function call to be renamed. This
// ensures that the same renaming will happen everywhere. Includes of this file
// can be added wherever needed to ensure this consistent renaming.

#undef CopyFile
#define CopyFile CopyFileW
#undef CreateDirectory
#define CreateDirectory CreateDirectoryW
#undef CreateEvent
#define CreateEvent CreateEventW
#undef CreateFile
#define CreateFile CreateFileW
#undef CreateService
#define CreateService CreateServiceW
#undef DeleteFile
#define DeleteFile DeleteFileW
#undef DispatchMessage
#define DispatchMessage DispatchMessageW
#undef DrawText
#define DrawText DrawTextW
#undef GetComputerName
#define GetComputerName GetComputerNameW
#undef GetCurrentDirectory
#define GetCurrentDirectory GetCurrentDirectoryW
#undef GetCurrentTime
#define GetCurrentTime() GetTickCount()
#undef GetFileAttributes
#define GetFileAttributes GetFileAttributesW
#undef GetMessage
#define GetMessage GetMessageW
#undef GetUserName
#define GetUserName GetUserNameW
#undef LoadIcon
#define LoadIcon LoadIconW
#undef LoadImage
#define LoadImage LoadImageW
#undef PostMessage
#define PostMessage PostMessageW
#undef ReplaceFile
#define ReplaceFile ReplaceFileW
#undef ReportEvent
#define ReportEvent ReportEventW
#undef SendMessage
#define SendMessage SendMessageW
#undef SendMessageCallback
#define SendMessageCallback SendMessageCallbackW
#undef SetCurrentDirectory
#define SetCurrentDirectory SetCurrentDirectoryW
#undef StartService
#define StartService StartServiceW
#undef StrCat
#define StrCat StrCatW

#endif  // BASE_WIN_WINDOWS_TYPES_H
