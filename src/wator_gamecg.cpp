#include "wator/wator_gamecg.hpp"
#include <chrono>
#include <limits>
#include <random>
#include <type_traits>

using namespace WaTor;

auto GameCG::GamePerThdWork::findTiles(const std::array<Map::Cordinate, 4> &dirs,
                        Entity entSearch, unsigned &resSize) const
    -> std::array<unsigned, 4> {
    std::array<unsigned, 4> res; // NOLINT
    resSize = 0;
    for(unsigned i=0; i<dirs.size(); ++i) {
        if(m_map->get(dirs[i]).getEntity() == entSearch) { // NOLINT
            res[resSize] = i; // NOLINT
            ++resSize;
        }
    }
    return res;
}

auto GameCG::GamePerThdWork::findTilesFish(const std::array<Map::Cordinate, 4> &dirs,
                        unsigned &resSize) const
    -> std::array<unsigned, 4> {
    return findTiles(dirs, Entity::WATER, resSize);
}

auto GameCG::GamePerThdWork::findTilesShark(const std::array<Map::Cordinate, 4> &dirs,
                        unsigned &resSize, bool &ate) const
    -> std::array<unsigned, 4> {
    std::array<unsigned, 4> res { findTiles(dirs, Entity::FISH, resSize) };
    if(resSize == 0) {
        ate = false;
        return findTiles(dirs, Entity::WATER, resSize);
    }
    ate = true;
    return res;
}

template<class T, bool midInLine>
T GameCG::GamePerThdWork::updateFish(
        const Map::Cordinate &curCord, Tile &curTile, 
        const std::array<Map::Cordinate, 4> &dirs) {
    assert(&m_map->get(curCord) == &curTile);
    assert(curTile.getEntity() == Map::Entity::FISH);

    bool breeding{false};

    if(curTile.getAge() >= m_rules.fishBreedTime) {
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

    Tile &newTile = m_map->get(newCord);
    assert(newTile.getEntity() == Map::Entity::WATER);
    newTile = curTile;

    if(!breeding) {
        curTile.set(Entity::WATER, 0, 0);
    } else {
        curTile.setAge(0);
        newTile.setAge(0);
    }

    if constexpr(midInLine) {
        if(curCord.m_posy < newCord.m_posy || curCord.m_posx < newCord.m_posx) {
            m_map->getUpdateBuf(curCord)[newCord.m_posx] = true; // newCord == curCord in this case
        }
    } else { 
        if(curCord.m_lineInd == newCord.m_lineInd &&
           curCord.m_numaInd == newCord.m_numaInd && 
           (curCord.m_posy < newCord.m_posy || curCord.m_posx < newCord.m_posx)) {
            m_map->getUpdateBuf(curCord)[newCord.m_posx] = true; // newCord == curCord in this case
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
        const Map::Cordinate &curCord, Tile &curTile, 
        const std::array<Map::Cordinate, 4> &dirs) {
    assert(&m_map->get(curCord) == &curTile);
    assert(curTile.getEntity() == Map::Entity::SHARK);

    if(curTile.getLastAte() >= m_rules.sharkStarveTime) {
        // the shark is DEAD, RIP :(
        curTile.set(Entity::WATER, 0, 0);
        if constexpr(std::is_void_v<T>) { 
            return;
        } else {
            return std::numeric_limits<unsigned>::max();
        }
    }
    curTile.setLastAte(curTile.getLastAte()+1);
   
    bool breeding{false};

    if(curTile.getAge() >= m_rules.sharkBreedTime) {
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

    Tile &newTile = m_map->get(newCord);

    if(ate) {
        curTile.setLastAte(0);
    }
    newTile = curTile;

    if(!breeding) {
        curTile.set(Entity::WATER, 0, 0);
    } else {
        curTile.setAge(0);
        newTile.setAge(0);
    }

    if constexpr(midInLine) {
        if(curCord.m_posy < newCord.m_posy || curCord.m_posx < newCord.m_posx) {
            m_map->getUpdateBuf(curCord)[newCord.m_posx] = true; // newCord == curCord in this case
        }
    } else { 
        if(curCord.m_lineInd == newCord.m_lineInd &&
           curCord.m_numaInd == newCord.m_numaInd && 
           (curCord.m_posy < newCord.m_posy || curCord.m_posx < newCord.m_posx)) {
            m_map->getUpdateBuf(curCord)[newCord.m_posx] = true; // newCord == curCord in this case
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

    assert(m_map->getLineHeight(curCord) >= 4);
    assert(vertLevel != 0 || curCord.posy == 0);
    assert(vertLevel != 1 || curCord.posy == 1);

    assert(vertLevel != 3 || curCord.posy == m_map->getLineHeight(curCord) - 2);
    assert(vertLevel != 4 || curCord.posy == m_map->getLineHeight(curCord) - 1);

    if constexpr(vertLevel == 0) {
        if(m_map->getUpBuf(curCord)[curCord.m_posx]) {
            m_map->getUpBuf(curCord)[curCord.m_posx] = false;
            return;
        }
    } else if constexpr(vertLevel == 4) {
        if(m_map->getDownBuf(curCord)[curCord.m_posx]) {
            m_map->getDownBuf(curCord)[curCord.m_posx] = false;
            return;
        }
    }

    if(m_map->getUpdateBuf(curCord)[curCord.m_posx]) {
        m_map->getUpdateBuf(curCord)[curCord.m_posx] = false;
        return;
    }

    Map::Tile &curTile {m_map->get(curCord)};
    if(curTile.getEntity() == Map::Entity::WATER) {
        return;
    }

    std::array<Map::Cordinate, 4> dirs; // NOLINT
    for(unsigned d=0; d<4; ++d) { // NOLINT
        if constexpr(vertLevel == 0 || vertLevel == 4) {
            dirs[d] = m_map->dirHelper(curCord, d); // NOLINT
        } else {
            dirs[d] = m_map->dirHelperFastPath(curCord, d); // NOLINT
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
            auto &lineBorderBuff = m_map->getDownBuf(dirs[0]);
            lineBorderBuff[dirs[0].m_posx] = true;
        }
    }

    if constexpr(vertLevel == 0 || vertLevel == 1) {
        // some sh*tty corner case:
        Map::Cordinate &newCord {dirs[dir]};
        if(newCord.m_posy == 0) {
            auto &lineBorderBuff = m_map->getUpBuf(newCord);
            lineBorderBuff[newCord.m_posx] = false;
        }
    }

    if constexpr(vertLevel == 3 || vertLevel == 4) {
        // some sh*tty corner case:
        Map::Cordinate &newCord {dirs[dir]};
        if(newCord.m_posy == m_map->getLineHeight(newCord) - 1) {
            auto &lineBorderBuff = m_map->getDownBuf(newCord);
            lineBorderBuff[newCord.m_posx] = false;
        }
    }

    if constexpr(vertLevel == 4) {
        if(dir == 2) {
            auto &lineBorderBuff = m_map->getUpBuf(dirs[2]);
            lineBorderBuff[dirs[2].m_posx] = true;
        }
    }
}

void GameCG::GamePerThdWork::operator() () {
    Map::Cordinate curCord = m_map->getCord(m_numaInd, m_line, 0, 0);
    auto dim = m_map->getLineWidthHeight(curCord);
    unsigned width{dim.first};
    unsigned height{dim.second};

    assert(height >= 4);

    curCord.m_posy = 0;
    for(unsigned j=0; j<width; ++j) {
        curCord.m_posx = j;
        updateEntity<0>(curCord);
    }

    curCord.m_posy = 1;
    for(unsigned j=0; j<width; ++j) {
        curCord.m_posx = j;
        updateEntity<1>(curCord);
    }
    
    for(unsigned i=2; i<height-2; ++i) {
        curCord.m_posy = i;
        for(unsigned j=0; j<width; ++j) {
            curCord.m_posx = j;
            updateEntity<2>(curCord);
        }
    }

    curCord.m_posy = height-2;
    for(unsigned j=0; j<width; ++j) {
        curCord.m_posx = j;
        updateEntity<3>(curCord);
    }

    curCord.m_posy = height-1;
    for(unsigned j=0; j<width; ++j) {
        curCord.m_posx = j;
        updateEntity<4>(curCord);

        assert(!m_map->getUpdateBuf(curCord)[curCord.posx]);
    }
}


GameCG::GamePerThdWork::GamePerThdWork(Map *map, unsigned numaInd, unsigned line, const Rules &rules,
                                       unsigned seed)
    : m_map(map), m_rules(rules), rng((seed == 0) ? 1337 : seed), 
      m_numaInd(numaInd), m_line(line) { }

GameCG::GameCG(const Rules &rules, const ExecutionPlanner &exp, unsigned seed) 
    : m_rules(rules), m_exp(&exp), m_map(rules, exp, seed), m_rng(seed), 
      m_waitingTime(exp.getCpuCnt(), std::chrono::microseconds{0}) {
    m_workers = std::make_unique<std::optional<Worker<GamePerThdWork>>[]>(exp.getCpuCnt()-1);

    unsigned cpuInd=0;
    for(unsigned numaNode : exp.getNumaList()) {
        for(unsigned cpu : exp.getCpuListPerNuma(numaNode)) {
            if(cpuInd > 0) {
                m_workers[cpuInd-1].emplace(cpu);
            } else {
                m_worker0Cpu = cpu;
            }
            ++cpuInd;
        }
    }
}

void GameCG::calcStats() {
    // Important:
    m_worker0AllDuration += m_worker0LastDuration;
    m_worker0SumFreq += m_worker0LastFreq*static_cast<std::uint64_t>(m_worker0LastDuration.count());
    ++m_halfIterCnt;


    std::chrono::microseconds maxTime = getWorker0LastRunDuration();
    std::chrono::microseconds minTime = getWorker0LastRunDuration();
    for(unsigned i=1; i<m_exp->getCpuCnt(); ++i) {
        maxTime = std::max(maxTime, m_workers[i-1]->getLastRunDuration()); // NOLINT
        minTime = std::min(minTime, m_workers[i-1]->getLastRunDuration()); // NOLINT
    }

    m_waitingTime[0] += maxTime - getWorker0LastRunDuration();
    for(unsigned i=1; i<m_exp->getCpuCnt(); ++i) {
        m_waitingTime[i] += maxTime - m_workers[i-1]->getLastRunDuration();
    }
    m_lastWatingDelta = maxTime - minTime;
    m_maxWatingDelta = std::max(m_maxWatingDelta, m_lastWatingDelta);
    m_sumWatingDelta += m_lastWatingDelta;
}

void GameCG::doHalfIteration(bool odd) {
    unsigned cpuInd = 1;
    for(unsigned i=0; i<m_exp->getNumaList().size(); ++i) {
        unsigned numaNode = m_exp->getNumaList()[i];
        for(unsigned j=0; j<m_exp->getCpuListPerNuma(numaNode).size(); ++j) {
            if(i == 0 && j == 0) {
                continue;
            }
        
            m_workers[cpuInd-1]->pushWork(GamePerThdWork{&m_map, i, 2*j+static_cast<unsigned>(odd), 
                                        m_rules, static_cast<unsigned int>(m_rng())});
            ++cpuInd;
        }
    }

    auto clockStart = std::chrono::steady_clock::now();
    GamePerThdWork work1{&m_map, 0, static_cast<unsigned>(odd), m_rules, static_cast<unsigned int>(m_rng())};
    work1();
    auto clockEnd = std::chrono::steady_clock::now();

    m_worker0LastFreq = Utils::readCurCpuFreq(m_worker0Cpu);
    m_worker0LastDuration = std::chrono::duration_cast<std::chrono::microseconds>(clockEnd - clockStart);

    cpuInd = 1;
    for(unsigned i=0; i<m_exp->getNumaList().size(); ++i) {
        unsigned numaNode = m_exp->getNumaList()[i];
        for(unsigned j=0; j<m_exp->getCpuListPerNuma(numaNode).size(); ++j) {
            if(i == 0 && j == 0) {
                continue;
            }
        
            m_workers[cpuInd-1]->waitFinish();
            ++cpuInd;
        }
    }

    calcStats();
}

void GameCG::doIteration() {
    auto clockStart = std::chrono::steady_clock::now();
    doHalfIteration(false);
    doHalfIteration(true);
    auto clockEnd = std::chrono::steady_clock::now();
    std::chrono::microseconds diff = std::chrono::duration_cast<std::chrono::microseconds>(clockEnd - clockStart);
    m_allTime += diff;
}

[[nodiscard]] std::vector<std::uint64_t> GameCG::getAvgFreqPerWorker() const {
    std::vector<std::uint64_t> res(m_exp->getCpuCnt());

    res[0] = getWorker0AvgFreq();
    for(unsigned i=1; i<m_exp->getCpuCnt(); ++i) {
        res[i] = m_workers[i-1]->getAvgFreq();  // NOLINT
    }

    return res;
}

[[nodiscard]] std::vector<std::chrono::microseconds> GameCG::getAllRunDurationPerWorker() const {
    std::vector<std::chrono::microseconds> res(m_exp->getCpuCnt());

    res[0] = getWorker0AllRunDuration();
    for(unsigned i=1; i<m_exp->getCpuCnt(); ++i) {
        res[i] = m_workers[i-1]->getAllRunDuration();  // NOLINT
    }

    return res;
}
