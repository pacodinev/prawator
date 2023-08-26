#pragma once

#include <cassert>
#include <cstddef>
#include <iterator>
#include <vector>

#include "tile.hpp"

namespace WaTor {

    class MapLine {
    private:
        std::pmr::vector<bool> m_topMask, m_bottomMask, m_updateMask;
        std::pmr::vector<Tile> m_map;
        unsigned m_height, m_width;

    public:
        // type deffinitions:
        using TileIter = decltype(m_map)::iterator;
        using UpdateMaskIter = decltype(m_updateMask)::iterator;
        using TopMaskIter = decltype(m_topMask)::iterator;
        using BottomMaskIter = decltype(m_bottomMask)::iterator;

        // member functions:

        MapLine(unsigned height, unsigned width, std::pmr::memory_resource *mmr) 
            : m_topMask(width, false, mmr), m_bottomMask(width, false, mmr), m_updateMask(width, false, mmr), 
              m_map(static_cast<std::size_t>(width)*height, Tile(), mmr), m_height(height), m_width(width) 
              {

            // std::clog << "Creating line with: " << width << ' ' << height << '\n'; // TODO: comment out
        }

        [[nodiscard]] unsigned getWidth() const noexcept { return m_width; }
        [[nodiscard]] unsigned getHeight() const noexcept { return m_height; }

        [[nodiscard]] std::size_t getAbsSize() const noexcept { return m_map.size(); }

        [[nodiscard]] Tile& get(unsigned posy, unsigned posx) noexcept {
            assert(posy < m_height);
            assert(posx < m_width);
            return m_map[posy*m_width + posx];
        }
        [[nodiscard]] const Tile& get(unsigned posy, unsigned posx) const noexcept {
            assert(posy < m_height);
            assert(posx < m_width);
            return m_map[posy*m_width + posx];
        }

        [[nodiscard]] Tile& getAbs(std::size_t indx) {
            assert(indx < m_map.size());
            return m_map[indx];
        }
        [[nodiscard]] const Tile& getAbs(std::size_t indx) const {
            assert(indx < m_map.size());
            return m_map[indx];
        }

        [[nodiscard]] std::pmr::vector<bool>::reference getUpdateMask(unsigned posx) {
            assert(posx < m_width);
            return m_updateMask[posx];
        }
        [[nodiscard]] bool getUpdateMask(unsigned posx) const {
            assert(posx < m_width);
            return m_updateMask[posx];
        }
        
        [[nodiscard]] std::pmr::vector<bool>::reference getTopMask(unsigned posx) {
            assert(posx < m_width);
            return m_topMask[posx];
        }
        [[nodiscard]] bool getTopMask(unsigned posx) const {
            assert(posx < m_width);
            return m_topMask[posx];
        }

        [[nodiscard]] std::pmr::vector<bool>::reference getBottomMask(unsigned posx) {
            assert(posx < m_width);
            return m_bottomMask[posx];
        }
        [[nodiscard]] bool getBottomMask(unsigned posx) const {
            assert(posx < m_width);
            return m_bottomMask[posx];
        }

        [[nodiscard]] TileIter getTileIter(unsigned posy, unsigned posx) {
            const std::size_t adist = posy*m_width + posx;
            assert(adist < m_map.size());
            TileIter res = m_map.begin();
            std::advance(res, adist);
            return res;
        }

        [[nodiscard]] UpdateMaskIter getUpdateMaskIter(unsigned posx) {
            assert(posx < m_width);
            UpdateMaskIter res = m_updateMask.begin();
            std::advance(res, posx);
            return res;
        }

        [[nodiscard]] TopMaskIter getTopMaskIter(unsigned posx) {
            assert(posx < m_width);
            TopMaskIter res = m_topMask.begin();
            std::advance(res, posx);
            return res;
        }

        [[nodiscard]] BottomMaskIter getBottomMaskIter(unsigned posx) {
            assert(posx < m_width);
            BottomMaskIter res = m_bottomMask.begin();
            std::advance(res, posx);
            return res;
        }

    };
}
