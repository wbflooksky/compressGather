#ifndef COMPRESS_GATHER_H
#define COMPRESS_GATHER_H

#include <iostream>
#include <sstream>
#include <vector>
#include <string>

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

int bin_hex[7] = { BIN_MAP_HEX::BIN_MAP_HEX_ONE, BIN_MAP_HEX::BIN_MAP_HEX_TWO, 
                   BIN_MAP_HEX::BIN_MAP_HEX_THREE, BIN_MAP_HEX::BIN_MAP_HEX_FOUR, 
                   BIN_MAP_HEX::BIN_MAP_HEX_FIVE, BIN_MAP_HEX::BIN_MAP_HEX_SIX, BIN_MAP_HEX::BIN_MAP_HEX_SEVEN };

// type include int and int64, applies to all integers.
template<typename T>
std::stringstream integerCompress(const std::vector<T>& integer_set) {
    std::stringstream compress;
    unsigned char temp_integer;
    T integer;
    int int_bit = sizeof(integer) * SEEK_LENGTH::SEEK_LENGTH_WRITE - 1;
    for (int i = 0; i < integer_set.size(); i++) {
        integer = (integer_set[i] << 1) | (integer_set[i] >> int_bit);
        while (integer > 0x7f) {
            temp_integer = 0x80;
            temp_integer += integer & 0x7f;
            compress << temp_integer;
            integer = integer >> 7;
        }
        temp_integer = integer & 0x7f;
        compress << temp_integer;
    }
    return compress;
}

template<typename T>
std::vector<T> integerUncompress(std::stringstream& compress) {
    std::vector<T> integer_set;
    int integer_length = sizeof(T) * SEEK_LENGTH::SEEK_LENGTH_WRITE;
    int use_bit = 0;
    unsigned char temp_value;
    T integer = 0, temp_integer = 0;
    auto seekLength = [&](int bit) {
        use_bit += bit;
        temp_integer = temp_value;
        integer = (integer >> bit) | (temp_integer << (integer_length - bit));
    };
    // A restore operation for a compression shift
    auto seekInteger = [&]() {
        seekLength(integer_length - use_bit);
        integer = (integer >> 1) | (integer << (integer_length - 1));
        use_bit = 0;
    };
    while (compress >> temp_value) {
        if (temp_value > 0x7f) {
            seekLength(7);
        }
        else {
            seekInteger();
            integer_set.push_back(integer);
            integer = 0;
        }
    }
    return integer_set;
}

std::stringstream standardIntegerCompress(const std::vector<int> &integer_set) {
    if (integer_set.empty()) {
        std::stringstream compress;
        return compress;
    }
    std::vector<uint32_t> unsigned_integer_set;
    for (auto integer : integer_set) {
        // In order to be compatible with negative Numbers, All integers have to do a loop to the left
        if (integer < 0) {
            integer = ~(integer - 1);
        }
        unsigned_integer_set.push_back(integer);
    }
    return integerCompress(unsigned_integer_set);
}

std::stringstream longIntegerCompress(const std::vector<int64_t> &integer_set) {
    if (integer_set.empty()) {
        std::stringstream compress;
        return compress;
    }
    std::vector<uint64_t> unsigned_integer_set;
    for (auto integer : integer_set) {
        // In order to be compatible with negative Numbers, All integers have to do a loop to the left
        if (integer < 0) {
            integer = ~(integer - 1);
        }
        unsigned_integer_set.push_back(integer);
    }
    return integerCompress(unsigned_integer_set);
}

std::vector<int> standardIntegerUncompress(std::stringstream &compress) {
    std::vector<int> integer_set;
    std::vector<uint32_t> unsigned_integer_set = integerUncompress<uint32_t>(compress);
    for (auto integer : unsigned_integer_set) {
        integer_set.push_back(integer);
    }
    return integer_set;
}

std::vector<int64_t> longIntegerUncompress(std::stringstream &compress) {
    std::vector<int64_t> integer_set;
    std::vector<uint64_t> unsigned_integer_set = integerUncompress<uint64_t>(compress);
    for (int i = 0; i < unsigned_integer_set.size(); i++) {
        integer_set.push_back(unsigned_integer_set[i]);
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

    unsigned int previous_value = 0, new_value = 0;
    int previous_begin_zero = 0, previous_end_zero = 0;
    int new_begin_zero = 0, new_end_zero = 0;

    unsigned char temp_value;
    uint64_t output = 0;
    int use_bit = 0;
    int count = 0;
    auto seekOutput = [&](int seek_bit, uint64_t add_value) {
        bool is_write = false;
        use_bit += seek_bit;
        output = (output << seek_bit) + add_value;
        // seek ready
        if (use_bit > SEEK_LENGTH::SEEK_LENGTH_WRITE_STREAM_THRESHOLD) {
            is_write = true;
            output = output << (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - use_bit);
        }
        while (use_bit > SEEK_LENGTH::SEEK_LENGTH_WRITE_STREAM_THRESHOLD) {
            temp_value = output >> (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - SEEK_LENGTH::SEEK_LENGTH_WRITE);
            output = output << SEEK_LENGTH::SEEK_LENGTH_WRITE;
            compress << temp_value;
            count++;
            use_bit -= SEEK_LENGTH::SEEK_LENGTH_WRITE;
        }
        // seek restore
        if (is_write)
            output = output >> (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - use_bit);
    };

    auto countXor = [&]() {
        unsigned int result = new_value ^ previous_value;
        unsigned int temp_result = result;
        new_begin_zero = 0;
        new_end_zero = 0;
        while (temp_result > 0) {
            new_begin_zero++;
            temp_result = temp_result >> 1;
        }
        temp_result = result;
        while (temp_result > 0) {
            new_end_zero++;
            temp_result = temp_result << 1;
        }
        new_begin_zero = SEEK_LENGTH::SEEK_LENGTH_FLOAT - new_begin_zero;
        new_end_zero = SEEK_LENGTH::SEEK_LENGTH_FLOAT - new_end_zero;
        return result;
    };
    // write frist value
    memcpy(&previous_value, begin++, sizeof(float));
    seekOutput(SEEK_LENGTH::SEEK_LENGTH_FLOAT, previous_value);

    for (int i = 1; i < len; i++) {
        // TODO calculate delta
        memcpy(&new_value, begin++, sizeof(float));
        unsigned int result = countXor();
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
            seekOutput(SEEK_LENGTH::SEEK_LENGTH_REPETITION, COMPARED_PREVIOUS::COMPARED_PREVIOUS_REPETITION);
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
        temp_value = output >> (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - SEEK_LENGTH::SEEK_LENGTH_WRITE);
        output = output << SEEK_LENGTH::SEEK_LENGTH_WRITE;
        compress << temp_value;
        count++;
        use_bit -= SEEK_LENGTH::SEEK_LENGTH_WRITE;
    }
    std::cout << "count = " << count << std::endl;
    return compress;
}

std::vector<float> floatUncompress(std::stringstream &compress) {
    std::vector<float> float_set;
    uint64_t intput = 0;
    unsigned char temp_value = 0;
    float temp_float = 0;

    unsigned int previous_value = 0, new_value = 0;
    int previous_begin_zero = 0, previous_end_zero = 0;
    
    int use_bit = 0, valid_bit = 0;
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
        int temp_integer = intput >> SEEK_LENGTH::SEEK_LENGTH_FLOAT;
        memcpy(&new_value, &temp_integer, sizeof(float));
        new_value = (new_value >> (previous_end_zero + previous_begin_zero)) << previous_end_zero;
        previous_value = new_value ^ previous_value;

        memcpy(&temp_float, &previous_value, sizeof(float));
        float_set.push_back(temp_float);
        intput = intput << valid_bit;
        use_bit -= valid_bit;
    };
    auto prase = [&]() {
        new_value = 0;
        intput = intput << (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - use_bit);

        temp_value = intput >> (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - SEEK_LENGTH::SEEK_LENGTH_SIGN);
        if (temp_value == COMPARED_PREVIOUS::COMPARED_PREVIOUS_SHARE) {
            intput = intput << SEEK_LENGTH::SEEK_LENGTH_SIGN;
            use_bit -= SEEK_LENGTH::SEEK_LENGTH_SIGN;
            valid_bit = SEEK_LENGTH::SEEK_LENGTH_FLOAT - previous_begin_zero - previous_end_zero;
            copyFloat();
        } else if (temp_value == COMPARED_PREVIOUS::COMPARED_PREVIOUS_INDEPENDENT) {
            intput = intput << SEEK_LENGTH::SEEK_LENGTH_SIGN;
            use_bit -= SEEK_LENGTH::SEEK_LENGTH_SIGN;

            getBit(SEEK_LENGTH::SEEK_LENGTH_HEAD_ZERO, previous_begin_zero);
            getBit(SEEK_LENGTH::SEEK_LENGTH_END_ZERO, previous_end_zero);
            valid_bit = SEEK_LENGTH::SEEK_LENGTH_FLOAT - previous_begin_zero - previous_end_zero;
            if (valid_bit < 0) {
                use_bit = 0;
                return float_set;
            }

            copyFloat();
        } else {
            intput = intput << SEEK_LENGTH::SEEK_LENGTH_REPETITION;
            use_bit -= SEEK_LENGTH::SEEK_LENGTH_REPETITION;
            float_set.push_back(temp_float);
        }
        intput = intput >> (SEEK_LENGTH::SEEK_LENGTH_LONG_INT - use_bit);
    };

    auto seekIntput = [&]() {
        use_bit += SEEK_LENGTH::SEEK_LENGTH_WRITE;
        intput = (intput << SEEK_LENGTH::SEEK_LENGTH_WRITE) + temp_value;
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
    return float_set;
}

int accuracyToMultiple(const int accuracy) {
    int true_accracy = accuracy;
    if (true_accracy > sizeof(bin_hex) / sizeof(bin_hex[0])) {
        true_accracy = sizeof(bin_hex) / sizeof(bin_hex[0]);
    }
    int multiple = 1;
    for (int i = 0; i < true_accracy; i++) {
        multiple *= 10;
    }
    return multiple;
}
// only for float value between (-1, 0) && (0, 1)
std::stringstream floatLossyCompress(const std::vector<float> &float_set, const int accuracy) {
    unsigned int temp_int;
    int multiple = accuracyToMultiple(accuracy);
    std::vector<int32_t> integer_set;
    for (auto float_value : float_set) {
        int32_t seek_bit = float_value * multiple;
        integer_set.push_back(seek_bit);
    }
    return standardIntegerCompress(integer_set);
}

std::vector<float> floatLossyUncompress(std::stringstream &compress, const int accuracy) {
    std::vector<float> float_set;
    float temp_float;
    int multiple = accuracyToMultiple(accuracy);
    std::vector<int32_t> integer_set = standardIntegerUncompress(compress);
    for (auto temp_int : integer_set) {
        temp_float = (float)temp_int / multiple;
        float_set.push_back(temp_float);
    }
    return float_set;
}

}
#endif