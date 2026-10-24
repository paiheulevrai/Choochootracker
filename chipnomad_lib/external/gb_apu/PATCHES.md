# Local integration changes

`gb_apu.c` is compiled as C++17 by the existing project build. The calloc result
has an explicit pointer cast. Five sparse C99 designated-initializer tables are
expanded into positional entries with identical implicit-zero values so GCC 9
can compile them. All table indices and values are preserved; no synthesis or
filter policy is changed. Provenance records both upstream and patched hashes.
