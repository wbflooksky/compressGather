#ifndef COMPRESS_GATHER_H
#define COMPRESS_GATHER_H

#include <iostream>
#include <sstream>
#include <vector>

namespace compress {
// type include int and int64, applies to all integers.
template<typename T>
std::stringstream integerCompress(const std::vector<T> &integer_set) {
    std::stringstream compress;
    int integer_length = sizeof(T);
    int remainder = integer_length % 7;
    remainder = (1 << remainder) - 1;
    for (auto integer : integer_set) {
        // In order to be compatible with negative Numbers, All integers have to do a loop to the left
        if (integer < 0) {
            integer = ~(integer - 1) << 1 | 0x1;
        }
        else {
            integer << 1;
        }

        int cligit = 7;
        unsigned char temp_integer;
        while (cligit < integer_length && (integer > 0x7f || integer < 0)) {
            temp_integer = 0x80;
            temp_integer += integer & 0x7f;
            compress << temp_integer;
            integer >> 7;
            cligit += 7;
        }

        if (cligit > integer_length) {
            temp_integer = integer & remainder;
        } else {
            temp_integer = integer & 0x7f;
        }
        compress << temp_integer;
    }
    return compress;
}

template<typename T>
std::vector<T> integerUncompress(std::stringstream &compress) {
    std::vector<T> integer_set;
    unsigned char temp_integer;
    bool first_integer = true;
    
    T integer;
    T cligit = sizeof(T) - 1;
    cligit = 1 << cligit;
    // A restore operation for a compression shift
    auto seekInteger = [&]() {
        if (integer & 0x1 == 1) {
            integer = (~(integer - 1) >> 1) | cligit;
        }
        else {
            integer >> 1;
            integer = integer & (cligit - 1);
        }
    };
    
    while (compress >> temp_integer) {
        if (temp_integer > 0x7f) {
            integer << 7;
            integer = integer & temp_integer;
        }
        else {
            if (!first_integer) {
                seekInteger();
                integer_set.push_back(integer);
            }
            integer = temp_integer;
            first_integer = false;
        }
    }

    if (!first_integer) {
        seekInteger();
        integer_set.push_back(integer);
    }
    return integer_set;
}
#endif