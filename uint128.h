#pragma once

#include <cstdint>
#include <iostream>
#include <iomanip>

using uint128 = __uint128_t;

constexpr uint128 make_uint128(std::uint64_t h, std::uint64_t l) {
    uint128 v = h;
    v <<= 64;
    v += l;
    return v;
}

// std::ostream& operator<<(std::ostream& out, const uint128& v) {
//     out << std::hex << std::setfill('0') << std::setw(16) << static_cast<uint64_t>(v >> 64) << " | " << std::hex << std::setfill('0') << std::setw(16) << static_cast<uint64_t>(v);
//     return out;
// }

std::ostream& print(std::ostream& out, const __uint128_t& v) {
    // out << std::hex << std::setfill('0') << std::setw(16);
    out << static_cast<uint64_t>(v >> 64) << " | " << static_cast<uint64_t>(v);
    return out;
}

