#pragma once

#include <stdint.h>
#include <byteswap.h>

#include <bit> //C++20
#include <concepts> //C++20

namespace nemo {

/**
 * @brief 如果是C++23的话，可以直接用标准库的byteswap
 * @tparam T 
 * @param value 
 * @return T 
 */

template<std::integral T>
T ByteSwap(T value) {
    if constexpr(sizeof(T) == sizeof(uint64_t)) {
        return static_cast<T>(bswap_64(static_cast<uint64_t>(value)));
    } else if constexpr(sizeof(T) == sizeof(uint32_t)) {
        return static_cast<T>(bswap_32(static_cast<uint32_t>(value)));
    } else if constexpr(sizeof(T) == sizeof(uint16_t)) {
        return static_cast<T>(bswap_16(static_cast<uint16_t>(value)));
    }

    return value;
}

/**
 * @brief 只在小端机器上执行byteswap, 在大端机器上什么都不做
 */
template<std::integral T>
T ByteSwapOnLittleEndian(T value) {
    if constexpr(std::endian::native == std::endian::little) {
        return ByteSwap(value);
    }

    return value;
}

/**
 * @brief 只在大端机器上执行byteswap, 在小端机器上什么都不做
 */
template<std::integral T>
T ByteSwapOnBigEndian(T value) {
    if constexpr(std::endian::native == std::endian::big) {
        return ByteSwap(value);
    }

    return value;
}

} // namespace nemo