// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

namespace chsave {
namespace detail {
// FAT short names are case-insensitive: 36^8 distinct eight-character names.
constexpr uint64_t NAME_SPACE = 2821109907456ULL;
constexpr uint64_t idHash(const char *s, unsigned n = 0,
                          uint64_t h = 14695981039346656037ULL) {
    return !s || !s[n] || n == 253 ? h :
        idHash(s, n + 1, (h ^ uint8_t(s[n])) * 1099511628211ULL);
}
constexpr bool idValid(const char *s, unsigned n = 0, unsigned label = 0,
                        unsigned dots = 0, bool dash = false) {
    return !s ? false : !s[n] ? label && dots && !dash : n == 253 ? false :
        s[n] == '.' ? label && !dash && idValid(s, n + 1, 0, dots + 1) :
        label < 63 && ((s[n] >= 'a' && s[n] <= 'z') ||
        (s[n] >= '0' && s[n] <= '9') || (s[n] == '-' && label)) &&
        idValid(s, n + 1, label + 1, dots, s[n] == '-');
}
constexpr unsigned idLength(const char *s, unsigned n = 0) {
    return !s || !s[n] || n == 253 ? n : idLength(s, n + 1);
}
constexpr char idDigit(uint64_t h, uint64_t place) {
    return (h / place % 36) < 10 ? char('0' + h / place % 36) :
        char('A' + h / place % 36 - 10);
}
}

// Keep this object and its text alive throughout each operation. Literal IDs
// can be constexpr, so hashing and base-36 conversion happen at compile time.
class Id {
    const char *text_;
    char basename_[9];
    uint8_t size_;
    constexpr Id(const char *text, uint64_t hash)
        : text_(text), basename_{
            detail::idDigit(hash, 78364164096ULL), detail::idDigit(hash, 2176782336ULL),
            detail::idDigit(hash, 60466176ULL), detail::idDigit(hash, 1679616ULL),
            detail::idDigit(hash, 46656ULL), detail::idDigit(hash, 1296ULL),
            detail::idDigit(hash, 36ULL), detail::idDigit(hash, 1ULL), 0},
          size_(detail::idValid(text) ? detail::idLength(text) : 0) {}
public:
    explicit constexpr Id(const char *text) : Id(text, detail::idHash(text) % detail::NAME_SPACE) {}
    constexpr const char *text() const { return text_; }
    constexpr const char *basename() const { return basename_; }
    constexpr uint8_t size() const { return size_; }
};
constexpr Id id(const char *text) { return Id(text); }
}
