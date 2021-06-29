# UniValue JSON Library for C++17 (and above)

An easy-to-use and competitively fast JSON parsing library for C++17, forked from Bitcoin Cash Node's own UniValue library.

Supports parsing and serializing, as well as modeling a JSON document.  The central class is `UniValue`, a universal value class, with JSON encoding and decoding methods. `UniValue` is an abstract data type that may be a null, boolean, string, number, array container, or a key/value dictionary container, nested to an arbitrary depth. This class implements  the JSON standard, [RFC 8259](https://tools.ietf.org/html/rfc8259).

## Summary of Differences from other JSON Libraries

- Faster than many implementations.  For example, faster than [nlohmann](https://github.com/nlohmann/json) for both parsing and for stringification (roughly 2x faster in many cases).
  - You can run the `json_test` binary from this library on the [nlohmann bench json files](https://github.com/nlohmann/json_test_data) to convince yourself of this.
- Easier to use, perhaps?
  - The entire implementation is wrapped by a single class, called `UniValue` which captures any JSON data item, as well as the whole document, with a single abstraction.
- "Faithful" representation of input JSON.
  - Stores the read JSON faithfully without "reinterpreting" anything.  For example if the input document had a JSON numeric `1.000000`, this library will re-serialize it verbatim as `1.000000` rather than `1.0` or `1`.
   - The reason for this: when this library parses JSON numerics, they are internally stored as string fragments (validation is applied, however, to ensure that invalid numerics cannot make it in).
   - JSON numerics are actually really parsed to ints or doubles "on-demand" only when the caller actually asks for a number via a getter method.
- Does not use `std::map` or other map-like structures for JSON objects.  JSON objects are implemented as a `std::vector` of `std::pair`s.
   - This has the benefit of fast inserts when building or parsing the JSON object. (No need to balance r-b trees, etc).
   - Inserts preserve the order of insert (which can be an advantage  or a disadvantage, depending on what matters to you; they are sort of like Python 3.7+ dicts in that regard).
   - Inserts do not check for dupes -- you can have the same key appear twice in the object (something which the JSON specification allows for but discourages).
   - Lookups are O(N), though -- but it is felt that for most usages of a C++ app manipulating JSON, this is an acceptable tradeoff.
     - In practice many applications merely either parse JSON and iterate over keys, or build the object up once to be sent out on the network or saved to disk immediately -- in such usecases the `std::vector` approach for JSON objects is faster & simpler.
- Nesting limits:
  - Unlimited for serializing/stringification
  - **512** for parsing (as a simple DoS measure)
    - This can be changed by modifying a compile-time constant in [`univalue_read.cpp`](https://github.com/cculianu/univalue/blob/master/lib/univalue_read.cpp#L31).

## Background

UniValue was originally created by [Jeff Garzik](https://github.com/jgarzik/univalue/) and is used in node software for many bitcoin-based cryptocurrencies.

**BCHN UniValue** was a fork of UniValue designed and maintained for use in [Bitcoin Cash Node (BCHN)](https://bitcoincashnode.org/).

**This library** is a fork of the above implementation, optimized and maintained by me, [Calin A. Culianu](mailto:calin.culianu@gmail.com)

Unlike the [Bitcoin Core fork](https://github.com/bitcoin-core/univalue/), this UniValue library contains major improvements to *code quality* and *performance*. This library's UniValue API differs slightly from its ancestor projects.

#### How this library differs from its ancestor UniValue libaries:

- Optimizations made to parsing (about 1.7x faster than the BCHN library, and several times faster than the Bitcoin Core library)
- Optimizations made to memory consumption (each UniValue nested instance eats only 32 bytes of memory, as opposed to 80 or more bytes in the other implementations)
- Various small nits and improvements to code quality

## License

This library is released under the terms of the MIT license. See [COPYING](COPYING) for more information or see <https://opensource.org/licenses/MIT>.

## Build Instructions

- `mkdir build && cd build`
- `cmake -GNinja ..`
- `ninja all check`

The above will build and run the unit tests, as well as build the shared library. Alternatively, you can just **put the source files** from the [`lib/`](lib) and [`include/`](include) folders into your project.  

This library requires C++17 or above.
