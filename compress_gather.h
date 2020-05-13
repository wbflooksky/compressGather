#ifndef COMPRESS_GATHER_H
#define COMPRESS_GATHER_H

#include <iostream>
#include <sstream>
#include <vector>

namespace compress {
enum COMPARED_PREVIOUS {
    COMPARED_PREVIOUS_REPETITION = 0,
    COMPARED_PREVIOUS_END = 1,/* already give up */
    COMPARED_PREVIOUS_SHARE = 2,
    COMPARED_PREVIOUS_INDEPENDENT = 3
};
enum SEEK_LENGTH {
    SEEK_LENGTH_REPETITION = 1,
    SEEK_LENGTH_SIGN = 2,
    SEEK_LENGTH_HEAD_ZERO = 5,
    SEEK_LENGTH_END_ZERO = 6,
    SEEK_LENGTH_WRITE = 8, 
    SEEK_LENGTH_WRITE_STREAM_THRESHOLD = 19,
    SEEK_LENGTH_FLOAT_END_COUNT = 23,
    SEEK_LENGTH_FLOAT = 32,
    SEEK_LENGTH_LONG_INT = 64
};
enum BIN_MAP_HEX {
    BIN_MAP_HEX_ONE = 4,
    BIN_MAP_HEX_TWO = 7,
    BIN_MAP_HEX_THREE = 10,
    BIN_MAP_HEX_FOUR = 14,
    BIN_MAP_HEX_FIVE = 17,
    BIN_MAP_HEX_SIX = 20,
    BIN_MAP_HEX_SEVEN = 23 // some lossy
};
int bin_hex[7] = {BIN_MAP_HEX::BIN_MAP_HEX_ONE, BIN_MAP_HEX::BIN_MAP_HEX_TWO, BIN_MAP_HEX::BIN_MAP_HEX_THREE, BIN_MAP_HEX::BIN_MAP_HEX_FOUR, BIN_MAP_HEX::BIN_MAP_HEX_FIVE, BIN_MAP_HEX::BIN_MAP_HEX_SIX, BIN_MAP_HEX::BIN_MAP_HEX_SEVEN};
// type include int and int64, applies to all integers.
template<typename T>
std::stringstream integerCompress(const std::vector<T> &integer_set, const int &integer_bit = 0) {
    std::stringstream compress;
    int integer_length = sizeof(T) * SEEK_LENGTH::SEEK_LENGTH_WRITE;
    int remainder_bit = integer_length % 7;
    if (integer_bit != 0) {
        integer_length = integer_bit;
    }
    int remainder = (1 << remainder_bit) - 1;

    for (auto integer : integer_set) {
        // In order to be compatible with negative Numbers, All integers have to do a loop to the left
        if (integer < 0) {
            integer = (~(integer - 1)) << 1 | 0x1;
        }
        else {
            integer = integer << 1;
        }

        int cligit = 7;
        unsigned char temp_integer;
        while (cligit < integer_length && (integer > 0x7f || integer < 0)) {
            temp_integer = 0x80;
            temp_integer += integer & 0x7f;
            compress << temp_integer;
            integer = integer >> 7;
            cligit += 7;
        }

        if (cligit > integer_length) {
            temp_integer = integer & remainder;
            if (integer < 0) {
                temp_integer += (1 << (integer_bit - 1));
            }
        } else {
            temp_integer = integer & 0x7f;
        }
        compress << temp_integer;
    }
    return compress;
}

template<typename T>
std::vector<T> integerUncompress(std::stringstream &compress, const int integer_bit = 0) {
    if (compress)
    std::vector<T> integer_set;
    int integer_length = sizeof(T) * SEEK_LENGTH::SEEK_LENGTH_WRITE;
    int remainder_bit = integer_length % 7;
    if (integer_bit != 0) {
        integer_length = integer_bit;
    }
    unsigned char temp_integer;
    bool first_integer = true;
    
    T integer;
    T cligit = (1 << integer_length) - 1;
    auto seekLength = [&](int bit) {
        auto temp_value = integer;
        temp_value = temp_integer;
        integer = (integer >> bit) & (temp_value << (integer_length - bit));
    };
    // A restore operation for a compression shift
    auto seekInteger = [&]() {
        seekLength(remainder_bit);
        if (integer & 0x1 == 1) {
            integer = (~(integer - 1) >> 1) | cligit;
        }
        else {
            integer = (integer >> 1) & cligit;
        }
    };
    
    while (compress >> temp_integer) {
        if (temp_integer > 0x7f) {
            seekLength(7);
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

// gorilla paper encoding
std::stringstream floatCompress(const std::vector<float> &float_set) {
    std::stringstream compress;
    auto begin = float_set.data();
    int len = float_set.size();
    if (!len) {
        return compress;
    }

    unsigned int previous_value, new_value;
    int previous_begin_zero, previous_end_zero;
    int new_begin_zero, new_end_zero;

    unsigned char temp_value;
    uint64_t output = 0;
    int use_bit = 0;

    auto seekOutput = [&](int seek_bit, uint64_t add_value) {
        bool is_write = false;
        use_bit += seek_bit;
        output = output << seek_bit;
        output += add_value;
        // seek ready
        if (use_bit > SEEK_LENGTH::SEEK_LENGTH_WRITE_STREAM_THRESHOLD) {
            is_write = true;
            output = output << (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - use_bit);
        }
        while (use_bit > SEEK_LENGTH::SEEK_LENGTH_WRITE_STREAM_THRESHOLD) {
            temp_value = (output & 0xff00000000000000) >> (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - SEEK_LENGTH::SEEK_LENGTH_WRITE);
            output = output << SEEK_LENGTH::SEEK_LENGTH_WRITE;
            compress << temp_value;
            use_bit -= SEEK_LENGTH::SEEK_LENGTH_WRITE;
        }
        // seek restore
        if (is_write)
            output = output >> (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - use_bit);
    };

    // write frist value
    memcpy(&previous_value, begin++, sizeof(float));
    seekOutput(SEEK_LENGTH::SEEK_LENGTH_FLOAT, previous_value);

    for (int i = 1; i < len; i++) {
        // TODO calculate delta
        memcpy(&new_value, begin++, sizeof(float));
        unsigned int result = new_value ^ previous_value;
        int valid_bit = SEEK_LENGTH::SEEK_LENGTH_FLOAT - new_begin_zero - new_end_zero;
        // TODO write float
        if (result) {
            if (new_begin_zero == previous_begin_zero && new_end_zero == previous_end_zero) {
                seekOutput(SEEK_LENGTH::SEEK_LENGTH_SIGN, COMPARED_PREVIOUS::COMPARED_PREVIOUS_SHARE);
                new_value = new_value >> new_end_zero;
                seekOutput(valid_bit, new_value);
            }
            else {
                seekOutput(SEEK_LENGTH::SEEK_LENGTH_SIGN, COMPARED_PREVIOUS::COMPARED_PREVIOUS_INDEPENDENT);
                seekOutput(SEEK_LENGTH::SEEK_LENGTH_HEAD_ZERO, new_begin_zero);
                seekOutput(SEEK_LENGTH::SEEK_LENGTH_END_ZERO, new_end_zero);
                new_value = new_value >> new_end_zero;
                seekOutput(valid_bit, new_value);
            }
        }
        else {
            seekOutput(SEEK_LENGTH::SEEK_LENGTH_REPETITION, COMPARED_PREVIOUS::COMPARED_PREVIOUS_REPETITION)
        }
        previous_value = new_value;
        previous_begin_zero = new_begin_zero;
        previous_end_zero = new_end_zero;
    }
    
    seekOutput(SEEK_LENGTH::SEEK_LENGTH_SIGN, COMPARED_PREVIOUS::COMPARED_PREVIOUS_INDEPENDENT);
    seekOutput(SEEK_LENGTH::SEEK_LENGTH_HEAD_ZERO, 0x1f);
    seekOutput(SEEK_LENGTH::SEEK_LENGTH_END_ZERO, 0x3f);
    // write buffer
    output << (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - use_bit);
    while (use_bit > 0) {
        temp_value = (output & 0xff00000000000000) >> (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - SEEK_LENGTH::SEEK_LENGTH_WRITE);
        output = output << SEEK_LENGTH::SEEK_LENGTH_WRITE;
        compress << temp_value;
        use_bit -= SEEK_LENGTH::SEEK_LENGTH_WRITE;
    }
    return compress;
}

std::vector<float> floatUncompress(const std::stringstream &compress) {
    std::stringstream compress;
    std::vector<float> float_set;
    uint64_t intput;
    unsigned char temp_value;
    float temp_float;

    unsigned int previous_value, new_value;
    int previous_begin_zero, previous_end_zero;
    
    int use_bit;
    int valid_bit;
    int memcpy_size;
    // read first
    for (int i = 0; i < sizeof(float); i++) {
        compress >> temp_value;
        previous_value << SEEK_LENGTH::SEEK_LENGTH_WRITE;
        previous_value += temp_value;
    }
    memcpy(&temp_float, &previous_value, sizeof(float));
    float_set.push_back(temp_float);

    auto getBit = [&] (int seek_bit, int &output) {
        output = intput >> (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - seek_bit);
        intput = intput << seek_bit;
        use_bit -= seek_bit;
    };
    auto copyFloat = [&] () {
        memcpy_size = valid_bit / SEEK_LENGTH::SEEK_LENGTH_WRITE + 1;
        if (memcpy_size > sizeof(float)) {
            memcpy_size -= 1;
        }
        memcpy(&new_value, &intput, memcpy_size);
        new_value = (new_value >> previous_end_zero) << previous_end_zero;
        previous_value = new_value ^ previous_value;

        memcpy(&temp_float, &previous_value, sizeof(float));
        float_set.push_back(temp_float);
        intput << valid_bit;
    };
    auto prase = [&]() {
        new_value = 0;
        valid_bit = SEEK_LENGTH::SEEK_LENGTH_FLOAT - previous_begin_zero - previous_end_zero;
        intput = intput << (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - use_bit);

        temp_value = intput >> (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - SEEK_LENGTH::SEEK_LENGTH_SIGN);
        if (temp_value == COMPARED_PREVIOUS::COMPARED_PREVIOUS_SHARE) {
            intput = intput << SEEK_LENGTH::SEEK_LENGTH_SIGN;
            use_bit -= SEEK_LENGTH::SEEK_LENGTH_SIGN;
            copyFloat();
        } else if (temp_value == COMPARED_PREVIOUS::COMPARED_PREVIOUS_INDEPENDENT) {
            intput = intput << SEEK_LENGTH::SEEK_LENGTH_SIGN;
            use_bit -= SEEK_LENGTH::SEEK_LENGTH_SIGN;

            getBit(SEEK_LENGTH::SEEK_LENGTH_HEAD_ZERO, previous_begin_zero);
            getBit(SEEK_LENGTH::SEEK_LENGTH_END_ZERO, previous_end_zero);
            
            if (valid_bit < 0) {
                use_bit = 0;
                return ;
            }

            copyFloat();
        } else {
            intput = intput << SEEK_LENGTH::SEEK_LENGTH_REPETITION;
            use_bit -= SEEK_LENGTH::SEEK_LENGTH_REPETITION;
            float_set.push_back(temp_float);
        }
    };

    auto seekIntput = [&]() {
        use_bit += SEEK_LENGTH::SEEK_LENGTH_WRITE;
        intput << SEEK_LENGTH::SEEK_LENGTH_WRITE;
        intput += temp_value;
        while (use_bit > (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - SEEK_LENGTH::SEEK_LENGTH_WRITE_STREAM_THRESHOLD)) {
            prase();
        }
    };

    while (compress >> temp_value) {
        seekIntput();
    }
    
    // read end
    while (use_bit > 0) {
        prase();
    }
    return compress;
}
// only for float value between (-1, 0) && (0, 1)
std::stringstream floatLossyCompress(const std::vector<float> &float_set, const int accuracy) {
    unsigned int temp_int;
    std::vector<uint32_t> integer_set;
    for (auto float_value : float_set) {
        memcpy(&temp_int, &float_value, sizeof(float));
        int seek_bit = (temp_int >> (SEEK_LENGTH::SEEK_LENGTH_FLOAT - 1)) & (((temp_int >> ((temp_int >> SEEK_LENGTH::SEEK_LENGTH_FLOAT_END_COUNT) & 0xff) >> (SEEK_LENGTH::SEEK_LENGTH_FLOAT_END_COUNT - bin_hex[accuracy]))) << 1);
        integer_set.push_back(seek_bit);
    }
    if (bin_hex[accuracy] < SEEK_LENGTH::SEEK_LENGTH_WRITE) {
        std::stringstream compress;
        unsigned char temp_value;
        for (auto value : float_set) {
            temp_value = value;
            compress << temp_value;
        }
        return compress;
    }
    return integerCompress(integer_set, bin_hex[accuracy] + 1);
}

std::vector<float> floatLossyUncompress(const std::stringstream &compress, const int accuracy) {
    std::vector<float> float_set;
    std::vector<uint32_t> integer_set;
    float temp_float;
    if (bin_hex[accuracy] < SEEK_LENGTH::SEEK_LENGTH_WRITE) {
        unsigned char temp_value;
        while (temp_value << compress) {
            integer_set.push_back(temp_value);
        }
    } else {
        integer_set = integerUncompress(compress, bin_hex[accuracy] + 1);
    }
    for (auto temp_int : integer_set) {
        temp_int = (temp_int << (SEEK_LENGTH::SEEK_LENGTH_FLOAT - 1)) & (0xff >> SEEK_LENGTH::SEEK_LENGTH_FLOAT_END_COUNT) & ((temp_int >> 1) << (SEEK_LENGTH::SEEK_LENGTH_FLOAT_END_COUNT - bin_hex[accuracy]));
        memcpy(&temp_float, &temp_int, sizeof(float));
        float_set.push_back(temp_float);
    }
    return float_set;
}

}
#endif