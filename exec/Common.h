//
// Created by 赵程 on 2021/4/22.
//

#ifndef DISTRIBUTEDHASHINDEXING_COMMON_H
#define DISTRIBUTEDHASHINDEXING_COMMON_H

#include <utils/Type.h>
#include <limits>

using KeyT = UInt64;
using ValueT = UInt64;

using SlotID = UInt32;
using InsertPack = std::tuple<KeyT, ValueT, SlotID>;
using InsertionPool = std::vector<InsertPack>;

const UInt32 POOL_SIZE{64u << 20}; // 64MB
const auto pool_capacity = POOL_SIZE / sizeof(KeyT); // TODO:

static const auto ERROR_INDEX = std::numeric_limits<ValueT>::max();

const static UInt64 RETRY_TIME_OUT(10);
inline static size_t hashKey(KeyT x) {
    // TODO: CityHash or MurmurHash for String
    // UInt64
    x = (x ^ (x >> 31) ^ (x >> 62)) * UINT64_C(0x319642b2d24d8ec3);
    x = (x ^ (x >> 27) ^ (x >> 54)) * UINT64_C(0x96de1b173f119089);
    x = x ^ (x >> 30) ^ (x >> 60);
    return x;
}

/**
 * Given a value "word", produces an integer in [0,p) without division.
 * The function is as fair as possible in the sense that if you iterate
 * through all possible values of "word", then you will generate all
 * possible outputs as uniformly as possible.
 */
static inline uint32_t fastrange32(uint32_t word, uint32_t p) {
    // http://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
    return (uint32_t) (((uint64_t) word * (uint64_t) p) >> 32);
}

/**
 * Given a value "word", produces an integer in [0,p) without division.
 * The function is as fair as possible in the sense that if you iterate
 * through all possible values of "word", then you will generate all
 * possible outputs as uniformly as possible.
 */
static inline uint64_t fastrange64(uint64_t word, uint64_t p) {
    // http://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
#ifdef __SIZEOF_INT128__ // then we know we have a 128-bit int
    return (uint64_t) (((__uint128_t) word * (__uint128_t) p) >> 64);
#elif defined(_MSC_VER) && defined(_WIN64)
    // supported in Visual Studio 2005 and better
    uint64_t highProduct;
    _umul128(word, p, &highProduct); // ignore output
    return highProduct;
    unsigned __int64 _umul128(
        unsigned __int64 Multiplier,
        unsigned __int64 Multiplicand,
        unsigned __int64 *HighProduct
    );
#else
    return word % p; // fallback
#endif // __SIZEOF_INT128__
}


#endif //DISTRIBUTEDHASHINDEXING_COMMON_H
