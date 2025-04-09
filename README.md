# Decoder/Encoder Library for RISC-V Nexus Trace

This library implements the decoder/encoder of RISC-V Nexus Trace (1.0 Ratified spec).
It's inspired by the [Reference Code](https://github.com/riscv-non-isa/tg-nexus-trace/tree/1.0_Ratified/refcode/c).
Compared to the reference code, it abstracts Nexus Messages into C structures for easier interface with C/C++.

## Build

```shell
cmake [-DBUILD_SHARED_LIBS=ON|OFF] [-DUTIL=ON|OFF] ... -B <build-dir>
cmake --build <build-dir>
```
 * `-DBUILD_SHARED_LIBS` Controls whether to generate shared libraries (default ON)
 * `-DUTIL` Controls whether to build utilities (default ON)

## Install
```shell
cmake --install <build-dir> # Don't forget to set -DCMAKE_INSTALL_PREFIX during build
```

## Documentation

The API Documentation is embedded as inline comments in the header files.
 * For decoder [decoder.h](include/libnexus-rv/decoder.h)
 * For encoder [encoder.h](include/libnexus-rv/encoder.h)

They should be self-explanatory.

## Limitations

For more efficient storage of data structures, currently the size of the following fields are reduced.
During decoding, higher bits are discarded.
 * `SRC 12 -> 10`
 * `X-ADDR 63 -> 58`
 * `TIMESTAMP 64 -> 54`

This is considered a reasonable compromise:
 * 10-bit SRC gives 1024 Harts maximum.
 * 58-bit X-ADDR can reconstruct 59-bit PC, which already covers Sv57x4
 * 54-bit TIMESTAMP wraps around in ~208.5 days for 1ns timer

## Utilities

The library providers two utilities and demonstrate the use of decoder/encoder:

* nexusrv-dump: decodes raw trace into human readable text
```shell
nexusrv-dump: [OPTIONS...] <trace file> or - for stdin

	-h, --help       Display this help message
	-y, --sync       Synchronize before decoding
	-t, --timestamp  Enable timestamp
	-s, --srcbits    Bits of SRC field, default 0
	-b, --buffersz   Buffer size (default 4096)
	-r, --raw        Decode to raw binary

```

* nexusrv-assemble: encodes human readable text info raw trace
```shell
	nexusrv-assemble: [OPTIONS...] [<output trace file> or stdout if not specified] 

	-h, --help       Display this help message
	-t, --timestamp  Enable timestamp
	-s, --srcbits    Bits of SRC field, default 0
	-x, --text       Text mode
```

# Bug report
Post on [github issues](https://github.com/ganboing/libnexus-rv/issues) for bug report and suggestions. Thanks.