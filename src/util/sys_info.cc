// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/sys_info.h"

#include "base/logging.h"
#include "util/build_config.h"

#if defined(OS_POSIX)
#include <sys/utsname.h>
#include <unistd.h>
#endif

#if defined(OS_WIN)
#include <windows.h>
#include "base/win/registry.h"
#endif

bool IsLongPathsSupportEnabled() {
#if defined(OS_WIN)
  struct LongPathSupport {
    LongPathSupport() {
      // Probe ntdll.dll for RtlAreLongPathsEnabled, and call it if it exists.
      HINSTANCE ntdll_lib = GetModuleHandleW(L"ntdll");
      if (ntdll_lib) {
        using FunctionType = BOOLEAN(WINAPI*)();
        auto func_ptr = reinterpret_cast<FunctionType>(
            GetProcAddress(ntdll_lib, "RtlAreLongPathsEnabled"));
        if (func_ptr) {
          supported = func_ptr();
          return;
        }
      }

      // If the ntdll approach failed, the registry approach is still reliable,
      // because the manifest should've always be linked with gn.exe in Windows.
      const char16_t key_name[] =
          uR"(SYSTEM\CurrentControlSet\Control\FileSystem)";
      const char16_t value_name[] = u"LongPathsEnabled";

      base::win::RegKey key(HKEY_LOCAL_MACHINE, key_name, KEY_READ);
      DWORD value;
      if (key.ReadValueDW(value_name, &value) == ERROR_SUCCESS) {
        supported = value == 1;
      }
    }

    bool supported = false;
  };

  static LongPathSupport s_long_paths;  // constructed lazily
  return s_long_paths.supported;
#else
  return true;
#endif
}

std::string OperatingSystemArchitecture() {
#if defined(OS_POSIX)
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return std::string();
  }
  std::string arch(info.machine);
  std::string os(info.sysname);
  if (arch == "i386" || arch == "i486" || arch == "i586" || arch == "i686") {
    arch = "x86";
  } else if (arch == "i86pc") {
    // Solaris and illumos systems report 'i86pc' (an Intel x86 PC) as their
    // machine for both 32-bit and 64-bit x86 systems.  Considering the rarity
    // of 32-bit systems at this point, it is safe to assume 64-bit.
    arch = "x86_64";
  } else if (arch == "amd64") {
    arch = "x86_64";
  } else if (os == "AIX" || os == "OS400") {
    arch = "ppc64";
  } else if (std::string(info.sysname) == "OS/390") {
    arch = "s390x";
  }
  return arch;
#elif defined(OS_WIN)
  SYSTEM_INFO system_info = {};
  ::GetNativeSystemInfo(&system_info);
  switch (system_info.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_INTEL:
      return "x86";
    case PROCESSOR_ARCHITECTURE_AMD64:
      return "x86_64";
    case PROCESSOR_ARCHITECTURE_IA64:
      return "ia64";
  }
  return std::string();
#else
#error
#endif
}

int NumberOfProcessors() {
#if defined(OS_ZOS)
  return __get_num_online_cpus();

#elif defined(OS_POSIX)
  // sysconf returns the number of "logical" (not "physical") processors on both
  // Mac and Linux.  So we get the number of max available "logical" processors.
  //
  // Note that the number of "currently online" processors may be fewer than the
  // returned value of NumberOfProcessors(). On some platforms, the kernel may
  // make some processors offline intermittently, to save power when system
  // loading is low.
  //
  // One common use case that needs to know the processor count is to create
  // optimal number of threads for optimization. It should make plan according
  // to the number of "max available" processors instead of "currently online"
  // ones. The kernel should be smart enough to make all processors online when
  // it has sufficient number of threads waiting to run.
  long res = sysconf(_SC_NPROCESSORS_CONF);
  if (res == -1) {
    NOTREACHED();
    return 1;
  }

  return static_cast<int>(res);
#elif defined(OS_WIN)
  // On machines with more than 64 logical processors this will not return the
  // full number of processors. Instead it will return the number of processors
  // in one processor group - something smaller than 65. However this is okay
  // because gn doesn't parallelize well beyond 10-20 threads - it starts to run
  // slower.
  SYSTEM_INFO system_info = {};
  ::GetNativeSystemInfo(&system_info);
  if (system_info.dwNumberOfProcessors < 1) {
    NOTREACHED();
    return 1;
  }
  return static_cast<int>(system_info.dwNumberOfProcessors);
#else
#error
#endif
}

#if defined(OS_WIN)
namespace {

// Reads the true NT version straight from ntdll, bypassing the app-compat
// version that GetVersionExW reports to legacy-targeted binaries. Any out-param
// may be null. Returns false if the version could not be determined.
bool GetRawNtVersion(UINT* major, UINT* minor, UINT* build) {
  HMODULE ntdll_lib = ::GetModuleHandleW(L"ntdll");
  if (ntdll_lib == nullptr) {
    return false;
  }

  // Primary: RtlGetNtVersionNumbers (XP+, undocumented). Packs a build-type
  // flag into the top 4 bits of the build number - mask them off so callers
  // see the plain build (e.g. 2600 for XP SP3, 19045 for Win10 22H2).
  using RtlGetNtVersionNumbersFunc = void(WINAPI*)(DWORD*, DWORD*, DWORD*);
  const auto rtl_get_nt_version_numbers =
      reinterpret_cast<RtlGetNtVersionNumbersFunc>(
          ::GetProcAddress(ntdll_lib, "RtlGetNtVersionNumbers"));
  if (rtl_get_nt_version_numbers != nullptr) {
    DWORD major_ver = 0;
    DWORD minor_ver = 0;
    DWORD build_ver = 0;
    rtl_get_nt_version_numbers(&major_ver, &minor_ver, &build_ver);
    if (major_ver != 0) {
      if (major != nullptr) {
        *major = static_cast<UINT>(major_ver);
      }
      if (minor != nullptr) {
        *minor = static_cast<UINT>(minor_ver);
      }
      if (build != nullptr) {
        *build = static_cast<UINT>(build_ver & 0x0FFFFFFFu);
      }
      return true;
    }
  }

  // Fallback: RtlGetVersion (documented, NT-native, not shimmed by
  // application-compatibility manifests unlike GetVersionExW on Win8.1+).
  using RtlGetVersionFunc = LONG(WINAPI*)(OSVERSIONINFOW*);
  const auto rtl_get_version = reinterpret_cast<RtlGetVersionFunc>(
      ::GetProcAddress(ntdll_lib, "RtlGetVersion"));
  if (rtl_get_version != nullptr) {
    OSVERSIONINFOW version_info = {};
    version_info.dwOSVersionInfoSize = sizeof(version_info);
    if (rtl_get_version(&version_info) == 0 &&
        version_info.dwMajorVersion != 0) {
      if (major != nullptr) {
        *major = static_cast<UINT>(version_info.dwMajorVersion);
      }
      if (minor != nullptr) {
        *minor = static_cast<UINT>(version_info.dwMinorVersion);
      }
      if (build != nullptr) {
        *build = static_cast<UINT>(version_info.dwBuildNumber);
      }
      return true;
    }
  }

  return false;
}

// Marketing name for an NT major.minor(.build), or an empty string if the
// version is not recognized. The build number distinguishes Windows 10 from 11
// (both report NT 10.0; 11 starts at build 22000).
std::string WindowsVersionName(UINT major, UINT minor, UINT build) {
  switch (major) {
    case 5:
      switch (minor) {
        case 0:
          return "2000";
        case 1:
          return "XP";
        case 2:
          return "XP x64 / Server 2003";
      }
      break;
    case 6:
      switch (minor) {
        case 0:
          return "Vista";
        case 1:
          return "7";
        case 2:
          return "8";
        case 3:
          return "8.1";
      }
      break;
    case 10:
      if (minor == 0) {
        return build >= 22000u ? "11" : "10";
      }
      break;
  }
  return std::string();
}

}  // namespace
#endif  // defined(OS_WIN)

bool IsLegacyWindows() {
#if defined(OS_WIN)
  UINT major = 0;
  UINT minor = 0;
  if (GetRawNtVersion(&major, &minor, nullptr)) {
    // Anything older than Windows 7 (6.1) is considered legacy here.
    return major < 6u || (major == 6u && minor == 0u);
  }
  return false;
#else
  return false;
#endif
}

std::string OperatingSystemVersion() {
#if defined(OS_WIN)
  UINT major = 0;
  UINT minor = 0;
  UINT build = 0;
  if (!GetRawNtVersion(&major, &minor, &build)) {
    return std::string();
  }
  std::string version = "Windows ";
  std::string name = WindowsVersionName(major, minor, build);
  if (!name.empty()) {
    version += name + " ";
  }
  version += "(" + std::to_string(major) + "." + std::to_string(minor) + "." +
             std::to_string(build) + ")";
  return version;
#else
  return std::string();
#endif
}
