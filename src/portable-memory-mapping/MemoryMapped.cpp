// //////////////////////////////////////////////////////////
// MemoryMapped.cpp
// Copyright (c) 2013 Stephan Brumme. All rights reserved.
// see http://create.stephan-brumme.com/disclaimer.html
//
// Changes by Ivan Kuznetsov for "vsign" project:
//  - avoid paddings in memory layout
//  - add write support
//  - remove unused code
//  - error reporting
//  - clang-format

#include "MemoryMapped.h"

#include <iostream>

// OS-specific
#ifdef _MSC_VER
// Windows
#include <windows.h>
#else
// Linux
// enable large file support on 32 bit systems
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#endif
#define _FILE_OFFSET_BITS 64
// and include needed headers
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/// do nothing, must use open()
MemoryMapped::MemoryMapped()
    : _filesize(0), _file(0),
#ifdef _MSC_VER
      _mappedFile(NULL),
#endif
      _mappedView(NULL) {
}

/// close file (see close() )
MemoryMapped::~MemoryMapped() { close(); }

/// open file for reading
bool MemoryMapped::open_read(const char *filename) {
  // already open ?
  if (isValid())
    return false;

  _file = 0;
  _filesize = 0;
#ifdef _MSC_VER
  _mappedFile = NULL;
#endif
  _mappedView = NULL;

#ifdef _MSC_VER
  // Windows

  // open file
  // FILE_FLAG_SEQUENTIAL_SCAN or FILE_FLAG_RANDOM_ACCESS or
  // FILE_ATTRIBUTE_NORMAL
  _file = ::CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  if (!_file) {
    std::cerr << "CreateFileA Win32 error code: " << GetLastError() << "\n";
    return false;
  }

  // file size
  LARGE_INTEGER result;
  if (!GetFileSizeEx(_file, &result)) {
    std::cerr << "GetFileSizeEx Win32 error code: " << GetLastError() << "\n";
    return false;
  }
  _filesize = static_cast<uint64_t>(result.QuadPart);

  // convert to mapped mode
  _mappedFile = ::CreateFileMapping(_file, NULL, PAGE_READONLY, 0, 0, NULL);
  if (!_mappedFile) {
    std::cerr << "CreateFileMapping Win32 error code: " << GetLastError()
              << "\n";
    return false;
  }

  // get memory address
  _mappedView = ::MapViewOfFile(_mappedFile, FILE_MAP_READ, 0, 0, _filesize);
  if (_mappedView == NULL) {
    std::cerr << "MapViewOfFile Win32 error code: " << GetLastError() << "\n";
    return false;
  }

#else
  // Linux

  // open file
  _file = ::open(filename, O_RDONLY | O_LARGEFILE);
  if (_file == -1) {
    _file = 0;
    return false;
  }

  // file size
  struct stat64 statInfo;
  if (fstat64(_file, &statInfo) < 0) {
    return false;
  }

  _filesize = statInfo.st_size;

  _mappedView = ::mmap64(NULL, _filesize, PROT_READ, MAP_SHARED, _file, 0);
  if (_mappedView == MAP_FAILED) {
    _mappedView = NULL;
    return false;
  }

  // tweak performance
  int linuxHint =
      MADV_SEQUENTIAL; // MADV_NORMAL or MADV_SEQUENTIAL or MADV_RANDOM
  // assume that file will be accessed soon
  linuxHint |= MADV_WILLNEED;
  // assume that file will be large
  linuxHint |= MADV_HUGEPAGE;

  const int madvise_result = ::madvise(_mappedView, _filesize, linuxHint);
  if (madvise_result == -1) {
    return false;
  }

#endif

  // everything's fine
  return true;
}

/// open file for writing
bool MemoryMapped::open_write(const char *filename, size_t size) {
  // already open ?
  if (isValid()) {
    return false;
  }

  _file = 0;
  _filesize = size;
#ifdef _MSC_VER
  _mappedFile = NULL;
#endif
  _mappedView = NULL;

#ifdef _MSC_VER
  // Windows

  // open file
  // FILE_FLAG_SEQUENTIAL_SCAN or FILE_FLAG_RANDOM_ACCESS or
  // FILE_ATTRIBUTE_NORMAL
  _file =
      ::CreateFileA(filename, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE,
                    NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  if (!_file) {
    std::cerr << "CreateFileA Win32 error code: " << GetLastError() << "\n";
    return false;
  }

  LARGE_INTEGER file_size_WIN;
  file_size_WIN.QuadPart = _filesize;
  // convert to mapped mode
  _mappedFile =
      ::CreateFileMapping(_file, NULL, PAGE_READWRITE, file_size_WIN.u.HighPart,
                          file_size_WIN.u.LowPart, NULL);
  if (!_mappedFile) {
    std::cerr << "CreateFileMapping Win32 error code: " << GetLastError()
              << "\n";
    return false;
  }

  // get memory address
  _mappedView = ::MapViewOfFile(_mappedFile, FILE_MAP_WRITE, 0, 0, _filesize);
  if (_mappedView == NULL) {
    std::cerr << "MapViewOfFile Win32 error code: " << GetLastError() << "\n";
    return false;
  }

#else
  // Linux

  // open file
  const mode_t rw = 0660;
  _file = ::open(filename, O_CREAT | O_RDWR | O_LARGEFILE | O_TRUNC, rw);
  if (_file == -1) {
    _file = 0;
    return false;
  }

  // grow file size
  int truncate_result = truncate(filename, _filesize);
  if (truncate_result == -1) {
    return false;
  }

  _mappedView = ::mmap64(NULL, _filesize, PROT_WRITE, MAP_SHARED, _file, 0);
  if (_mappedView == MAP_FAILED) {
    _mappedView = NULL;
    return false;
  }

  // tweak performance
  int linuxHint =
      MADV_SEQUENTIAL; // MADV_NORMAL or MADV_SEQUENTIAL or MADV_RANDOM
  // assume that file will be accessed soon
  linuxHint |= MADV_WILLNEED;
  // assume that file will be large
  linuxHint |= MADV_HUGEPAGE;

  const int madvise_result = ::madvise(_mappedView, _filesize, linuxHint);
  if (madvise_result == -1) {
    return false;
  }

#endif

  // everything's fine
  return true;
}

/// close file
void MemoryMapped::close() {
  // kill pointer
  if (_mappedView) {
#ifdef _MSC_VER
    const BOOL ok = ::UnmapViewOfFile(_mappedView);
    if (!ok) {
      std::cerr << "UnmapViewOfFile Win32 error code: " << GetLastError()
                << "\n";
    }
#else
    ::munmap(_mappedView, _filesize);
#endif
    _mappedView = NULL;
  }

#ifdef _MSC_VER
  if (_mappedFile) {
    const BOOL ok = ::CloseHandle(_mappedFile);
    if (!ok) {
      std::cerr << "CloseHandle(_mappedFile) Win32 error code: "
                << GetLastError() << "\n";
    }
    _mappedFile = NULL;
  }
#endif

  // close underlying file
  if (_file) {
#ifdef _MSC_VER
    const BOOL ok = ::CloseHandle(_file);
    if (!ok) {
      std::cerr << "CloseHandle(_file) Win32 error code: " << GetLastError()
                << "\n";
    }
#else
    ::close(_file);
#endif
    _file = 0;
  }

  _filesize = 0;
}

/// raw access
void *MemoryMapped::accessData() const { return _mappedView; }

/// true, if file successfully opened
bool MemoryMapped::isValid() const { return _mappedView != NULL; }

/// get file size
uint64_t MemoryMapped::size() const { return _filesize; }
