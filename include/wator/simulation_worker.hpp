#pragma once

#include "map.hpp"
#include "rules.hpp"
#include "../src/lfsr_engine.hpp" // TODO: this

namespace WaTor {

class SimulationWorker {

private:
    
    static constexpr unsigned LFSR_MASK = 0xDEADBEEF;

    Map &m_map;
    Rules m_rules;
    linear_feedback_shift_register_engine<std::uint32_t, LFSR_MASK> m_rng;
    unsigned m_numaInd, m_lineInd;

    [[nodiscard]] unsigned findTileFish(const std::array<Map::Cordinate, 4> &dirs, 
                              unsigned rnd) const;

    [[nodiscard]] unsigned findTileShark(const std::array<Map::Cordinate, 4> &dirs, 
                              unsigned rnd, bool& ate) const;

    //0 - x=0
    //1 - mid
    //2 - x=height-1
    template<bool isBotLvl, unsigned horLevel, bool isShark>
    unsigned tickEntity( const Map::Cordinate &cur, 
            const std::array<Map::Cordinate, 4> &dirs);

    struct PosCache {
        Map::Cordinate pos;
        std::array<Map::Cordinate, 4> dirs;
        union Tbmcache {
            // top and bot are (like) plain old data, union is not problem (probably)
            MapLine::TopMaskIter top;
            MapLine::BottomMaskIter bot;

            Tbmcache() {} // NOLINT
        } curCache, neigCache;
    };

    template<unsigned vertLevel, unsigned horLevel>
    void updatePosCache(unsigned posy, unsigned posx, PosCache &cache);

    template<unsigned vertLevel, unsigned horLevel>
    void updateEntity(unsigned posy, unsigned posx, PosCache &cache);

public:
    SimulationWorker(Map &map, unsigned numaInd, unsigned lineInd, 
                     const Rules &rules, unsigned seed);

    void operator() ();

};
 
}
