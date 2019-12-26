# vsign

Creates binary "signature" of contents of files.
Code is as simple and fast as I can manage so far.
Using Meow Hash (https://github.com/cmuratori/meow_hash) - 
the best hash that I could find for this particual type of work.

## Prerequisites

x86-64 C++14 compiler

## Building

### Windows (using Visual Studio)

 - make sure you have relevant Windows SDK installed
 - run `cmd` in vsign dir
 - run `C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat` or equivalent for your Visual Studio version
 - run `build.bat`

### Linux (using clang++)

 - run `build.sh`

## Usage:

Not very well tested, use at your own risk!
Overwrites output file.

TODO: usage examples

## Known issues:

Check [github issues page](https://github.com/ivan-cx/vsign/issues).
Please, report any issues you find.

## Plan:

Project:
 * [ ] Generate test file (must be 4Gb+)
 * [ ] Documentation in README.md

Consider:
 * [ ] oink/cqual++
 * [ ] PVS studio for open source project?

Code:
 * [ ] handle out of disk space?
 * [ ] Optimization: 2 threads per core to utilize cache?
 * [ ] Optimization: mtune=native?

Done:
 * [ ] use some algos from boost? like hash, program options parsing, logging? - the answer is "NO", just "no"
 * [x] CMake build infrastructure (also, build.sh) - [dropped this later]
 * [x] Parameters: block size, threads limit, input, output, verbosity, verify-written, memmorymap
 * [x] open file 
 * [x] Platform abstraction: what C++ STL can offer in C++ 14/17? - will use atomics and std::thread
 * [x] min block size
 * [x] max block size: 1Gb notification for slowness, but proceed [dropped this later]
 * [x] memory limit [dropped this later]
 * [x] output running time
 * [x] start worker thread
 * [x] split work into chunks [dropped this later]
 * [x] Jobs container [dropped this later]
 * [x] correct occupation of slots
 * [x] fix Hang!!!
 * [x] output multithreading overhead [dropped this later]
 * [x] Debug assertions
 * [x] simple output
 * [ ] Identify bottlenecks on HDD/SSD/Tape/etc and consider implementing special code for such cases ("strategy" - buffered read VS memory mappnig?) - no, too much hassle
 * [ ] output thread - no need to!
 * [x] Refactor WorkerData into encapsulated Worker class [dropped this later]
 * [ ] streaming output (write chunks of data, not 1 by 1) - this is already done by kernel (buffered I/O)
 * [ ] modular architecture - core, input, processing, output, error handling, logging with verbosity, testing - nah, forget it
 * [x] Meow Hash
 * [x] help for command line arguments
 * [x] License.txt
 * [x] cppcheck
 * [x] valgrind
 * [x] clang-format
 * [x] stability - fix last block with -t 8
 * [x] atomic relaxed?
 * [ ] care for big endian? - no, we are x86-64 only
 * [ ] why 2000 freads of 1 megabyte takes less time than 2 freads of 1 gigabyte? - I don't know exact answer, but probably it's due to buffering in kernel and copying to userspace, should not be an issue since we use memory mapping now
 * [ ] check what is default buffer size (in C lib) is and if it helps to tweak it? - no need, since we use memory mapping now
 * [x] calculate output file size before mapping output
 * [x] number of worker threads = logical cores by default
 * [x] MemoryMapped solution? - we are not 100% sequential, we need 2 files and 0 copies. According to my tests, memory-mapped solution is 2x faster than straightforward buffered I/O solution.
 * [x] nandle exception std::thread
 * [ ] what are other exception places? - who knows? any std::stuff potentially could throw. Installed general execption handlers.
 * [x] Smart pointers for resources
 * [x] push to github
 * [x] build.bat for windows
 * [x] compile on Windows
