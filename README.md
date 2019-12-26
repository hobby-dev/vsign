# vsign

Creates binary signature of file contents. 
Works as fast as your storage. 
Uses [Meow Hash](https://github.com/cmuratori/meow_hash) - 
the best hash that I could find for this particual type of work.

## Prerequisites

 - Windows or Linux
 - x86-64 C++14 compiler

## Building

### Windows (using Visual Studio)

 - make sure you have relevant Windows SDK installed
 - run `cmd` in vsign dir
 - run `C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat` or equivalent for your Visual Studio version
 - run `build.bat`

### Linux (using clang++)

 - run `build.sh`

## Usage:

Experimental software! Use at your own risk! 
Overwrites output file.

```
vsign -h

Usage: vsign [OPTIONS] INPUT_FILE [OUTPUT_FILE]

Creates binary signature of contents of INPUT_FILE and writes to OUTPUT_FILE
(by default will write to 'INPUT_FILE.signature')

Options:
 -b		Block size (bytes), default is 1 048 576 bytes
 -h		Print help text
 -t		Threads count, equals to number of logical cores by default 
 -v		Verbose output
 -y		Verify that OUTPUT_FILE contains correct signature of INPUT_FILE
```

## Known issues:

Check [github issues page](https://github.com/ivan-cx/vsign/issues).
Please, report any issues you find.

