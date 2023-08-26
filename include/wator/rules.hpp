#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>

#include "wator/tile.hpp"

namespace WaTor {

    class Rules {
    private:
        unsigned m_height;
        unsigned m_width;
        std::size_t m_initialFishCount;
        std::size_t m_initialSharkCount;
        std::uint16_t m_fishBreedTime, m_sharkBreedTime;
        std::uint16_t m_sharkStarveTime;


    public:
        static constexpr unsigned MAX_FISH_BREED_TIME = Tile::MAX_AGE;
        static constexpr unsigned MAX_SHARK_BREED_TIME = Tile::MAX_AGE;
        static constexpr unsigned MAX_SHARK_STARVE_TIME = Tile::MAX_LAST_ATE;

        Rules(unsigned height, unsigned width, std::size_t initFishCnt, std::size_t initSharkCnt, 
              unsigned fishBreedT, unsigned sharkBreedT, unsigned sharkStarveT)
            : m_height(height), m_width(width), m_initialFishCount(initFishCnt),
              m_initialSharkCount(initSharkCnt),
              m_fishBreedTime(static_cast<std::uint16_t>(fishBreedT)), 
              m_sharkBreedTime(static_cast<std::uint16_t>(sharkBreedT)), 
              m_sharkStarveTime(static_cast<std::uint16_t>(sharkStarveT)) {

            if(m_width == 0 || m_height == 0) {
                throw std::runtime_error("Width and height could not be zero!");
            }

            std::size_t tilesCnt = static_cast<std::size_t>(m_width) * m_height;

            if(tilesCnt/m_width != m_height) {
                throw std::runtime_error("Width and height are too large!");
            }

            if(m_fishBreedTime > MAX_FISH_BREED_TIME || 
               m_sharkBreedTime > MAX_SHARK_BREED_TIME || 
               m_sharkStarveTime > MAX_SHARK_STARVE_TIME) {
                throw std::runtime_error("fish breed time, shark breed time or shark starve time are too large!");
            }

        }

        [[nodiscard]] unsigned getWidth() const noexcept { return m_width; }
        [[nodiscard]] unsigned getHeight() const noexcept { return m_height; }
        [[nodiscard]] std::size_t getInitialFishCnt() const noexcept { return m_initialFishCount; }
        [[nodiscard]] std::size_t getInitialSharkCnt() const noexcept { return m_initialSharkCount; }
        [[nodiscard]] std::uint16_t getFishBreedTime() const noexcept { return m_fishBreedTime; }
        [[nodiscard]] std::uint16_t getSharkBreedTime() const noexcept { return m_sharkBreedTime; }
        [[nodiscard]] std::uint16_t getSharkStarveTime() const noexcept { return m_sharkStarveTime; }

    };
}
