#pragma once

#include <vector>

#include "map_line.hpp"
#include "execution_planner.hpp"

namespace WaTor {

    class MapNuma {
    private:
        std::pmr::vector<MapLine> m_lines;

    public:

        // width, height of the map for this numaNode
        MapNuma(unsigned height, unsigned width, unsigned numaInx, const ExecutionPlanner &exp,
                    std::pmr::memory_resource *pmr) 
            : m_lines(pmr) {
            const std::vector<unsigned> &cpuList = exp.getCpuListPerNuma(numaInx);
            unsigned cpuCnt = static_cast<unsigned>(cpuList.size());

            if(cpuCnt == 0) { return; }

            // TODO: granularity
            unsigned heightPerLine = height/(2*cpuCnt);
            unsigned heightRem = height - 2*cpuCnt*heightPerLine;

            m_lines.reserve(static_cast<std::size_t>(cpuCnt)*2);

            for(unsigned i=0; i<cpuCnt*2; ++i) {
                unsigned newHeight = heightPerLine;
                if(heightRem > 0) {
                    ++newHeight;
                    --heightRem;
                }

                m_lines.emplace_back(newHeight, width, pmr);
            }
        }

        [[nodiscard]] MapLine& getLine(unsigned line) {
            return m_lines[line];
        }

        [[nodiscard]] const MapLine& getLine(unsigned line) const {
            return m_lines[line];
        }

        [[nodiscard]] unsigned getLineCnt() const {
            return static_cast<unsigned>(m_lines.size());
        }
    };

}
