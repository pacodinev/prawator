#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <cassert>
#include <functional>
#include <memory>
#include <memory_resource>
#include <optional>
#include <ostream>
#include <random>
#include <vector>

#include <iostream>

#include "execution_planner.hpp"
#include "numa_allocator.hpp"
#include "wator_rules.hpp"

#include <config.h>

namespace WaTor {

class Map
{
    public:

    enum class Entity {
        WATER = 0,
        FISH,
        SHARK
    };

    class Tile {
    private:
        std::uint8_t m_lastAte:4,  m_age:4;

    public:

        static constexpr unsigned MAX_AGE = 14;
        static constexpr unsigned MAX_LAST_ATE = 14;

        void set(Entity ent, unsigned age, unsigned lastAte) {
            assert(age <= MAX_AGE && lastAte <= MAX_LAST_ATE);
            switch (ent) {
                case Entity::WATER:
                    assert(age == 0 && lastAte == 0);
                    m_lastAte = 0; m_age = 0;
                    break;
                case Entity::FISH:
                    assert(lastAte == 0 && age <= 14);
                    m_lastAte = 0; m_age = static_cast<std::uint8_t>(age+1);
                    break;
                case Entity::SHARK:
                    assert(lastAte <= 14 && age <= 14);
                    m_lastAte = static_cast<std::uint8_t>(lastAte+1); 
                    m_age = static_cast<std::uint8_t>(age+1);
                    break;
            }
        }


        Tile(Entity ent, unsigned age, unsigned lastAte) { // NOLINT
            set(ent, age, lastAte);
        }

        Tile() : Tile(Entity::WATER, 0, 0) {}

        [[nodiscard]] Entity getEntity() const {
            if(m_age == 0 && m_lastAte == 0) { return Entity::WATER; }
            if(m_lastAte == 0) { return Entity::FISH; }
            return Entity::SHARK;
        }

        [[nodiscard]] unsigned getAge() const { assert(m_age>0); return m_age-1; }
        [[nodiscard]] unsigned getLastAte() const { assert(m_lastAte>0); return m_lastAte-1; }

        void setAge(unsigned age) {
            assert(m_age>0 && age <= 14); 
            m_age = static_cast<std::uint8_t>(age+1);
        }
        void setLastAte(unsigned lastAte) { 
            assert(m_lastAte>0 && lastAte <= 14); 
            m_lastAte = static_cast<std::uint8_t>(lastAte+1); 
        }
    };
    static_assert(sizeof(Tile) == 1, "Tile is large .. just large");

private:

    struct PerNumaData;

    struct PerMapLineData {
        std::size_t width, height;
        std::pmr::vector<bool> newUp, newDown, updated;
        std::pmr::vector<Tile> map;

        PerMapLineData(std::size_t width, std::size_t height, std::pmr::memory_resource *mmr) 
            : width(width), height(height),
              newUp(width, false, mmr), newDown(width, false, mmr), updated(width, false, mmr), 
              map(width*height, Tile(), mmr) {

            std::clog << "Creating line with: " << width << ' ' << height << '\n'; // TODO: comment out
        }

    };

    struct PerNumaData {
        std::pmr::vector<PerMapLineData> lines;

        unsigned numaNode;

        // width, height of the map for this numaNode
        PerNumaData(unsigned width, unsigned height, unsigned curNode, const ExecutionPlanner &exp,
                    std::pmr::memory_resource *pmr) 
            : lines(pmr), numaNode(curNode) {
            const std::vector<unsigned> &cpuList = exp.getCpuListPerNuma(curNode);
            unsigned cpuCnt = static_cast<unsigned>(cpuList.size());

            if(cpuCnt == 0) { return; }

            unsigned heightPerLine = height/(2*cpuCnt);
            unsigned heightRem = height - 2*cpuCnt*heightPerLine;

            lines.reserve(static_cast<std::size_t>(cpuCnt)*2);

            for(unsigned i=0; i<cpuCnt*2; ++i) {
                unsigned newHeight = heightPerLine;
                if(heightRem > 0) {
                    ++newHeight;
                    --heightRem;
                }

                lines.emplace_back(width, newHeight, pmr);
            }
        }
    };

    class Allocators {
    private:
#ifdef WATOR_NUMA
        NumaAllocator numaAlloc;
#endif
        std::pmr::monotonic_buffer_resource alloc;

    public:
        explicit Allocators(unsigned numaNode) : 
#ifdef WATOR_NUMA
            numaAlloc(numaNode), 
            alloc(&numaAlloc)
#else 
            alloc(std::pmr::get_default_resource())
#endif
            { }
        std::pmr::memory_resource* get() { return &alloc; }

    };

    unsigned m_width, m_height;
    unsigned numaCount;
    std::unique_ptr<std::unique_ptr<Allocators>[]> numaAlloc;

    struct PmrAllocDeleter {
    private:
        std::pmr::memory_resource *m_pmr;
    public:
        explicit PmrAllocDeleter(std::pmr::memory_resource *pmr) noexcept : m_pmr{pmr} {}

        explicit PmrAllocDeleter(std::nullptr_t) noexcept : m_pmr{nullptr} {}
        explicit PmrAllocDeleter() noexcept : PmrAllocDeleter(nullptr) {}

        void operator() (PerNumaData *ptr) {
            assert(m_pmr != nullptr);
            std::pmr::polymorphic_allocator<PerNumaData> alloc(m_pmr);
            alloc.destroy(ptr);
            alloc.deallocate(ptr, 1);
        }
    };

    std::unique_ptr<std::unique_ptr<PerNumaData, PmrAllocDeleter>[]> perNuma;

    void generateNuma(const Rules &rules, const ExecutionPlanner &exp, unsigned numaNode, unsigned heightPerCpu, 
            unsigned &heightRem, std::pmr::memory_resource *pmr) {
        unsigned curNumaCpuCnt = static_cast<unsigned>(exp.getCpuListPerNuma(numaNode).size());
        unsigned newHeight = 2*heightPerCpu*curNumaCpuCnt;
        newHeight += std::min(2*curNumaCpuCnt, heightRem);
        heightRem -= std::min(2*curNumaCpuCnt, heightRem);

        // rules.width, newHeight, numaNode, exp
        // perNuma[numaNode] = std::make_unique<PerNumaData, AllocatorDeleter>(rules.width, newHeight, numaNode, exp);
        std::pmr::polymorphic_allocator<PerNumaData> alloc{pmr};
        PerNumaData *ptr = alloc.allocate(1);

        // TODO: catch if this throws ... memory leak ... too bad :(
        alloc.construct(ptr, rules.width, newHeight, numaNode, exp, pmr);

        perNuma[numaNode] = {ptr, PmrAllocDeleter{pmr}};
    }

    // called inside constructor
    void fillRandom(const Rules &rules, unsigned seed) {
        struct Indx {
            unsigned numaInd;
            unsigned lineInd;
            size_t pos;
        };

        std::vector<Indx> indxList;
        indxList.reserve(rules.width*rules.height);

        for(unsigned numa=0; numa<numaCount; ++numa) {
            for(unsigned line=0; line<perNuma[numa]->lines.size(); ++line) {
                for(std::size_t i=0; i<perNuma[numa]->lines[line].map.size(); ++i) {
                    indxList.push_back({numa, line, i});
                }
            }
        }

        std::mt19937 mtg(seed);
        std::shuffle(indxList.begin(), indxList.end(), mtg);
        indxList.resize(rules.initialFishCount + rules.initialSharkCount);

        Tile defFish{Entity::FISH, 0, 0};
        Tile defShark{Entity::SHARK, 0, 0};

        for(std::size_t i=0; i<rules.initialFishCount; ++i) {
            const Indx &indx = indxList[i];
            perNuma[indx.numaInd]->lines[indx.lineInd].map[indx.pos] = defFish;
        }
        for(std::size_t i=rules.initialFishCount; 
                i<rules.initialFishCount+rules.initialSharkCount; ++i) {
            const Indx &indx = indxList[i];
            perNuma[indx.numaInd]->lines[indx.lineInd].map[indx.pos] = defShark;
        }
    }

public:

    Map(const Rules &rules, const ExecutionPlanner &exp, unsigned seed) {

        if(rules.height < static_cast<std::size_t>(2)*2*exp.getCpuCnt()) {
           throw std::runtime_error("Height is too small or CPU count is too large!");
        }

        m_width = rules.width;
        m_height = rules.height;

        numaCount = exp.getNumaList().size();
        if(exp.isNuma()) {
            
            numaAlloc = std::make_unique<std::unique_ptr<Allocators>[]>(numaCount);

            for(unsigned i=0; i<numaCount; ++i) {
                unsigned numaNode = exp.getNumaList()[i];
                numaAlloc[i] = std::make_unique<Allocators>(numaNode);
            }
            
        }
        perNuma = std::make_unique<std::unique_ptr<PerNumaData, PmrAllocDeleter>[]>(numaCount);

        unsigned heightPerCpu = rules.height/(2*exp.getCpuCnt());
        unsigned heightRem = rules.height - 2*exp.getCpuCnt()*heightPerCpu;

        if(exp.isNuma()) {
            for(unsigned i=0; i<exp.getNumaList().size(); ++i) {
                unsigned numaNode = exp.getNumaList()[i];
                generateNuma(rules, exp, numaNode, heightPerCpu, heightRem, numaAlloc[i]->get());
            }
        } else {
            generateNuma(rules, exp, 0, heightPerCpu, heightRem, std::pmr::get_default_resource());
        }

        fillRandom(rules, seed);
    }

    struct Cordinate {
        PerNumaData *numaData; // numaData == perNuma[numaInd].get()
        unsigned numaInd;
        unsigned lineInNuma;
        unsigned posy, posx;
    };

    [[nodiscard]] Cordinate dirHelper(Cordinate cord, unsigned dir) const {
        Cordinate res;
        int posy = static_cast<int>(cord.posy);
        int posx = static_cast<int>(cord.posx);
        
        constexpr int diry[] = {-1, 0, 1, 0};
        constexpr int dirx[] = {0, 1, 0, -1};

        int resY = posy + diry[dir];
        int resX = posx + dirx[dir];

        if(resX < 0) {
            resX = static_cast<int>(cord.numaData->lines[cord.lineInNuma].width-1);
        } else if(resX >= static_cast<int>(cord.numaData->lines[cord.lineInNuma].width-1)) {
            resX = 0;
        }

        int newNumaInd = static_cast<int>(cord.numaInd);
        int newLineInNuma = static_cast<int>(cord.lineInNuma);

        if(resY < 0)
        {
            --newLineInNuma;
            if(newLineInNuma < 0) {
                --newNumaInd;
                if(newNumaInd < 0) {
                    newNumaInd = static_cast<int>(numaCount-1); 
                }
                newLineInNuma = static_cast<int>(perNuma[static_cast<unsigned>(newNumaInd)]->lines.size()-1);
            }
            resY = perNuma[static_cast<unsigned>(newNumaInd)]->lines[newLineInNuma].height-1;
        } else if(resY >= cord.numaData->lines[newLineInNuma].height) {
            ++newLineInNuma;
            if(newLineInNuma >= cord.numaData->lines.size()) {
                ++newNumaInd;
                if(newNumaInd >= numaCount) {
                    newNumaInd = 0;
                }

                newLineInNuma = 0;
            }

            resY = 0;
        }

        if(cord.numaInd == static_cast<unsigned>(newNumaInd)) {
            res.numaData = cord.numaData;
        } else {
            res.numaData = perNuma[newNumaInd].get();
        }

        res.numaInd = newNumaInd;
        res.lineInNuma = newLineInNuma;
        res.posy = resY;
        res.posx = resX;

        return res;
    }

    // only works if cord.posy is not min or max
    [[nodiscard]] Cordinate dirHelperFastPath(Cordinate cord, unsigned dir) const {
        Cordinate res;
        int posy = static_cast<int>(cord.posy);
        int posx = static_cast<int>(cord.posx);
        
        constexpr int diry[] = {-1, 0, 1, 0};
        constexpr int dirx[] = {0, 1, 0, -1};

        int resY = posy + diry[dir];
        assert(0 <= resY && resY < cord.numaData->lines[cord.lineInNuma].height);
        int resX = posx + dirx[dir];

        if(resX < 0) {
            resX = static_cast<int>(cord.numaData->lines[cord.lineInNuma].width-1);
        } else if(resX >= static_cast<int>(cord.numaData->lines[cord.lineInNuma].width-1)) {
            resX = 0;
        }

        res.numaData = cord.numaData;
        res.numaInd = cord.numaInd;
        res.lineInNuma = cord.lineInNuma;
        res.posy = resY;
        res.posx = resX;

        return res;
    }

    [[nodiscard]] Cordinate getCord(unsigned numaInd, unsigned lineInd, unsigned posy, unsigned posx) {
        Cordinate cord;
        cord.numaInd = numaInd;
        cord.numaData = perNuma[cord.numaInd].get();
        cord.lineInNuma = lineInd;
        cord.posy = posy;
        cord.posx = posx;

        return cord;
    }

    [[nodiscard]] Tile& get(const Cordinate &cord) {
        assert(perNuma[cord.numaInd].get() == cord.numaData);

        PerMapLineData &line = cord.numaData->lines[cord.lineInNuma];
        return line.map[line.width*cord.posy + cord.posx];
    }

    [[nodiscard]] const Tile& get(const Cordinate &cord) const {
        assert(perNuma[cord.numaInd].get() == cord.numaData);

        PerMapLineData &line = cord.numaData->lines[cord.lineInNuma];
        return line.map[line.width*cord.posy + cord.posx];
    }

    [[nodiscard]] std::pmr::vector<bool>& getUpdateBuf(const Cordinate &cord) {
        assert(perNuma[cord.numaInd].get() == cord.numaData);

        PerMapLineData &line = cord.numaData->lines[cord.lineInNuma];
        return line.updated;
    }

    [[nodiscard]] const std::pmr::vector<bool>& getUpdateBuf(const Cordinate &cord) const {
        assert(perNuma[cord.numaInd].get() == cord.numaData);

        PerMapLineData &line = cord.numaData->lines[cord.lineInNuma];
        return line.updated;
    }

    [[nodiscard]] std::pair<unsigned, unsigned> getLineWidthHeight(const Cordinate &cord) const {
        assert(perNuma[cord.numaInd].get() == cord.numaData);

        PerMapLineData &line = cord.numaData->lines[cord.lineInNuma];
        return std::make_pair(line.width, line.height);
    }

    [[nodiscard]] unsigned getLineHeight(const Cordinate &cord) const {
        assert(perNuma[cord.numaInd].get() == cord.numaData);

        PerMapLineData &line = cord.numaData->lines[cord.lineInNuma];
        return line.height;
    }

    [[nodiscard]] const std::pmr::vector<bool>& getUpBuf(const Cordinate &cord) const {
        assert(perNuma[cord.numaInd].get() == cord.numaData);

        PerMapLineData &line = cord.numaData->lines[cord.lineInNuma];
        return line.newUp;
    }

    [[nodiscard]] std::pmr::vector<bool>& getUpBuf(const Cordinate &cord) {
        assert(perNuma[cord.numaInd].get() == cord.numaData);

        PerMapLineData &line = cord.numaData->lines[cord.lineInNuma];
        return line.newUp;
    }

    [[nodiscard]] const std::pmr::vector<bool>& getDownBuf(const Cordinate &cord) const {
        assert(perNuma[cord.numaInd].get() == cord.numaData);

        PerMapLineData &line = cord.numaData->lines[cord.lineInNuma];
        return line.newDown;
    }

    [[nodiscard]] std::pmr::vector<bool>& getDownBuf(const Cordinate &cord) {
        assert(perNuma[cord.numaInd].get() == cord.numaData);

        PerMapLineData &line = cord.numaData->lines[cord.lineInNuma];
        return line.newDown;
    }

    void saveMap(std::ostream &fout, bool includeHeader = false) const {
        if(includeHeader) {
            fout.write(reinterpret_cast<const char*>(&m_width), sizeof(m_width)); // NOLINT 
            fout.write(reinterpret_cast<const char*>(&m_height), sizeof(m_height)); // NOLINT
            std::size_t bytesPerMap = static_cast<std::size_t>(m_width)*m_height;
            bytesPerMap = (bytesPerMap+3)/4;
            fout.write(reinterpret_cast<const char*>(&bytesPerMap), sizeof(bytesPerMap)); // NOLINT
        }

        unsigned shift = 0;
        std::uint8_t buffer = 0;

        // TODO: remove
        volatile unsigned bytesWritten = 0;
        for(unsigned numaInd=0; numaInd<numaCount; ++numaInd) {
            for(unsigned lineInd=0; lineInd<perNuma[numaInd]->lines.size(); ++lineInd) {
                for(std::size_t i=0; i<perNuma[numaInd]->lines[lineInd].map.size(); ++i) {
                    unsigned curEnt = static_cast<unsigned>(perNuma[numaInd]->lines[lineInd].map[i].getEntity());
                    // curEnt &= 0x03;

                    buffer |= (curEnt << shift); shift += 2;
                    if(shift >= 8) { // NOLINT
                        shift = 0;
                        fout.write(reinterpret_cast<const char*>(&buffer), sizeof(buffer)); // NOLINT
                        ++bytesWritten;
                        buffer = 0;
                    }
                }
            }
        }

        if(shift != 0) {
            shift = 0;
            fout.write(reinterpret_cast<const char*>(&buffer), sizeof(buffer)); // NOLINT
            ++bytesWritten;
            buffer = 0;
        }

        // fout.flush();
    }

};

}
