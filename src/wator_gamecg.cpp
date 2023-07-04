#include "wator_gamecg.hpp"
#include <limits>
#include <random>
#include <type_traits>

using namespace WaTor;

auto GameCG::GamePerThdWork::findTiles(const std::array<Map::Cordinate, 4> &dirs,
                        Map::Entity entSearch, unsigned &resSize) const
    -> std::array<unsigned, 4> {
    std::array<unsigned, 4> res; // NOLINT
    resSize = 0;
    for(unsigned i=0; i<dirs.size(); ++i) {
        if(map->get(dirs[i]).getEntity() == entSearch) { // NOLINT
            res[resSize] = i; // NOLINT
            ++resSize;
        }
    }
    return res;
}

auto GameCG::GamePerThdWork::findTilesFish(const std::array<Map::Cordinate, 4> &dirs,
                        unsigned &resSize) const
    -> std::array<unsigned, 4> {
    return findTiles(dirs, Map::Entity::WATER, resSize);
}

auto GameCG::GamePerThdWork::findTilesShark(const std::array<Map::Cordinate, 4> &dirs,
                        unsigned &resSize, bool &ate) const
    -> std::array<unsigned, 4> {
    std::array<unsigned, 4> res { findTiles(dirs, Map::Entity::FISH, resSize) };
    if(resSize == 0) {
        ate = false;
        return findTiles(dirs, Map::Entity::WATER, resSize);
    }
    ate = true;
    return res;
}

template<class T, bool midInLine>
T GameCG::GamePerThdWork::updateFish(
        const Map::Cordinate &curCord, Map::Tile &curTile, 
        const std::array<Map::Cordinate, 4> &dirs) {
    assert(&map->get(curCord) == &curTile);
    assert(curTile.getEntity() == Map::Entity::FISH);

    bool breeding{false};

    if(curTile.getAge() >= rules.fishBreedTime) {
        breeding = true;
    } else {
        curTile.setAge(curTile.getAge()+1);
    }

    unsigned freeDirCnt;
    std::array<unsigned, 4> freeDirs
            {findTilesFish(dirs, freeDirCnt)};

    if(freeDirCnt == 0) {
        if constexpr(std::is_void_v<T>) { 
            return;
        } else {
            return std::numeric_limits<unsigned>::max();
        }
    }

    unsigned nextDir = freeDirs[rng.get_bits(4)%freeDirCnt]; // NOLINT

    Map::Cordinate newCord{dirs[nextDir]}; // NOLINT

    Map::Tile &newTile = map->get(newCord);
    assert(newTile.getEntity() == Map::Entity::WATER);
    newTile = curTile;

    if(!breeding) {
        curTile.set(Map::Entity::WATER, 0, 0);
    } else {
        curTile.setAge(0);
        newTile.setAge(0);
    }

    if constexpr(midInLine) {
        if(curCord.posy < newCord.posy || curCord.posx < newCord.posx) {
            map->getUpdateBuf(curCord)[newCord.posx] = true; // newCord == curCord in this case
        }
    } else { 
        if(curCord.lineInNuma == newCord.lineInNuma &&
           curCord.numaInd == newCord.numaInd && 
           (curCord.posy < newCord.posy || curCord.posx < newCord.posx)) {
            map->getUpdateBuf(curCord)[newCord.posx] = true; // newCord == curCord in this case
        }
    }

    if constexpr(std::is_void_v<T>) { 
        return;
    } else {
        return nextDir;
    }

}

template<class T, bool midInLine>
T GameCG::GamePerThdWork::updateShark(
        const Map::Cordinate &curCord, Map::Tile &curTile, 
        const std::array<Map::Cordinate, 4> &dirs) {
    assert(&map->get(curCord) == &curTile);
    assert(curTile.getEntity() == Map::Entity::SHARK);

    if(curTile.getLastAte() >= rules.sharkStarveTime) {
        // the shark is DEAD, RIP :(
        curTile.set(Map::Entity::WATER, 0, 0);
        if constexpr(std::is_void_v<T>) { 
            return;
        } else {
            return std::numeric_limits<unsigned>::max();
        }
    }
    curTile.setLastAte(curTile.getLastAte()+1);
   
    bool breeding{false};

    if(curTile.getAge() >= rules.sharkBreedTime) {
        breeding = true;
    } else {
        curTile.setAge(curTile.getAge()+1);
    }

    unsigned nextDirsCnt;
    bool ate;
    std::array<unsigned, 4> nextDirs
            {findTilesShark(dirs, nextDirsCnt, ate)};

    if(nextDirsCnt == 0) {
        // No free space
        if constexpr(std::is_void_v<T>) { 
            return;
        } else {
            return std::numeric_limits<unsigned>::max();
        }
    }

    unsigned nextDir = nextDirs[rng.get_bits(4)%nextDirsCnt]; // NOLINT

    Map::Cordinate newCord{dirs[nextDir]}; // NOLINT

    Map::Tile &newTile = map->get(newCord);

    if(ate) {
        curTile.setLastAte(0);
    }
    newTile = curTile;

    if(!breeding) {
        curTile.set(Map::Entity::WATER, 0, 0);
    } else {
        curTile.setAge(0);
        newTile.setAge(0);
    }

    if constexpr(midInLine) {
        if(curCord.posy < newCord.posy || curCord.posx < newCord.posx) {
            map->getUpdateBuf(curCord)[newCord.posx] = true; // newCord == curCord in this case
        }
    } else { 
        if(curCord.lineInNuma == newCord.lineInNuma &&
           curCord.numaInd == newCord.numaInd && 
           (curCord.posy < newCord.posy || curCord.posx < newCord.posx)) {
            map->getUpdateBuf(curCord)[newCord.posx] = true; // newCord == curCord in this case
        }
    }

    if constexpr(std::is_void_v<T>) { 
        return;
    } else {
        return nextDir;
    }

}

//0 - y=0
//1 - y=1
//2 - mid
//3 - y=height-2
//4 - y=height-1
template<unsigned vertLevel>
void GameCG::GamePerThdWork::updateEntity(const Map::Cordinate &curCord) {
    static_assert(0 <= vertLevel && vertLevel <5, "Invalid vertLevel argument");

    assert(map->getLineHeight(curCord) >= 4);
    assert(vertLevel != 0 || curCord.posy == 0);
    assert(vertLevel != 1 || curCord.posy == 1);

    assert(vertLevel != 3 || curCord.posy == map->getLineHeight(curCord) - 2);
    assert(vertLevel != 4 || curCord.posy == map->getLineHeight(curCord) - 1);

    if constexpr(vertLevel == 0) {
        if(map->getUpBuf(curCord)[curCord.posx]) {
            map->getUpBuf(curCord)[curCord.posx] = false;
            return;
        }
    } else if constexpr(vertLevel == 4) {
        if(map->getDownBuf(curCord)[curCord.posx]) {
            map->getDownBuf(curCord)[curCord.posx] = false;
            return;
        }
    }

    if(map->getUpdateBuf(curCord)[curCord.posx]) {
        map->getUpdateBuf(curCord)[curCord.posx] = false;
        return;
    }

    Map::Tile &curTile {map->get(curCord)};
    if(curTile.getEntity() == Map::Entity::WATER) {
        return;
    }

    std::array<Map::Cordinate, 4> dirs; // NOLINT
    for(unsigned d=0; d<4; ++d) { // NOLINT
        if constexpr(vertLevel == 0 || vertLevel == 4) {
            dirs[d] = map->dirHelper(curCord, d); // NOLINT
        } else {
            dirs[d] = map->dirHelperFastPath(curCord, d); // NOLINT
        }
    }

    unsigned dir;
    if constexpr(vertLevel == 2) {
        if(curTile.getEntity() == Map::Entity::FISH) {
            updateFish<void, true>(curCord, curTile, dirs);
        } else { // if Shark
            updateShark<void, true>(curCord, curTile, dirs);
        }
        return;
    } else {
        if(curTile.getEntity() == Map::Entity::FISH) {
            dir = updateFish<unsigned, true>(curCord, curTile, dirs);
        } else { // if Shark
            dir = updateShark<unsigned, true>(curCord, curTile, dirs);
        }
        if(dir == std::numeric_limits<unsigned>::max()) {
            return;
        }
    }

    if constexpr(vertLevel == 0) {
        if(dir == 0) {
            auto &lineBorderBuff = map->getDownBuf(dirs[0]);
            lineBorderBuff[dirs[0].posx] = true;
        }
    }

    if constexpr(vertLevel == 0 || vertLevel == 1) {
        // some sh*tty corner case:
        Map::Cordinate &newCord {dirs[dir]};
        if(newCord.posy == 0) {
            auto &lineBorderBuff = map->getUpBuf(newCord);
            lineBorderBuff[newCord.posx] = false;
        }
    }

    if constexpr(vertLevel == 3 || vertLevel == 4) {
        // some sh*tty corner case:
        Map::Cordinate &newCord {dirs[dir]};
        if(newCord.posy == map->getLineHeight(newCord) - 1) {
            auto &lineBorderBuff = map->getDownBuf(newCord);
            lineBorderBuff[newCord.posx] = false;
        }
    }

    if constexpr(vertLevel == 4) {
        if(dir == 2) {
            auto &lineBorderBuff = map->getUpBuf(dirs[2]);
            lineBorderBuff[dirs[2].posx] = true;
        }
    }
}

void GameCG::GamePerThdWork::operator() () {
    auto dim = map->getLineWidthHeight(cord);
    unsigned width{dim.first};
    unsigned height{dim.second};
    Map::Cordinate curCord = cord;

    assert(height >= 4);

    curCord.posy = 0;
    for(unsigned j=0; j<width; ++j) {
        curCord.posx = j;
        updateEntity<0>(curCord);
    }

    curCord.posy = 1;
    for(unsigned j=0; j<width; ++j) {
        curCord.posx = j;
        updateEntity<1>(curCord);
    }
    
    for(unsigned i=2; i<height-2; ++i) {
        curCord.posy = i;
        for(unsigned j=0; j<width; ++j) {
            curCord.posx = j;
            updateEntity<2>(curCord);
        }
    }

    curCord.posy = height-2;
    for(unsigned j=0; j<width; ++j) {
        curCord.posx = j;
        updateEntity<3>(curCord);
    }

    curCord.posy = height-1;
    for(unsigned j=0; j<width; ++j) {
        curCord.posx = j;
        updateEntity<4>(curCord);

        assert(!map->getUpdateBuf(curCord)[curCord.posx]);
    }
}


GameCG::GamePerThdWork::GamePerThdWork(Map *_map, unsigned _numaInd, unsigned _line, const Rules &_rules,
                                       unsigned seed)
    : map(_map), rules(_rules), cord(map->getCord(_numaInd, _line, 0, 0)), 
      rng((seed == 0) ? 1337 : seed) {
}

GameCG::GameCG(const Rules &rules, const ExecutionPlanner &exp, unsigned seed) 
    :m_rules(rules), m_exp(&exp), m_map(rules, exp, seed), m_rng(seed) {
    m_workers = std::make_unique<std::optional<Worker<GamePerThdWork>>[]>(exp.getCpuCnt());

    if(m_exp->isNuma()) {
        unsigned cpuInd=0;
        for(unsigned numaNode : exp.getNumaList()) {
            for(unsigned cpu : exp.getCpuListPerNuma(numaNode)) {
                if(cpuInd > 0)
                    m_workers[cpuInd].emplace(cpu);
                ++cpuInd;
            }
        }
    } else {
        unsigned cpuInd=0;
        for(unsigned cpu : exp.getCpuListPerNuma(0)) {
            if(cpuInd > 0)
                m_workers[cpuInd].emplace(cpu);
            ++cpuInd;
        }
    }
}

void GameCG::doHalfIterationNuma(bool odd) {
    unsigned cpuInd = 1;
    for(unsigned i=0; i<m_exp->getNumaList().size(); ++i) {
        unsigned numaNode = m_exp->getNumaList()[i];
        for(unsigned j=0; j<m_exp->getCpuListPerNuma(numaNode).size(); ++j) {
            if(i == 0 && j == 0) {
                continue;
            }
        
            m_workers[cpuInd]->pushWork(GamePerThdWork{&m_map, i, 2*j+static_cast<unsigned>(odd), 
                                        m_rules, static_cast<unsigned int>(m_rng())});
            ++cpuInd;
        }
    }

    GamePerThdWork work1{&m_map, 0, static_cast<unsigned>(odd), m_rules, static_cast<unsigned int>(m_rng())};
    work1();

    cpuInd = 1;
    for(unsigned i=0; i<m_exp->getNumaList().size(); ++i) {
        unsigned numaNode = m_exp->getNumaList()[i];
        for(unsigned j=0; j<m_exp->getCpuListPerNuma(numaNode).size(); ++j) {
            if(i == 0 && j == 0) {
                continue;
            }
        
            m_workers[cpuInd]->waitFinish();
            ++cpuInd;
        }
    }
}

void GameCG::doHalfIterationNoNuma(bool odd) {
    unsigned cpuInd = 1;
    unsigned numaNode = 0;
    for(unsigned j=0; j<m_exp->getCpuListPerNuma(numaNode).size(); ++j) {
        if(j == 0) {
            continue;
        }
    
        m_workers[cpuInd]->pushWork(GamePerThdWork{&m_map, 0, 2*j+static_cast<unsigned>(odd), 
                                    m_rules, static_cast<unsigned int>(m_rng())});
        ++cpuInd;
    }

    GamePerThdWork work1{&m_map, 0, static_cast<unsigned>(odd), m_rules, static_cast<unsigned int>(m_rng())};
    work1();

    cpuInd = 1;
    for(unsigned j=0; j<m_exp->getCpuListPerNuma(numaNode).size(); ++j) {
        if(j == 0) {
            continue;
        }
    
        m_workers[cpuInd]->waitFinish();
        ++cpuInd;
    }
}

void GameCG::doIteration() {
    if(m_exp->isNuma()) {
        doHalfIterationNuma(false);
        doHalfIterationNuma(true);
    } else { 
        doHalfIterationNoNuma(false);
        doHalfIterationNoNuma(true);
    }
}
