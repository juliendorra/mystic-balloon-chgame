// SPDX-License-Identifier: MIT
#pragma once
#include "Journal.h"

namespace chsave {

// CHGame Rev0 adapter. Calls wait for the display, then borrow CHGfx's
// scratch buffer. Use after chgame.boot() and gfx_begin(), between frames.
class Store {
public:
    bool load(const Id &gameId, uint8_t schema, void *data, uint16_t size,
              uint8_t *flags = nullptr);
    bool store(const Id &gameId, uint8_t schema, const void *data, uint16_t size,
               uint8_t flags = 0);
    Status status() const { return status_; }

    template<class T>
    bool load(const Id &gameId, uint8_t schema, T &data, uint8_t *flags = nullptr) {
        static_assert(std::is_trivially_copyable<T>::value, "save plain data only");
        static_assert(sizeof(T) <= MAX_DATA, "save payload exceeds CHSAVE_MAX_SECTORS_PER_RECORD capacity");
        if (MAX_SLOT_SECTORS == 1) return load(gameId, schema, &data, sizeof(T), flags);
        T candidate{};
        if (!load(gameId, schema, &candidate, sizeof(T), flags)) return false;
        memcpy(&data, &candidate, sizeof(T));
        return true;
    }
    template<class T>
    bool store(const Id &gameId, uint8_t schema, const T &data, uint8_t flags = 0) {
        static_assert(std::is_trivially_copyable<T>::value, "save plain data only");
        static_assert(sizeof(T) <= MAX_DATA, "save payload exceeds CHSAVE_MAX_SECTORS_PER_RECORD capacity");
        return store(gameId, schema, &data, sizeof(T), flags);
    }
private:
    Status status_ = Status::Empty;
};

} // namespace chsave
