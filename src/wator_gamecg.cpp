#include "wator_gamecg.hpp"
#include <random>

using namespace WaTor;
auto GameCG::GamePerThdWork::findTile(const std::array<Map::DirHelperData, 4> &dirs, 
                        Map::Entity entSearch, unsigned &resSize) const
    -> std::array<Map::DirHelperData, 4> {
    std::array<Map::DirHelperData, 4> res; // NOLINT
    resSize = 0;
    for(const auto &dir : dirs) {
        if(map->get(dir).getEntity() == entSearch) {
            res[resSize] = dir; // NOLINT
            ++resSize;
        }
    }
    return res;
}

void GameCG::GamePerThdWork::updateFish(
        const Map::Cordinate &curCord, Map::Tile &curTile, 
        const std::array<Map::DirHelperData, 4> &dirs) {
    assert(&map->get(curCord) == &curTile);
    assert(curTile.getEntity() == Map::Entity::FISH);

    bool breeding{false};

    if(curTile.getAge() >= rules.fishBreedTime) {
        breeding = true;
    } else {
        curTile.setAge(curTile.getAge()+1);
    }

    unsigned freeDirCnt;
    std::array<Map::DirHelperData, 4> freeDirs 
            {findTile(dirs, Map::Entity::WATER, freeDirCnt)};

    if(freeDirCnt == 0) {
        return;
    }

    unsigned rand = rng.get_bits(4);
    rand = rand%freeDirCnt;

    Map::DirHelperData nextDir{freeDirs[rand]}; // NOLINT

    Map::Tile &nextTile = map->get(nextDir);
    nextTile = curTile;

    if(!breeding) {
        curTile.set(Map::Entity::WATER, 0, 0);
    } else {
        curTile.setAge(0);
        nextTile.setAge(0);
    }

    if(nextDir.markNewLine == nullptr && 
            (curCord.posy < nextDir.cord.posy || curCord.posx < nextDir.cord.posx)) {
        map->getUpdateBuf(curCord)[nextDir.cord.posx] = true;
    } else if(nextDir.markNewLine != nullptr) {
        (*nextDir.markNewLine)[nextDir.cord.posx] = true;
    }
}

void GameCG::GamePerThdWork::updateShark(
        const Map::Cordinate &curCord, Map::Tile &curTile, 
        const std::array<Map::DirHelperData, 4> &dirs) {
    assert(&map->get(curCord) == &curTile);
    assert(curTile.getEntity() == Map::Entity::SHARK);

    if(curTile.getLastAte() >= rules.sharkStarveTime) {
        // the shark is DEAD, RIP :(
        curTile.set(Map::Entity::WATER, 0, 0);
        return;
    }
    curTile.setLastAte(curTile.getLastAte()+1);
   
    bool breeding{false};

    if(curTile.getAge() >= rules.sharkBreedTime) {
        breeding = true;
    } else {
        curTile.setAge(curTile.getAge()+1);
    }

    unsigned rand = rng.get_bits(4);

    bool ate{false};
    bool moved{false};
    Map::DirHelperData nextDir;

    unsigned fishDirCnt;
    std::array<Map::DirHelperData, 4> fishDir 
            {findTile(dirs, Map::Entity::FISH, fishDirCnt)};

    if(fishDirCnt > 0) {
        ate = true;
        moved = true;
        rand = rand%fishDirCnt;
        nextDir = fishDir[rand]; // NOLINT
    }

    if(!ate) {
        unsigned freeDirCnt;
        std::array<Map::DirHelperData, 4> freeDirs 
                {findTile(dirs, Map::Entity::WATER, freeDirCnt)};
        if(freeDirCnt > 0) {
            moved = true;
            rand = rand%freeDirCnt;
            nextDir = freeDirs[rand]; // NOLINT
        }
    }

    if(!moved) {
        // No free space
        return;
    }

    if(ate) {
        curTile.setLastAte(0);
    }

    Map::Tile &nextTile = map->get(nextDir);
    nextTile = curTile;

    if(!breeding) {
        curTile.set(Map::Entity::WATER, 0, 0);
    } else {
        curTile.setAge(0);
        nextTile.setAge(0);
    }

    if(nextDir.markNewLine == nullptr && 
            (curCord.posy < nextDir.cord.posy || curCord.posx < nextDir.cord.posx)) {
        map->getUpdateBuf(curCord)[nextDir.cord.posx] = true;
    } else if(nextDir.markNewLine != nullptr) {
        (*nextDir.markNewLine)[nextDir.cord.posx] = true;
    }
}

void GameCG::GamePerThdWork::updateMidLine(const Map::Cordinate &curCord) {
    if(map->getUpdateBuf(curCord)[curCord.posx]) {
        map->getUpdateBuf(curCord)[curCord.posx] = false;
        return;
    }

    Map::Tile &curTile {map->get(curCord)};
    if(curTile.getEntity() == Map::Entity::WATER) {
        return;
    }

    std::array<Map::DirHelperData, 4> dirs; // NOLINT
    for(unsigned d=0; d<4; ++d) { // NOLINT
        dirs[d] = map->dirHelper(curCord, d); // NOLINT
    }

    if(curTile.getEntity() == Map::Entity::FISH) {
        updateFish(curCord, curTile, dirs);
    } else { // if Shark
        updateShark(curCord, curTile, dirs);
    }
}

void GameCG::GamePerThdWork::updateUpLine(const Map::Cordinate &curCord) {
    assert(curCord.posy == 0);

    if(map->getUpBuf(curCord)[curCord.posx]) {
        map->getUpBuf(curCord)[curCord.posx] = false;
        return;
    }

    updateMidLine(curCord);
}

void GameCG::GamePerThdWork::updateDownLine(const Map::Cordinate &curCord) {
    assert(curCord.posy == map->getLineWidthHeight(curCord).second - 1);

    if(map->getDownBuf(curCord)[curCord.posx]) {
        map->getDownBuf(curCord)[curCord.posx] = false;
        return;
    }

    updateMidLine(curCord);
}

void GameCG::GamePerThdWork::operator() () {
    auto dim = map->getLineWidthHeight(cord);
    unsigned width{dim.first};
    unsigned height{dim.second};
    Map::Cordinate curCord = cord;

    curCord.posy = 0;
    for(unsigned j=0; j<width; ++j) {
        curCord.posx = j;
        
        updateUpLine(curCord);
    }
    
    for(unsigned i=1; i<height-1; ++i) {
        curCord.posy = i;
        for(unsigned j=0; j<width; ++j) {
            curCord.posx = j;
            
            updateMidLine(curCord);
        }
    }

    curCord.posy = height-1;
    for(unsigned j=0; j<width; ++j) {
        curCord.posx = j;
        
        updateDownLine(curCord);

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
