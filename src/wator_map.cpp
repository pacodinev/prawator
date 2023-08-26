#include "wator/map.hpp"
#include "wator/map_numa.hpp"

#include <algorithm>
#include <cerrno>
#include <system_error>


namespace WaTor {

    void Map::saveMap(std::ostream &fout, bool includeHeader) const {
        if(includeHeader) {
            fout.write(reinterpret_cast<const char*>(&m_width), sizeof(m_width)); // NOLINT 
            fout.write(reinterpret_cast<const char*>(&m_height), sizeof(m_height)); // NOLINT
            std::size_t bytesPerMap = static_cast<std::size_t>(m_width)*m_height;
            bytesPerMap = (bytesPerMap+3)/4;
            fout.write(reinterpret_cast<const char*>(&bytesPerMap), sizeof(bytesPerMap)); // NOLINT
        }

        unsigned shift = 0;
        std::uint8_t buffer = 0;

        for(unsigned numaInd=0; numaInd<getMapNumaCnt(); ++numaInd) {
            const MapNuma &numa = getMapNuma(numaInd);
            for(unsigned lineInd=0; lineInd<numa.getLineCnt(); ++lineInd) {
                const MapLine &line = numa.getLine(lineInd);
                for(unsigned i=0; i<line.getAbsSize(); ++i) {
                    const Tile& tile = line.getAbs(i);
                    unsigned entity = static_cast<unsigned>(tile.getEntity());

                    // entity &= 0x03;
                    buffer |= static_cast<std::uint8_t>(entity << shift); shift += 2;
                    if(shift >= 8) { // NOLINT
                        shift = 0;
                        fout.write(reinterpret_cast<const char*>(&buffer), sizeof(buffer)); // NOLINT
                        buffer = 0;
                    }
                }
            }
        }

        if(shift != 0) {
            shift = 0;
            fout.write(reinterpret_cast<const char*>(&buffer), sizeof(buffer)); // NOLINT
            buffer = 0;
        }

        // fout.flush();
    }

#ifdef __unix__
    
    void Map::saveMap(PosixFostream &fout, bool includeHeader) const {
        if(includeHeader) {
            fout.write(m_width);
            fout.write(m_height);
            std::size_t bytesPerMap = static_cast<std::size_t>(m_width)*m_height;
            bytesPerMap = (bytesPerMap+3)/4;
            fout.write(bytesPerMap);
        }

        unsigned shift = 0;
        std::uint8_t bits = 0;

        for(unsigned numaInd=0; numaInd<getMapNumaCnt(); ++numaInd) {
            const MapNuma &numa = getMapNuma(numaInd);
            for(unsigned lineInd=0; lineInd<numa.getLineCnt(); ++lineInd) {
                const MapLine &line = numa.getLine(lineInd);
                #pragma GCC unroll 16
                for(unsigned i=0; i<line.getAbsSize(); ++i) {
                    const Tile& tile = line.getAbs(i);
                    unsigned entity = static_cast<unsigned>(tile.getEntity());

                    // entity &= 0x03;
                    bits |= static_cast<std::uint8_t>(entity << shift); shift += 2;
                    if(shift >= 8) { // NOLINT
                        shift = 0;
                        fout.write(bits);
                        bits = 0;
                    }
                }
            }
        }

        if(shift != 0) {
            shift = 0;
            fout.write(bits);
            // bits = 0;
        }
    }

    void Map::randomize(std::size_t fishCnt, std::size_t sharkCnt, unsigned seed) {
        struct Indx {
            unsigned numaInd;
            unsigned lineInd;
            size_t pos;
        };

        std::vector<Indx> indxList;
        std::size_t mapSize = static_cast<std::size_t>(getWidth())*getHeight();
        indxList.reserve(mapSize);

        for(unsigned numaI=0; numaI<getMapNumaCnt(); ++numaI) {
            const MapNuma& mapNuma = getMapNuma(numaI);
            for(unsigned lineI=0; lineI<mapNuma.getLineCnt(); ++lineI) {
                const MapLine& line = mapNuma.getLine(lineI);
                std::size_t lineSize = static_cast<std::size_t>(line.getHeight())*line.getWidth();
                for(std::size_t i=0; i<lineSize; ++i) {
                    indxList.push_back({numaI, lineI, i});
                }
            }
        }

        std::mt19937 mtg(seed);
        std::shuffle(indxList.begin(), indxList.end(), mtg);
        indxList.resize(fishCnt + sharkCnt);

        Tile defFish{Entity::FISH, 0, 0};
        Tile defShark{Entity::SHARK, 0, 0};

        for(std::size_t i=0; i<fishCnt + sharkCnt; ++i) {
            const Indx &indx = indxList[i];
            MapNuma& mapNuma = getMapNuma(indx.numaInd);
            MapLine& line = mapNuma.getLine(indx.lineInd);
            unsigned posy = static_cast<unsigned>(indx.pos/line.getWidth());
            unsigned posx = static_cast<unsigned>(indx.pos%line.getWidth());
            line.get(posy, posx) = (i<fishCnt) ? defFish : defShark;
        }
    }

#endif
}
