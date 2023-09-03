#include "utils.hpp"
#include "wator/entity.hpp"
#include "wator/rules.hpp"
#include "wator/simulation_worker.hpp"
#include "wator/tile.hpp"
#include <limits>

namespace {
    unsigned fastMod(unsigned num, unsigned denum) {
        switch(denum) {
            case 1: return 0;
            case 2: return num%2;
            case 3: return num%3;
            case 4: return num%4;
            default: assert(0);
        }
    }
}

namespace WaTor {

    SimulationWorker::SimulationWorker(Map &map, unsigned numaInd, unsigned lineInd, 
            const Rules &rules, unsigned seed) 
        : m_map(map), m_rules(rules), m_rng((seed != 0) ? seed : 1337), 
        m_numaInd(numaInd), m_lineInd(lineInd) { // NOLINT
    }
    
    unsigned SimulationWorker::findTileFish(const std::array<Map::Cordinate, 4> &dirs, 
                                                unsigned rnd) const {
        std::array<unsigned, 4> waterDirs; // NOLINT
        unsigned waterDirsFilled{0};

        for(unsigned i=0; i<dirs.size(); ++i) {
            if(m_map.get(dirs[i]).getEntity() == Entity::WATER) {
                waterDirs[waterDirsFilled++] = i; 
            }
        }

        if(waterDirsFilled > 0) {
            unsigned resInd = fastMod(rnd, waterDirsFilled);
            return waterDirs[resInd];
        }

        // no free cells, we stay here!
        return std::numeric_limits<unsigned>::max();
    }

    unsigned SimulationWorker::findTileShark(const std::array<Map::Cordinate, 4> &dirs, 
                                                unsigned rnd, bool& ate) const {
        std::array<unsigned, 4> waterDirs; // NOLINT
        unsigned waterDirsFilled{0};
        std::array<unsigned, 4> fishDirs; // NOLINT
        unsigned fishDirsFilled{0};

        for(unsigned i=0; i<dirs.size(); ++i) {
            Entity ent = m_map.get(dirs[i]).getEntity();
            if(ent == Entity::FISH) {
                fishDirs[fishDirsFilled++] = i;
            } else if(ent == Entity::WATER) {
                waterDirs[waterDirsFilled++] = i;
            }
        }

        if(fishDirsFilled > 0) {
            unsigned resInd = fastMod(rnd, fishDirsFilled);
            ate = true;
            return fishDirs[resInd];
        }
        if(waterDirsFilled > 0) {
            unsigned resInd = fastMod(rnd, waterDirsFilled);
            ate = false;
            return waterDirs[resInd];
        }

        // no free cells, we stay here!
        ate = false;
        return std::numeric_limits<unsigned>::max();
    }


    //0 - x=0
    //1 - mid
    //2 - x=height-1
    template<bool isBotLvl, unsigned vertLevel, bool isShark>
    unsigned SimulationWorker::tickEntity( const Map::Cordinate &cur, 
            const std::array<Map::Cordinate, 4> &dirs) {
        static_assert(0 <= vertLevel && vertLevel <=2 , "Invalid argument");
        assert(isBotLvl == (cur.posy() + 1 == m_map.getMapLineHeight(cur.numaInd(), cur.lineInd())));
        assert((vertLevel == 0) == (cur.posx() == 0));
        assert((vertLevel == 2) == (cur.posx() + 1 == m_map.getMapLineWidth(cur.numaInd(), cur.lineInd())));
        constexpr bool isFirstCol = (vertLevel == 0);
        constexpr bool isLastCol = (vertLevel == 2);
        constexpr bool isFish = !isShark;
        Tile &curTile = m_map.get(cur);

        unsigned breedTime;
        if constexpr(isFish) {
            assert(curTile.getEntity() == Entity::FISH);
            breedTime = m_rules.getFishBreedTime();
        } else if constexpr(isShark) {
            assert(curTile.getEntity() == Entity::SHARK);
            breedTime = m_rules.getSharkBreedTime();
        }

        bool breeding;

        if(curTile.getAge() >= breedTime) {
            breeding = true;
        } else {
            curTile.setAge(curTile.getAge()+1);
            breeding = false;
        }

        unsigned rnd = m_rng.get_bits(4);

        unsigned nextDir;

        if constexpr(isFish) {
            nextDir = findTileFish(dirs, rnd);
        } else if constexpr(isShark) {
            bool ate;
            nextDir = findTileShark(dirs, rnd, ate);
            if(ate) {
                curTile.setLastAte(0);
            } else {
                if(curTile.getLastAte() >= m_rules.getSharkStarveTime()) {
                    // THE SHARK IS DEAD, RIP SHARK
                    curTile.set(Entity::WATER, 0, 0);
                    return std::numeric_limits<unsigned>::max();
                }    
                curTile.setLastAte(curTile.getLastAte() + 1);
            }
        }

        if(nextDir == std::numeric_limits<unsigned>::max()) {
            return nextDir;
        }

        const Map::Cordinate &newCord{dirs[nextDir]}; // NOLINT
        Tile &newTile = m_map.get(newCord);
        if constexpr(isFish) {
            assert(newTile.getEntity() == Entity::WATER);
        } else if constexpr(isShark) {
            assert(newTile.getEntity() != Entity::SHARK);
        }
        newTile = curTile;

        if(!breeding) {
            curTile.set(Entity::WATER, 0, 0);
        } else {
            curTile.setAge(0);
            newTile.setAge(0);
        }

        if constexpr (isFirstCol) {
            if(nextDir == 3) {
                m_map.getUpdateMask(newCord) = true;
            }
        }
        if constexpr (!isBotLvl) {
            if(nextDir == 2) {
                m_map.getUpdateMask(newCord) = true;
            }
        }
        if constexpr (!isLastCol) {
            if(nextDir == 1) {
                m_map.getUpdateMask(newCord) = true;
            }
        }

        return nextDir;
    }

    template<unsigned horLevel, unsigned vertLevel>
    void SimulationWorker::updatePosCache(unsigned posy, unsigned posx, PosCache &cache) {
        if constexpr(vertLevel == 0 || vertLevel == 1 || vertLevel == 4) {
            cache.pos = m_map.makeCordinate(m_numaInd, m_lineInd, posy, posx);
            for(unsigned d=0; d<4; ++d) {
                cache.dirs[d] = m_map.dirHelper(cache.pos, d);
            }
        } else {
            m_map.dirRightFast(cache.dirs[0]);
            m_map.dirRightFast(cache.dirs[2]);
            cache.dirs[3] = cache.pos;
            cache.pos = cache.dirs[1];
            m_map.dirRightFast(cache.dirs[1]);
        }

        if constexpr(horLevel != 2) {
            if constexpr(vertLevel == 0) {
                if constexpr(horLevel == 0) {
                    cache.curCache.top = m_map.getTopMaskIter(cache.pos);
                    cache.neigCache.bot = m_map.getBottomMaskIter(cache.dirs[0]);
                } else if constexpr(horLevel == 1) {
                    cache.curCache.top = m_map.getTopMaskIter(cache.pos);
                } else if constexpr(horLevel == 3) {
                    cache.curCache.bot = m_map.getBottomMaskIter(cache.pos);
                } else if constexpr(horLevel == 4) {
                    cache.curCache.bot = m_map.getBottomMaskIter(cache.pos);
                    cache.neigCache.top = m_map.getTopMaskIter(cache.dirs[2]);
                }
            } else {
                if constexpr(horLevel == 0) {
                    ++cache.curCache.top;
                    ++cache.neigCache.bot;
                } else if constexpr(horLevel == 1) {
                    ++cache.curCache.top;
                } else if constexpr(horLevel == 3) {
                    ++cache.curCache.bot;
                } else if constexpr(horLevel == 4) {
                    ++cache.curCache.bot;
                    ++cache.neigCache.top;
                }
            }
        }
        
        /*
        if constexpr(vertLevel != 2) {
            if constexpr(horLevel == 0) {
                if constexpr(vertLevel == 0) {
                    cache.curCache.top = m_map.getTopMaskIter(cache.pos);
                    cache.neigCache.bot = m_map.getBottomMaskIter(cache.dirs[0]);
                } else if constexpr(vertLevel == 1) {
                    cache.curCache.top = m_map.getTopMaskIter(cache.pos);
                } else if constexpr(vertLevel == 3) {
                    cache.curCache.bot = m_map.getBottomMaskIter(cache.pos);
                } else if constexpr(vertLevel == 4) {
                    cache.curCache.bot = m_map.getBottomMaskIter(cache.pos);
                    cache.neigCache.top = m_map.getTopMaskIter(cache.dirs[2]);
                }
            } else {
                if constexpr(vertLevel == 0) {
                    ++cache.curCache.top;
                    ++cache.neigCache.bot;
                } else if constexpr(vertLevel == 1) {
                    ++cache.curCache.top;
                } else if constexpr(vertLevel == 3) {
                    ++cache.curCache.bot;
                } else if constexpr(vertLevel == 4) {
                    ++cache.curCache.bot;
                    ++cache.neigCache.top;
                }
            }
        }
        */
    }

    template<unsigned vertLevel, unsigned horLevel>
    void SimulationWorker::updateEntity(unsigned posy, unsigned posx, PosCache &cache) {
        static_assert(0 <= vertLevel && vertLevel < 5, "Invalid vertLevel argument");
        static_assert(0 <= horLevel && horLevel < 5, "Invalid horLevel argument");

        updatePosCache<vertLevel, horLevel>(posy, posx, cache);
        const Map::Cordinate &curCord = cache.pos;
        const std::array<Map::Cordinate, 4>  &dirs = cache.dirs;

        assert((vertLevel == 0) == (curCord.posy() == 0));
        assert((vertLevel == 1) == (curCord.posy() == 1));
        assert((vertLevel == 3) == (curCord.posy() == m_map.getMapLineHeight(curCord.numaInd(), curCord.lineInd()) - 2));
        assert((vertLevel == 4) == (curCord.posy() == m_map.getMapLineHeight(curCord.numaInd(), curCord.lineInd()) - 1));

        assert((horLevel == 0) == (curCord.posx() == 0));
        assert((horLevel == 1) == (curCord.posx() == 1));
        assert((horLevel == 3) == (curCord.posx() == m_map.getMapLineWidth(curCord.numaInd(), curCord.lineInd()) - 2));
        assert((horLevel == 4) == (curCord.posx() == m_map.getMapLineWidth(curCord.numaInd(), curCord.lineInd()) - 1));
        // TODO: assserts

        Tile &curTile {m_map.get(curCord)};
        if(curTile.getEntity() == Entity::WATER) {
            return;
        }

        if(m_map.getUpdateMask(curCord)) {
            m_map.getUpdateMask(curCord) = false;
            return;
        }

        // TODO: fix *Mask
        if constexpr(vertLevel == 0) {
            if(*cache.curCache.top) {
                *cache.curCache.top = false;
                return;
            }
        } else if constexpr(vertLevel == 4) {
            if(*cache.curCache.bot) {
                *cache.curCache.bot = false;
                return;
            }
        }

        constexpr std::array<unsigned, 5> horLvlTickMap = {0, 1, 1, 1, 2};
        constexpr unsigned horLvlTick = horLvlTickMap[horLevel];
        unsigned newDir;
        if(curTile.getEntity() == Entity::FISH) {
            newDir = tickEntity<vertLevel==4, horLvlTick, false>(curCord, dirs);
        } else { 
            newDir = tickEntity<vertLevel==4, horLvlTick, true>(curCord, dirs);
        }
        if constexpr(vertLevel == 2) {
            return;
        }

        if(newDir == std::numeric_limits<unsigned>::max()) {
            return;
        }

        if constexpr(vertLevel == 0) {
            if(newDir == 0) {
                *cache.neigCache.bot = true;
                // m_map.getBottomMask(dirs[0]) = true;
            } else if(newDir == 1) {
                // corner case
                *(cache.curCache.top + 1) = false;
                // m_map.getTopMask(dirs[1]) = false;
            }
        } else if constexpr(vertLevel == 1) {
            if(newDir == 0) {
                // corner case
                *cache.curCache.top = false;
                // m_map.getTopMask(dirs[0]) = false;
            }
        } else if constexpr(vertLevel == 3) {
            if(newDir == 2) {
                // corner case
                // m_map.getBottomMask(dirs[2]) = false;
                *cache.curCache.bot = false;
            }
        } else if constexpr(vertLevel == 4) {
            if(newDir == 2) {
                // m_map.getTopMask(dirs[2]) = true;
                *cache.neigCache.top = true;
            } else if(newDir == 1) {
                // corner case
                // m_map.getBottomMask(dirs[1]) = false;
                *(cache.curCache.bot+1) = false;
            }
        }
        
    }

    void SimulationWorker::operator()() {
        unsigned height = m_map.getMapLineHeight(m_numaInd, m_lineInd);
        unsigned width = m_map.getMapLineWidth(m_numaInd, m_lineInd);

        assert(height > 4 && width > 4);

        unsigned posy=0;
        unsigned posx=0;
        PosCache cache;
        assertMemLocal(cache);
        assertMemLocal(m_rules);

        // 0
        updateEntity<0, 0>(posy, posx, cache); ++posx;
        updateEntity<0, 1>(posy, posx, cache); ++posx;
        for(; posx<width-2; ++posx) {
            updateEntity<0, 2>(posy, posx, cache);
        }
        updateEntity<0, 3>(posy, posx, cache); ++posx;
        updateEntity<0, 4>(posy, posx, cache); // ++posx;

        // 1
        posy = 1; posx = 0;
        updateEntity<1, 0>(posy, posx, cache); ++posx;
        updateEntity<1, 1>(posy, posx, cache); ++posx;
        for(; posx<width-2; ++posx) {
            updateEntity<1, 2>(posy, posx, cache);
        }
        updateEntity<1, 3>(posy, posx, cache); ++posx;
        updateEntity<1, 4>(posy, posx, cache); // ++posx;

        // 2
        posy = 2; posx = 0;
        for(; posy<height-2; ++posy) {
            posx = 0;
            updateEntity<2, 0>(posy, posx, cache); ++posx;
            updateEntity<2, 1>(posy, posx, cache); ++posx;
            for(; posx<width-2; ++posx) {
                updateEntity<2, 2>(posy, posx, cache);
            }
            updateEntity<2, 3>(posy, posx, cache); ++posx;
            updateEntity<2, 4>(posy, posx, cache); // ++posx;
        }

        // height-2
        posy = height-2; posx = 0;
        updateEntity<3, 0>(posy, posx, cache); ++posx;
        updateEntity<3, 1>(posy, posx, cache); ++posx;
        for(; posx<width-2; ++posx) {
            updateEntity<3, 2>(posy, posx, cache);
        }
        updateEntity<3, 3>(posy, posx, cache); ++posx;
        updateEntity<3, 4>(posy, posx, cache); // ++posx;

        // height-1
        posy = height-1; posx = 0;
        updateEntity<4, 0>(posy, posx, cache); ++posx;
        updateEntity<4, 1>(posy, posx, cache); ++posx;
        for(; posx<width-2; ++posx) {
            updateEntity<4, 2>(posy, posx, cache);
        }
        updateEntity<4, 3>(posy, posx, cache); ++posx;
        updateEntity<4, 4>(posy, posx, cache); // ++posx;
    }
}
