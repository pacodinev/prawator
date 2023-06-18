#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>

namespace WaTor {

    struct Rules {
        std::size_t width;
        std::size_t height;
        std::size_t initialFishCount;
        std::size_t initialSharkCount;
        std::uint16_t fishBreedTime, sharkBreedTime;
        std::uint16_t sharkStarveTime;

        static constexpr unsigned MAX_FISH_BREED_TIME = ((1U<<4U)-1)-1;
        static constexpr unsigned MAX_SHARK_BREED_TIME = ((1U<<4U)-1)-1;
        static constexpr unsigned MAX_SHARK_STARVE_TIME = ((1U<<4U)-1)-1;

        Rules(std::size_t wid, std::size_t hei, std::size_t initFishCnt, std::size_t initSharkCnt, 
              unsigned fishBreedT, unsigned sharkBreedT, unsigned sharkStarveT)
            : width(wid), height(hei), initialFishCount(initFishCnt),
              initialSharkCount(initSharkCnt),
              fishBreedTime(static_cast<std::uint16_t>(fishBreedT)), 
              sharkBreedTime(static_cast<std::uint16_t>(sharkBreedT)), 
              sharkStarveTime(static_cast<std::uint16_t>(sharkStarveT)) {

            if(width == 0 || height == 0) {
                throw std::runtime_error("Width and height could not be zero!");
            }

            std::size_t tilesCnt = width * height;

            if(tilesCnt/width != height) {
                throw std::runtime_error("Width and height are too large!");
            }

            if(fishBreedTime > MAX_FISH_BREED_TIME || 
               sharkBreedTime > MAX_SHARK_BREED_TIME || 
               sharkStarveTime > MAX_SHARK_STARVE_TIME) {
                throw std::runtime_error("fish breed time, shark breed time or shark starve time are too large!");
            }

        }
    };

}
