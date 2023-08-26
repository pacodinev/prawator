#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <cassert>
#include <functional>
#include <limits>
#include <memory>
#include <memory_resource>
#include <optional>
#include <ostream>
#include <random>
#include <vector>

#include <iostream>

#include "map_line.hpp"
#include "map_numa.hpp"

#include "execution_planner.hpp"
#include "numa_allocator.hpp"
#include "posixFostream.hpp"
#include "rules.hpp"
#include "pmr_deleter.hpp"

#include <config.h>

namespace WaTor {

class MapAllocStrategy {
public:
    virtual ~MapAllocStrategy() noexcept = default;

    virtual auto operator() (const ExecutionPlanner &exp) 
        -> std::unique_ptr<std::pmr::memory_resource*[]> = 0;
};

// NON NUMA Aware allocation strategy for WaTor::Map
// used mainly for testing
class MockAllocStrategy : public MapAllocStrategy {
private:

public:

    auto operator() (const ExecutionPlanner &exp) 
        -> std::unique_ptr<std::pmr::memory_resource*[]> override { // NOLINT
        auto res = std::make_unique<std::pmr::memory_resource*[]>
                            (exp.getNumaList().size());
        std::pmr::memory_resource *mmr = std::pmr::get_default_resource();
        for(unsigned i=0; i<exp.getNumaList().size(); ++i) {
            res[i] = mmr;
        }

        return res;
    }
};

#ifdef WATOR_NUMA
// NUMA aware allocation strategy for WaTor::Map

class NumaAllocStrategy : public MapAllocStrategy {
private:
    struct NumaAllocEnt {
        NumaAllocator numaAlloc;
        std::pmr::monotonic_buffer_resource alloc;
        explicit NumaAllocEnt(unsigned numaNode) 
            : numaAlloc(numaNode), alloc(&numaAlloc) { }
    };

    std::vector<std::unique_ptr<NumaAllocEnt>> alloc;
        
public:

    NumaAllocStrategy() = default;
    NumaAllocStrategy(const NumaAllocStrategy&) = delete; 
    NumaAllocStrategy& operator=(const NumaAllocStrategy&) = delete; 
    NumaAllocStrategy(NumaAllocStrategy&&) = default; 
    NumaAllocStrategy& operator=(NumaAllocStrategy&&) = default; 
    ~NumaAllocStrategy() noexcept override = default;

    auto operator() (const ExecutionPlanner &exp) 
        -> std::unique_ptr<std::pmr::memory_resource*[]> override { // NOLINT
        auto res = std::make_unique<std::pmr::memory_resource*[]>
                            (exp.getNumaList().size());

        alloc.reserve(exp.getNumaList().size());

        for(unsigned numaInd=0; numaInd<exp.getNumaList().size(); ++numaInd) {
            unsigned numaNode = exp.getNumaList()[numaInd];
            auto ent = std::make_unique<NumaAllocEnt>(numaNode);
            alloc.emplace_back(std::move(ent));
            res[numaInd] = &alloc.back().get()->alloc;
        }

        return res;
    }
};

#else
// NUMA aware allocation strategy for WaTor::Map
// WARNING: NO NUMA support compiled, using NON Numa alloc strategy
using NumaAllocStrategy = BasicAllocStrategy;

#endif

class Map
{

private:

    unsigned m_width, m_height;
    unsigned m_numaCount;
    std::unique_ptr<MapAllocStrategy> m_numaAlloc;

    std::unique_ptr<std::unique_ptr<MapNuma, PmrDelete<MapNuma>>[]> m_numaMap; // NOLINT

    void generateNuma(unsigned width, unsigned heightPerCpu, unsigned &heightRem,
                      const ExecutionPlanner &exp, unsigned numaInd, std::pmr::memory_resource *pmr) {
        unsigned curNumaCpuCnt = static_cast<unsigned>(exp.getCpuListPerNuma(numaInd).size());
        unsigned newHeight = 2*heightPerCpu*curNumaCpuCnt;
        newHeight += std::min(2*curNumaCpuCnt, heightRem);
        heightRem -= std::min(2*curNumaCpuCnt, heightRem);

        // rules.width, newHeight, numaNode, exp
        // perNuma[numaNode] = std::make_unique<PerNumaData, AllocatorDeleter>(rules.width, newHeight, numaNode, exp);
        std::pmr::polymorphic_allocator<MapNuma> alloc{pmr};
        MapNuma *ptr = alloc.allocate(1);

        try {
            alloc.construct(ptr, newHeight, width, numaInd, exp, pmr);
        } catch (...) {
            alloc.deallocate(ptr, 1);
            throw;
        }

        m_numaMap[numaInd] = {ptr, PmrDelete<MapNuma>{pmr}};
    }

public:

    // AllocStrategy - a class generating memory_resources 
    // based on NUMA node
    // NOTE: If the system is not NUMA (ExecutionPlanner::isNuma()), 
    // AllocStrategy is not used!
    Map(unsigned height, unsigned width, const ExecutionPlanner &exp, 
        std::unique_ptr<MapAllocStrategy> &&numaAlloc = std::make_unique<NumaAllocStrategy>()) 
        : m_width(width), m_height(height), 
          m_numaCount(static_cast<unsigned>(exp.getNumaList().size())),
          m_numaAlloc(std::move(numaAlloc)),
          m_numaMap(std::make_unique<std::unique_ptr<MapNuma, PmrDelete<MapNuma>>[]>(m_numaCount)) // NOLINT
          {

        if(height < static_cast<std::size_t>(2)*2*exp.getCpuCnt()) { // TODO: this has to be 4
           throw std::runtime_error("Height is too small or CPU count is too large!");
        }

        // TODO: granularity
        unsigned heightPerCpu = height / (2*exp.getCpuCnt());
        unsigned heightRem = height - 2*exp.getCpuCnt()*heightPerCpu;

        if(exp.isNuma()) {
            std::unique_ptr<std::pmr::memory_resource*[]> numaMem{(*m_numaAlloc)(exp)};

            for(unsigned numaInd=0; numaInd<m_numaCount; ++numaInd) {
                generateNuma(width, heightPerCpu, heightRem, exp, numaInd, numaMem[numaInd]);
            }
        } else {
            generateNuma(width, heightPerCpu, heightRem, exp, 0, std::pmr::get_default_resource());
        }
    }

    Map(const Rules &rules, const ExecutionPlanner &exp) 
        : Map(rules.getHeight(), rules.getWidth(), exp) {}

    [[nodiscard]] unsigned getHeight() const noexcept { return m_height; }
    [[nodiscard]] unsigned getWidth() const noexcept { return m_width; }
    [[nodiscard]] unsigned getMapNumaCnt() const noexcept { return m_numaCount; }
    [[nodiscard]] MapNuma& getMapNuma(unsigned numa) noexcept { 
        assert(numa < getMapNumaCnt());
        return *m_numaMap[numa]; 
    }
    [[nodiscard]] const MapNuma& getMapNuma(unsigned numa) const noexcept { 
        assert(numa < getMapNumaCnt());
        return *m_numaMap[numa]; 
    }

    Tile& get(unsigned numaInd, unsigned lineInNuma, unsigned posy, unsigned posx) noexcept {
        return getMapNuma(numaInd).getLine(lineInNuma).get(posy, posx);
    }

    [[nodiscard]] const Tile& get(unsigned numaInd, unsigned lineInNuma, unsigned posy, unsigned posx) const noexcept {
        return getMapNuma(numaInd).getLine(lineInNuma).get(posy, posx);
    }

    [[nodiscard]] auto getUpdateMask(unsigned numaInd, unsigned lineInNuma, unsigned posx) { // NOLINT
        return getMapNuma(numaInd).getLine(lineInNuma).getUpdateMask(posx);
    }

    [[nodiscard]] bool getUpdateMask(unsigned numaInd, unsigned lineInNuma, unsigned posx) const { // NOLINT
        return getMapNuma(numaInd).getLine(lineInNuma).getUpdateMask(posx);
    }

    [[nodiscard]] auto getTopMask(unsigned numaInd, unsigned lineInd, unsigned posx) noexcept {
        MapLine &line = getMapNuma(numaInd).getLine(lineInd);
        return line.getTopMask(posx);
    }

    [[nodiscard]] bool getTopMask(unsigned numaInd, unsigned lineInd, unsigned posx) const noexcept {
        const MapLine &line = getMapNuma(numaInd).getLine(lineInd);
        return line.getTopMask(posx);
    }

    [[nodiscard]] auto getBottomMask(unsigned numaInd, unsigned lineInd, unsigned posx) noexcept {
        MapLine &line = getMapNuma(numaInd).getLine(lineInd);
        return line.getBottomMask(posx);
    }

    [[nodiscard]] bool getBottomMask(unsigned numaInd, unsigned lineInd, unsigned posx) const noexcept {
        const MapLine &line = getMapNuma(numaInd).getLine(lineInd);
        return line.getBottomMask(posx);
    }

    [[nodiscard]] unsigned getMapLineHeight(unsigned numaInd, unsigned lineInd) const noexcept {
        return getMapNuma(numaInd).getLine(lineInd).getHeight();
    }

    [[nodiscard]] unsigned getMapLineWidth(unsigned numaInd, unsigned lineInd) const noexcept {
        return getMapNuma(numaInd).getLine(lineInd).getWidth();
    }

    struct Cordinate { // NOLINT
    private:
        // used as a cache:
        // MapNuma *numaData; // numaData == perNuma[numaInd].get()
        // we use iterators and as needed cast them to const
        MapLine::TileIter m_curTileIter;
        MapLine::UpdateMaskIter m_curUpdateMaskIter;

        unsigned m_numaInd;
        unsigned m_lineInd;
        unsigned m_posy, m_posx;

        Cordinate(MapLine::TileIter curTileIter, MapLine::UpdateMaskIter curUpdateMaskIter, 
                  unsigned numaInd, unsigned lineInNuma, unsigned posy, unsigned posx) 
            : m_curTileIter(curTileIter), m_curUpdateMaskIter(curUpdateMaskIter), 
              m_numaInd(numaInd), m_lineInd(lineInNuma), m_posy(posy), m_posx(posx) { }

    public:
        // default constructor, constructed object is invalid
        Cordinate() = default;

        [[nodiscard]] unsigned numaInd() const noexcept { return m_numaInd; }
        [[nodiscard]] unsigned lineInd() const noexcept { return m_lineInd; }
        [[nodiscard]] unsigned posy() const noexcept { return m_posy; }
        [[nodiscard]] unsigned posx() const noexcept { return m_posx; }

        friend class Map;
    };

    [[nodiscard]] Cordinate makeCordinate(unsigned numaInd, unsigned lineInd, unsigned posy, unsigned posx) const {
        MapLine &line = const_cast<MapLine&>(getMapNuma(numaInd).getLine(lineInd)); // NOLINT
        MapLine::TileIter tit = line.getTileIter(posy, posx);
        MapLine::UpdateMaskIter umit = line.getUpdateMaskIter(posx);

        return {tit, umit, numaInd, lineInd, posy, posx};
    }

    [[nodiscard]] Tile& get(const Cordinate& cord) noexcept { // NOLINT
        assert(&get(cord.numaInd(), cord.lineInd(), cord.posy(), cord.posx()) == &(*cord.m_curTileIter));
        return *cord.m_curTileIter;
    }

    [[nodiscard]] const Tile& get(const Cordinate& cord) const noexcept { // NOLINT
        assert(&get(cord.numaInd(), cord.lineInd(), cord.posy(), cord.posx()) == &(*cord.m_curTileIter));
        return *cord.m_curTileIter;
    }

    [[nodiscard]] auto getUpdateMask(const Cordinate& cord) noexcept { // NOLINT
        assert(*cord.m_curUpdateMaskIter == getUpdateMask(cord.numaInd(), cord.lineInd(), cord.posx()));
        return *cord.m_curUpdateMaskIter;
    }

    [[nodiscard]] bool getUpdateMask(const Cordinate& cord) const noexcept { // NOLINT
        assert(*cord.m_curUpdateMaskIter == getUpdateMask(cord.numaInd(), cord.lineInd(), cord.posx()));
        return *cord.m_curUpdateMaskIter;
    }

    [[nodiscard]] auto getTopMask(const Cordinate& cord) noexcept {
        MapLine &line = getMapNuma(cord.numaInd()).getLine(cord.lineInd());
        return line.getTopMask(cord.posx());
    }

    [[nodiscard]] bool getTopMask(const Cordinate& cord) const noexcept {
        const MapLine &line = getMapNuma(cord.numaInd()).getLine(cord.lineInd());
        return line.getTopMask(cord.posx());
    }

    [[nodiscard]] auto getBottomMask(const Cordinate& cord) noexcept {
        MapLine &line = getMapNuma(cord.numaInd()).getLine(cord.lineInd());
        return line.getBottomMask(cord.posx());
    }

    [[nodiscard]] bool getBottomMask(const Cordinate& cord) const noexcept {
        const MapLine &line = getMapNuma(cord.numaInd()).getLine(cord.lineInd());
        return line.getBottomMask(cord.posx());
    }

    [[nodiscard]] Cordinate dirHelper(Cordinate cord, unsigned dir) const {
        assert(dir < 4);

        // we would compare against umax as a -1, and yea it is ugly
        constexpr unsigned umax = std::numeric_limits<unsigned>::max();

        constexpr std::array<unsigned, 4> diry = {umax, 0, 1, 0};  // -1 is umax
        constexpr std::array<unsigned, 4> dirx = {0, 1, 0, umax};

        unsigned newPosy = cord.posy() + diry[dir]; // NOLINT
        unsigned newPosx = cord.posx() + dirx[dir]; // NOLINT

        const MapLine &curLine = getMapNuma(cord.numaInd()).getLine(cord.lineInd());

        if(newPosx == umax) {
            newPosx = curLine.getWidth()-1;
        } else if(newPosx >= curLine.getWidth()) {
            newPosx = 0;
        }

        unsigned newNumaInd = cord.numaInd();
        unsigned newLineInd = cord.lineInd();
        const MapNuma *newNuma = &getMapNuma(newNumaInd);
        const MapLine *newLine = &newNuma->getLine(newLineInd);

        if(newPosy == umax) // < 0
        {
            --newLineInd;
            if(newLineInd == umax) {
                --newNumaInd;
                if(newNumaInd == umax) {
                    newNumaInd = getMapNumaCnt()-1; 
                }
                newNuma = &getMapNuma(newNumaInd);
                newLineInd = newNuma->getLineCnt()-1;
            }
            newLine = &newNuma->getLine(newLineInd);
            newPosy = newLine->getHeight()-1;
        } else if(newPosy >= newLine->getHeight()) {
            ++newLineInd;
            if(newLineInd >= newNuma->getLineCnt()) {
                ++newNumaInd;
                if(newNumaInd >= getMapNumaCnt()) {
                    newNumaInd = 0;
                }
                newNuma = &getMapNuma(newNumaInd);
                newLineInd = 0;
            }
            newLine = &newNuma->getLine(newLineInd);
            newPosy = 0;
        }

        MapLine &ncLine = const_cast<MapLine&>(*newLine); // NOLINT

        MapLine::TileIter tit = ncLine.getTileIter(newPosy, newPosx);
        MapLine::UpdateMaskIter umit = ncLine.getUpdateMaskIter(newPosx);

        return {tit, umit, newNumaInd, newLineInd, newPosy, newPosx};
    }

    // TODO: document
    void dirRightFast(Cordinate& cord) const noexcept { // NOLINT
        // TODO: assert
        assert(cord.posx() + 1 < getMapLineWidth(cord.numaInd(), cord.lineInd()));
        ++cord.m_posx;
        ++cord.m_curTileIter;
        ++cord.m_curUpdateMaskIter;
    }

    MapLine::TopMaskIter getTopMaskIter(unsigned numaInd, unsigned lineInd, unsigned posx) {
        return getMapNuma(numaInd).getLine(lineInd).getTopMaskIter(posx);
    }

    MapLine::TopMaskIter getTopMaskIter(const Cordinate& cord) {
        return getMapNuma(cord.numaInd()).getLine(cord.lineInd()).getTopMaskIter(cord.posx());
    }

    MapLine::BottomMaskIter getBottomMaskIter(unsigned numaInd, unsigned lineInd, unsigned posx) {
        return getMapNuma(numaInd).getLine(lineInd).getBottomMaskIter(posx);
    }

    MapLine::TopMaskIter getBottomMaskIter(const Cordinate& cord) {
        return getMapNuma(cord.numaInd()).getLine(cord.lineInd()).getBottomMaskIter(cord.posx());
    }

    // TODO: move out of this class
    // saveMap*, randomize

    void saveMap(std::ostream &fout, bool includeHeader = false) const;

#ifdef __unix__
    void saveMap(PosixFostream &fout, bool includeHeader = false) const;
#endif

    void randomize(std::size_t fishCnt, std::size_t sharkCnt, unsigned seed);

    void randomize(const Rules &rules, unsigned seed) {
        randomize(rules.getInitialFishCnt(), rules.getInitialSharkCnt(), seed);
    }
};

}
