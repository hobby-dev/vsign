// //////////////////////////////////////////////////////////
// MemoryMapped.h
// Copyright (c) 2013 Stephan Brumme. All rights reserved.
// see http://create.stephan-brumme.com/disclaimer.html
//
// Changes by Ivan Kuznetsov for "vsign" project:
//  - avoid paddings in memory layout
//  - add write support
//  - remove unused code
//  - error reporting
//  - clang-format

#pragma once

// define fixed size integer types
#ifdef _MSC_VER
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

#include <string>

/// Portable memory mapping (Windows and Linux)
/** Filesize limited by size_t 2^64 */
class MemoryMapped {
public:
  /// do nothing, must use open()
  MemoryMapped();
  /// close file (see close() )
  ~MemoryMapped();

  /// open file for reading
  bool open_read(const char *filename);
  /// open file for writing
  bool open_write(const char *filename, size_t size);
  /// close file
  void close();

  /// raw access
  void *accessData() const;

  /// true, if file successfully opened
  bool isValid() const;

  /// get file size
  uint64_t size() const;

private:
  /// don't copy object
  MemoryMapped(const MemoryMapped &);
  /// don't copy object
  MemoryMapped &operator=(const MemoryMapped &);

  /// get OS page size (for remap)
  static int getpagesize();

  /// file size
  uint64_t _filesize;

  /// define handle
#ifdef _MSC_VER
  /// Windows handle to memory mapping of _file
  void *_mappedFile;
  /// file handle
  void *_file;
#else
  int64_t _file;
#endif

  /// pointer to the file contents mapped into memory
  void *_mappedView;
};
