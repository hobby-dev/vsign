# vsign

This is obsolete version of "vsign" project.
It's left here as a reference point.
Check out master branch for up-to-date version.

## Cons (why it's obsolete)

It's 2x slower than master version.
It's not using all available cores for multi-threaded processing.
I found that increasing number of worker threads in this type of architecture
does not improve performance. Indeed, it makes things slower.
Also, code is quite complicated, compared to the master.

## Pros

It does not use platform-dependent code (memory mapping) -> less library code, less potential undetected errors.
Better manual memory control compared to memory mapping solution:
all allocations are tweakable, even libc's cache size.


