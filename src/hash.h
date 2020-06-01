#pragma once

namespace Hash {

template < typename T >
struct Hash : std::false_type {};

constexpr unsigned long djb2_hash(const char* str) {
    return str[0] == 0 ? 0 : str[0] + (5381 * djb2_hash(str + 1));
}

inline size_t djb2_hash(const char* str, size_t size, size_t seed = 5381) {
    size_t hash = seed;
    for (size_t i = 0; i < size; i++) {
        hash = ((hash << 5) + hash) + str[i];
    }

    return hash;
}

template <>
struct Hash< const char* > {
    inline size_t operator()(const char* string) const {
        return djb2_hash(string);
    }
};

}