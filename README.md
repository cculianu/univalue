# UniValue JSON Library for C++17 (and above)

A simple and relatively fast JSON parsing library for C++17, forked from Bitcoin Cash Node's own UniValue library.

## Overview

This is your basic JSON library. Supports parsing and serializing, as well as modeling a JSON document.  The central class is `UniValue`, a universal value class, with JSON encoding and decoding methods. `UniValue` is an abstract data type that may be a null, boolean, string, number, array container, or a key/value dictionary container, nested to an arbitrary depth (512 nesting limit, but configurable by modifying a compile-time constant). This class implements  the JSON standard, [RFC 8259](https://tools.ietf.org/html/rfc8259).

UniValue was originally created by [Jeff Garzik](https://github.com/jgarzik/univalue/) and is used in node software for many bitcoin-based cryptocurrencies.

**BCHN UniValue** was a fork of UniValue designed and maintained for use in [Bitcoin Cash Node (BCHN)](https://bitcoincashnode.org/).

**This library** is a fork of the above implementation, optimized and maintained by me, [Calin A. Culianu](mailto:calin.culianu@gmail.com)

Unlike the [Bitcoin Core fork](https://github.com/bitcoin-core/univalue/), this UniValue library contains major improvements to *code quality* and *performance*. This library's UniValue API differs slightly from its ancestor projects.

#### How this library differs from its ancestor UniValue libaries:

- Optimizations made to parsing (about 1.7x faster than the BCHN library, and several times faster than the Bitcoin Core library)
- Optimizations made to memory consumption (each UniValue nested instance eats onluy 32 bytes if memory, as opposed to 80 or more bytes in the other implementations)
- Various small nits and improvements to code quality

## License

This library is released under the terms of the MIT license. See [COPYING](COPYING) for more information or see <https://opensource.org/licenses/MIT>.

## Build Instructions

- `mkdir build && cd build`
- `cmake -GNinja ..`
- `ninja all check`

The above will build and run the unit tests, as well as build the shared library. Alternatively, you can just **put the source files** from the [`lib/`](lib) and [`include/`](include) folders into your project.  

This library requires C++17 or above.
