LibXISF
=========

LibXISF is C++ library that can read and write XISF files produced by [PixInsight](https://pixinsight.com/).
It implement [XISF 1.0 specification](https://pixinsight.com/doc/docs/XISF-1.0-spec/XISF-1.0-spec.html).
It is licensed under GPLv3 or later. To compile you will need C++17 compiler and zlib, lz4, pugixml and zstd libraries.
Zstd is optional but then you would not be able to open files that use this compression method.

To compile simply run these commands
```
cmake -B build -S .
cmake --build build --parallel
cmake --install build
```

If you wish to bundled libraries you will may add `-DUSE_BUNDLED_LIBS=On` to first cmake configuration command. 
Then you will need *lz4 pkg-config pugixml zlib* installed. You may also specify `-DBUILD_SHARED_LIBS=Off` 
if you want build static lib.
