#include <catch2/catch.hpp>

#include <initializer_list>
#include <ios>
#include <iostream>
#include <cstdint>

#include "lfsr_engine.hpp"

TEST_CASE("16 bit with 8 bit output")
{
    constexpr std::uint16_t mask = 0x4A20;
    constexpr std::uint16_t seed = 0xD35E;
    linear_feedback_shift_register_engine<std::uint16_t, mask> sr(seed);

    CHECK(sr.operator()<std::uint8_t>() == 0xD3);
    CHECK(sr.operator()<std::uint8_t>() == 0x22);
    CHECK(sr.operator()<std::uint8_t>() == 0x1F);
    CHECK(sr.operator()<std::uint8_t>() == 0x11);
    CHECK(sr.operator()<std::uint8_t>() == 0x6A);
    CHECK(sr.operator()<std::uint8_t>() == 0xBD);
    CHECK(sr.operator()<std::uint8_t>() == 0x20);
    CHECK(sr.operator()<std::uint8_t>() == 0xA5);
}

TEST_CASE("16 bit with 2 bit output")
{
    constexpr std::uint16_t mask = 0x4A20;
    constexpr std::uint16_t seed = 0xD35E;
    linear_feedback_shift_register_engine<std::uint16_t, mask> sr(seed);

    std::initializer_list<unsigned> output = {
       3, 1, 1, 3, 0, 1, 3, 2, 0, 2, 0, 3, 3, 1, 0, 1, 0, 1, 0, 2, 2, 2, 1, 1, 3, 3, 2, 0, 0, 2, 0, 1 
    };

    for(unsigned expt : output)
        CHECK(sr.get_bits(2) == expt);
}
