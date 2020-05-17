# compressGather
Some compression algorithms for common types
Including integer compression algorithm,both positive and negative Numbers are common reference zig 7bit.
Lossless compression of floating point Numbers reference gorilla paper encoding.
Lossy compression of floating point Numbers, you need to pass in a few decimal places, Floating-point compression is currently supported only type float, Lossy compression reuse integer method.

// signed integer compress
std::vector<int> integer_set;
for (int i = 0; i < 1024; i++) {
    integer_set.push_back(i);
}
auto stream_str = compress::standardIntegerCompress(integer_set);
auto integer_result = compress::standardIntegerUncompress(stream_str);
for (auto integer : integer_result) {
    std::cout << result << ",";
}

// unsigned integer compress
std::vector<uint64_t> unsigned_integer_set;
for (int i = 0; i < 1024; i++) {
    unsigned_integer_set.push_back(i);
}
auto stream_str = compress::integerCompress<uint64_t>(unsigned_integer_set);
auto result_vectors = compress::integerUncompress<uint64_t>(stream_str);
for (auto integer : integer_result) {
    std::cout << result << ",";
}

// float compress
std::vector<float> float_set;
for (int i = 0; i < 1024; i++) {
    temp_vector.push_back(i * 0.0001);
}
// keep four digits behind the decimal point
auto stream_str = compress::floatLossyCompress(float_set, 4);
auto result_vectors = compress::floatLossyUncompress(stream_str, 4);
for (auto result : result_vectors) {
    std::cout << result << ",";d
}