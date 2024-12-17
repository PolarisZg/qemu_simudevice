#ifndef WIRELESS_SIMU_NUM
#define WIRELESS_SIMU_NUM

#include <stdint.h>

/**
 * ROUNDUP_POW_OF_TWO - 将整数 n 向上取整到最近的 2 的幂
 * 支持 u8, u16, u32, u64 类型。
 */
#define roundup_pow_of_two(n) \
    _Generic((n), \
        uint8_t:  roundup_pow_of_two_u8, \
        uint16_t: roundup_pow_of_two_u16, \
        uint32_t: roundup_pow_of_two_u32, \
        uint64_t: roundup_pow_of_two_u64)(n)

static inline uint8_t roundup_pow_of_two_u8(uint8_t n) {
    return (n <= 1) ? 1 : (1U << (8 - __builtin_clz(n - 1)));
}

static inline uint16_t roundup_pow_of_two_u16(uint16_t n) {
    return (n <= 1) ? 1 : (1U << (16 - __builtin_clz(n - 1)));
}

static inline uint32_t roundup_pow_of_two_u32(uint32_t n) {
    return (n <= 1) ? 1 : (1U << (32 - __builtin_clz(n - 1)));
}

static inline uint64_t roundup_pow_of_two_u64(uint64_t n) {
    return (n <= 1) ? 1 : (1ULL << (64 - __builtin_clzll(n - 1)));
}


// 处理非对齐读取
#define READ_64BIT_FROM_ADDR(addr) ({           \
    uint64_t _val;                             \
    memcpy(&_val, (addr), sizeof(uint64_t));   \
    _val;                                      \
})
#endif /*WIRELESS_SIMU_NUM*/