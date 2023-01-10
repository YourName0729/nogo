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

constexpr uint128 shifted(int sft) {
    uint128 re = 1;
    re <<= sft;
    return re;
}

constexpr uint128 lsb(uint128 v) { return (v & -v); }
constexpr uint128 reset(uint128 v) { return (v & (v - 1)); }

// int bit_scan(uint128 v) {
//     constexpr const static uint128 debrujin = make_uint128(0x0106143891634793, 0x2A5CD9D3EAD7B77F);
//     int re = 0, step = 64;
//     while (step) {
//         if (v >> step) re += step, v >>= step;
//         step >>= 1;
//     }
//     return re;
// }

int ith_one(uint128 cur, uint128 v) {
    int re = 0;
    while (lsb(cur) != v) ++re, cur = reset(cur);
    return re;
}

int bit_count(uint128 v) {
    const constexpr uint128 flt1 = make_uint128(0x5555555555555555, 0x5555555555555555);
    const constexpr uint128 flt2 = make_uint128(0x3333333333333333, 0x3333333333333333);
    const constexpr uint128 flt3 = make_uint128(0x0f0f0f0f0f0f0f0f, 0x0f0f0f0f0f0f0f0f);
    const constexpr uint128 mul  = make_uint128(0x0101010101010101, 0x0101010101010101);
    // const constexpr uint128 flt4 = make_uint128(0x00ff00ff00ff00ff, 0x00ff00ff00ff00ff);
    // const constexpr uint128 flt5 = make_uint128(0x0000ffff0000ffff, 0x0000ffff0000ffff);
    v = ((v & flt1)) + ((v >> 1) & flt1);
    v = ((v & flt2)) + ((v >> 2) & flt2);
    v = ((v & flt3)) + ((v >> 4) & flt3);
    return (v * mul) >> 120;
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

