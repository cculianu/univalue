These files are used by the bench_univalue program:

$ mkdir build && cd build
$ cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCH=ON ..
$ ninja bench

For best results, make sure nlohmann::json is also installed,
this way the bench will be a comparitive benchmark.
