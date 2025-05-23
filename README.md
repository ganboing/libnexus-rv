# Decoder/Encoder Library for RISC-V Nexus Trace

Github: https://github.com/ganboing/libnexus-rv

This library implements the decoder/encoder of RISC-V Nexus Trace (1.0 Ratified spec).
It's inspired by the [Reference Code](https://github.com/riscv-non-isa/tg-nexus-trace/tree/1.0_Ratified/refcode/c).
It works at two levels -- The Nexus Message level, and the Nexus Trace level.
The Message decoder/encoder operates on a single Nexus Message or a stream of Messages,
and doesn't care about relations between Messages or the surrounding context. It's effectively
a Nexus Message serializer/de-serializer. The Trace decoder (encoder TODO) is stateful, and
keep track of Hart states like a HW Nexus encoder working in reverse. A caller who have knowledge
of program execution context, including the instructions being executed, can interactively
reconstruct the control-flow by asking the Trace encoder to retire a certain number of instructions,
and check for pending events. (See API documentation)

## Demo on P550

To get a rough idea of how decoded trace looks like, refer to this
[HOW-TO doc for trace on p550](https://github.com/ganboing/libnexus-rv/tree/master/contrib/p550#observe-trace-of-userspace--kernel--firmware)
It's able to provide you with instruction level full-system trace.

## Build

```shell
cmake [-DBUILD_SHARED_LIBS=ON|OFF] [-DUTIL=ON|OFF] [-DDOCS=ON|OFF]... -B <build-dir>
cmake --build <build-dir>
```
 * `-DBUILD_SHARED_LIBS` Controls whether to generate shared libraries (default ON)
 * `-DUTIL` Controls whether to build utilities (default ON)
 * `-DDOCS` Controls whether to build documentation (default OFF)

## Install
```shell
cmake --install <build-dir> # Don't forget to set -DCMAKE_INSTALL_PREFIX during build
```

## Documentation

The API Documentation is generated by Doxygen, and published [here](https://ganboing.github.io/libnexus-rv/)

 * [Message decoder](https://ganboing.github.io/libnexus-rv/msg-decoder_8h.html)
 * [Message encoder](https://ganboing.github.io/libnexus-rv/msg-encoder_8h.html)
 * [Trace decoder](https://ganboing.github.io/libnexus-rv/trace-decoder_8h.html)

## Utilities

The library providers two utilities and demonstrate the use of Message decoder/encoder:

* nexusrv-dump: Dump NexusRV Messages into human-readable text
* nexusrv-assemble: Assemble NexusRV Messages from human-readable text
* nexusrv-split: Split the NexusRV Messages into per-SRC files
* nexusrv-replay: Replay the control-flow by decoding the NexusRV Trace

# Bug report
Post on [github issues](https://github.com/ganboing/libnexus-rv/issues) for bug report and suggestions. Thanks.