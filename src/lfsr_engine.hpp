#pragma once

#include <limits>
#include <type_traits>
#include <climits>
#include <cassert>

template<class UIntType, UIntType m>
class linear_feedback_shift_register_engine
{
public:
    static_assert(std::is_unsigned_v<UIntType>, "UIntType should be unsigned type");

    using result_type = UIntType;

    static constexpr UIntType mask = m;
    static constexpr UIntType default_seed = 1;


private:
    UIntType cur_val;

    static unsigned popcount(unsigned long long val)
    {
        return static_cast<unsigned>(__builtin_popcountll(val));
    };

    static unsigned popcount(unsigned long val)
    {
        return static_cast<unsigned>(__builtin_popcountl(val));
    };

    static unsigned popcount(unsigned val)
    {
        return static_cast<unsigned>(__builtin_popcount(val));
    };

    static unsigned popcount(int val)
    {
        return static_cast<unsigned>(__builtin_popcount(static_cast<unsigned>(val)));
    };



    void shift_reg(unsigned shift_cnt = sizeof(UIntType)*CHAR_BIT) {
        for(unsigned i=0; i<shift_cnt; i++) {
            UIntType new_bit = popcount(cur_val & mask) & 1U;
            cur_val = (cur_val >> 1) | (new_bit << (sizeof(UIntType)*CHAR_BIT-1));
        }
    }

public:
    linear_feedback_shift_register_engine() : linear_feedback_shift_register_engine(default_seed) {}

    linear_feedback_shift_register_engine(UIntType seed) {
        cur_val = seed;
    }

    void seed(UIntType seed = default_seed) {
        cur_val = seed;
    }

    template<class T = result_type>
    T operator()() {
        static_assert(std::is_unsigned_v<T>, "UIntType should be unsigned type");
        shift_reg(sizeof(T)*CHAR_BIT);
        return static_cast<T>(cur_val);
    }

    template<class T = unsigned>
    T get_bits(unsigned bit_cnt) {
        assert(bit_cnt > 0);
        assert(bit_cnt <= sizeof(UIntType)*CHAR_BIT);
        shift_reg(bit_cnt);
        return static_cast<T>(cur_val & ((1U << bit_cnt) - 1));
    }

    template<class T = unsigned>
    T operator()(unsigned bit_cnt) {
        return get_bits(bit_cnt);
    }

    static constexpr result_type min() {
        return std::numeric_limits<UIntType>::min();
    }

    static constexpr result_type max() {
        return std::numeric_limits<UIntType>::max();
    }


};

