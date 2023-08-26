#include <catch2/catch.hpp>
#include <cstddef>
#include <sstream>
#include <string>

#include "posixFostream.hpp"
#include "wator/entity.hpp"
#include "wator/map.hpp"
#include "wator/map_line.hpp"
#include "wator/map_numa.hpp"
#include "wator/rules.hpp"
#include "wator/tile.hpp"

TEST_CASE("WaTor::Map General usage on NUMA") { // NOLINT
    std::vector<unsigned> numaList = {0, 1};
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 1}, {2, 3}};
    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma));
    using namespace WaTor;

    Map map{51, 5, exp, std::make_unique<MockAllocStrategy>()};  // NOLINT

    CHECK(map.getHeight() == 51);
    CHECK(map.getWidth() == 5);
    CHECK(map.getMapNumaCnt() == 2);

    for(unsigned numaInd=0; numaInd<map.getMapNumaCnt(); ++numaInd) {
        MapNuma &numa = map.getMapNuma(numaInd);
        CHECK(numa.getLineCnt() == 4);
        for(unsigned lineInd=0; lineInd<numa.getLineCnt(); ++lineInd) {
            MapLine &line = numa.getLine(lineInd);
            const unsigned reqHeight = (numaInd == 0 && lineInd < 3) ? 7 : 6;
            CHECK(line.getHeight() == reqHeight);
            CHECK(line.getWidth() == 5);
        }
    }
}

TEST_CASE("WaTor::Map General usage NON NUMA") { // NOLINT
    std::vector<unsigned> numaList = {0};
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 7, 13, 21}};  // NOLINT
    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma));
    using namespace WaTor;

    Map map{51, 5, exp, std::make_unique<MockAllocStrategy>()};  // NOLINT

    CHECK(map.getHeight() == 51);
    CHECK(map.getWidth() == 5);
    CHECK(map.getMapNumaCnt() == 1);

    for(unsigned numaInd=0; numaInd<map.getMapNumaCnt(); ++numaInd) {
        MapNuma &numa = map.getMapNuma(numaInd);
        CHECK(numa.getLineCnt() == 8);
        for(unsigned lineInd=0; lineInd<numa.getLineCnt(); ++lineInd) {
            MapLine &line = numa.getLine(lineInd);
            const unsigned reqHeight = (numaInd == 0 && lineInd < 3) ? 7 : 6;
            CHECK(line.getHeight() == reqHeight);
            CHECK(line.getWidth() == 5);
        }
    }
}

TEST_CASE("WaTor::Map Verify line heights") {  // NOLINT
    std::vector<unsigned> numaList = {0, 1, 4};
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 1}, {10, 11}, {20, 21, 22}}; // NOLINT
    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma)); // NOLINT
    using namespace WaTor;

    for(unsigned height=50; height<=60; ++height) { // NOLINT
        Map map{height, 5, exp, std::make_unique<MockAllocStrategy>()};  // NOLINT

        std::vector<unsigned> heights;
        heights.reserve(8); // NOLINT
        for(unsigned numaInd=0; numaInd<map.getMapNumaCnt(); ++numaInd) {
            MapNuma &numa = map.getMapNuma(numaInd);
            for(unsigned lineInd=0; lineInd<numa.getLineCnt(); ++lineInd) {
                MapLine &line = numa.getLine(lineInd);
                heights.push_back(line.getHeight());
                CHECK(line.getWidth() == 5);
            }
        }

        std::size_t sumHeight = std::accumulate(heights.begin(), heights.end(), static_cast<std::size_t>(0));
        CHECK(sumHeight == height);

        CHECK(std::is_sorted(heights.rbegin(), heights.rend()));
        CHECK((heights.front() == heights.back() || heights.front() == heights.back() + 1));
    }
}

namespace {
    template<class TS, class TC>
    void mapSetAndCheck(WaTor::Map &map, TS ts, TC tc) {  // NOLINT
        using namespace WaTor;

        Map &cmap = map;
        for(unsigned numaInd=0; numaInd<map.getMapNumaCnt(); ++numaInd) {
            MapNuma &numa = map.getMapNuma(numaInd);
            for(unsigned lineInd=0; lineInd<numa.getLineCnt(); ++lineInd) {
                MapLine &line = numa.getLine(lineInd);
                for(unsigned posy=0; posy<line.getHeight(); ++posy) {
                    for(unsigned posx=0; posx<line.getWidth(); ++posx) {
                        ts(map, numaInd, lineInd, posy, posx);
                    }
                }
            }
        }

        for(unsigned numaInd=0; numaInd<map.getMapNumaCnt(); ++numaInd) {
            MapNuma &numa = map.getMapNuma(numaInd);
            for(unsigned lineInd=0; lineInd<numa.getLineCnt(); ++lineInd) {
                MapLine &line = numa.getLine(lineInd);
                for(unsigned posy=0; posy<line.getHeight(); ++posy) {
                    for(unsigned posx=0; posx<line.getWidth(); ++posx) {
                        tc(cmap, numaInd, lineInd, posy, posx);
                    }
                }
            }
        }
    }
}

TEST_CASE("WaTor::Map .get(numa, line, posy, posx)") {  // NOLINT
    std::vector<unsigned> numaList = {0, 1};
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 1}, {2, 3}};
    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma));
    using namespace WaTor;

    Map omap{41, 5, exp, std::make_unique<MockAllocStrategy>()};  // NOLINT

    const Tile dfish{Entity::FISH, 0, 0};
    const Tile dshark{Entity::SHARK, 1, 1};

    auto ts1 = [dfish, dshark](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned posy, unsigned posx) {
        Tile tile = ((posy + posx)%2 == 0) ? dfish : dshark;
        Tile &cur = map.getMapNuma(numaInd).getLine(lineInd).get(posy, posx);
        cur = tile;
    };
    auto tc1 = [dfish, dshark](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned posy, unsigned posx) {
        const Map &cmap = map;
        Tile tile = ((posy + posx)%2 == 0) ? dfish : dshark;
        const Tile &cur = cmap.get(numaInd, lineInd, posy, posx);
        CHECK(cur == tile);
    };
    auto ts2 = [dfish, dshark](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned posy, unsigned posx) {
        Tile ntile = ((posy + posx)%2 == 0) ? dshark : dfish;
        Tile &cur = map.get(numaInd, lineInd, posy, posx);
        cur = ntile;
    };
    auto tc2 = [dfish, dshark](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned posy, unsigned posx) {
        const Map &cmap = map;
        Tile ntile = ((posy + posx)%2 == 0) ? dshark : dfish;
        const Tile &cur = cmap.getMapNuma(numaInd).getLine(lineInd).get(posy, posx);
        CHECK(cur == ntile);
    };

    mapSetAndCheck(omap, ts1, tc1);
    mapSetAndCheck(omap, ts2, tc2);
}

TEST_CASE("WaTor::Map .getCord") {  // NOLINT
    std::vector<unsigned> numaList = {0, 1};
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 1}, {2, 3}};
    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma));
    using namespace WaTor;

    Map omap{39, 5, exp, std::make_unique<MockAllocStrategy>()}; // NOLINT

    const Tile dfish{Entity::FISH, 0, 0};
    const Tile dshark{Entity::SHARK, 1, 1};

    auto ts1 = [dfish, dshark](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned posy, unsigned posx) {
        Tile tile = ((posy + posx)%2 == 0) ? dfish : dshark;
        Tile &cur = map.get(numaInd, lineInd, posy, posx);
        cur = tile;
    };
    auto tc1 = [dfish, dshark](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned posy, unsigned posx) {
        const Map &cmap = map;
        Tile tile = ((posy + posx)%2 == 0) ? dfish : dshark;
        Map::Cordinate cord = map.makeCordinate(numaInd, lineInd, posy, posx);
        const Tile &cur = cmap.get(cord);
        CHECK(cur == tile);
    };
    auto ts2 = [dfish, dshark](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned posy, unsigned posx) {
        Tile ntile = ((posy + posx)%2 == 0) ? dshark : dfish;
        Map::Cordinate cord = map.makeCordinate(numaInd, lineInd, posy, posx);
        Tile &cur = map.get(cord);
        cur = ntile;
    };
    auto tc2 = [dfish, dshark](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned posy, unsigned posx) {
        const Map &cmap = map;
        Tile ntile = ((posy + posx)%2 == 0) ? dshark : dfish;
        const Tile &cur = cmap.get(numaInd, lineInd, posy, posx);
        CHECK(cur == ntile);
    };

    mapSetAndCheck(omap, ts1, tc1);
    mapSetAndCheck(omap, ts2, tc2);
}

TEST_CASE("WaTor::Map .getUpdateMask .getTopMask .getBottomMask") {  // NOLINT
    std::vector<unsigned> numaList = {0, 1};
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 1}, {2, 3}};
    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma));
    using namespace WaTor;

    Map omap{37, 5, exp, std::make_unique<MockAllocStrategy>()}; // NOLINT

    auto ts1 = [](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned, unsigned posx) {
        const bool bit = (posx % 2 == 0);
        auto cur1 = map.getMapNuma(numaInd).getLine(lineInd).getUpdateMask(posx);
        auto cur2 = map.getMapNuma(numaInd).getLine(lineInd).getTopMask(posx);
        auto cur3 = map.getMapNuma(numaInd).getLine(lineInd).getBottomMask(posx);
        cur1 = bit; cur2 = bit; cur3 = bit;
    };
    auto tc1 = [](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned, unsigned posx) {
        const Map &cmap = map;
        const bool bit = (posx % 2 == 0);
        bool cur1 = cmap.getUpdateMask(numaInd, lineInd, posx);
        bool cur2 = cmap.getBottomMask(numaInd, lineInd, posx);
        bool cur3 = cmap.getTopMask(numaInd, lineInd, posx);
        CHECK(cur1 == bit); CHECK(cur2 == bit); CHECK(cur3 == bit);
    };
    auto ts2 = [](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned, unsigned posx) {
        const bool nbit = (posx % 2 == 1);
        auto cur1 = map.getUpdateMask(numaInd, lineInd, posx);
        auto cur2 = map.getBottomMask(numaInd, lineInd, posx);
        auto cur3 = map.getTopMask(numaInd, lineInd, posx);
        cur1 = nbit; cur2 = nbit; cur3 = nbit;
    };
    auto tc2 = [](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned, unsigned posx) {
        const bool nbit = (posx % 2 == 1);
        const Map &cmap = map;
        bool cur1 = cmap.getMapNuma(numaInd).getLine(lineInd).getUpdateMask(posx);
        bool cur2 = cmap.getMapNuma(numaInd).getLine(lineInd).getTopMask(posx);
        bool cur3 = cmap.getMapNuma(numaInd).getLine(lineInd).getBottomMask(posx);
        CHECK(cur1 == nbit); CHECK(cur2 == nbit); CHECK(cur3 == nbit);
    };

    mapSetAndCheck(omap, ts1, tc1);
    mapSetAndCheck(omap, ts2, tc2);
}


TEST_CASE("WaTor::Map .dirHelper and .dirFastRight") {  // NOLINT
    std::vector<unsigned> numaList = {0, 1};
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 1}, {2, 3}};
    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma));
    using namespace WaTor;

    // chess pattern
    Map omap{32, 4, exp, std::make_unique<MockAllocStrategy>()};  // NOLINT

    const Tile dfish{Entity::FISH, 0, 0};
    const Tile dshark{Entity::SHARK, 1, 1};

    auto mts = [dfish, dshark](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned posy, unsigned posx) {
        // chess pattern
        const Tile curEnt{((posx + posy)%2 == 0) ? dfish : dshark};
        map.get(numaInd, lineInd, posy, posx) = curEnt;
        const bool bit = (posx % 2 == 0);
        map.getUpdateMask(numaInd, lineInd, posx) = bit;
        map.getTopMask(numaInd, lineInd, posx) = bit;
        map.getBottomMask(numaInd, lineInd, posx) = bit;
    };

    auto tc1 = [dfish, dshark](Map &map, unsigned numaInd, unsigned lineInd,  // NOLINT
                                         unsigned posy, unsigned posx) {
        const Map &cmap = map;

        const Tile opEnt{((posx + posy)%2 == 0) ? dshark : dfish};
        const bool nbit = (posx % 2 == 1);
        Map::Cordinate cord = map.makeCordinate(numaInd, lineInd, posy, posx);

        std::array<Map::Cordinate, 4> dirCord;
        for(unsigned i=0; i<dirCord.size(); ++i) {
            dirCord[i] = map.dirHelper(cord, i);
            CHECK(cmap.get(dirCord[i]) == opEnt);
            if(i == 1 || i == 3) {
                CHECK(cmap.getUpdateMask(dirCord[i]) == nbit);
                CHECK(cmap.getUpdateMask(dirCord[i]) == nbit);
                CHECK(cmap.getUpdateMask(dirCord[i]) == nbit);
            }
        }
    };

    Map::Cordinate ccord;
    // Map::CordTopMaskCache ctop;
    // Map::CordBottomMaskCache cbot;
    auto tc2 = [dfish, dshark, &ccord/*, &ctop, &cbot*/](Map &map, unsigned numaInd, unsigned lineInd, 
                                                     unsigned posy, unsigned posx) {
        const Map &cmap = map;

        if(posx == 0) {
            ccord = map.makeCordinate(numaInd, lineInd, posy, 0);
            // ctop = map.makeTopMaskCache(ccord);
            // cbot = map.makeBottomMaskCache(ccord);
        } else {
            const Tile curEnt{((posx + posy)%2 == 0) ? dfish : dshark};
            const bool bit = (posx % 2 == 0);

            // Map::Cordinate tcord = ccord;
            // Map::Cordinate bcord = ccord;

            map.dirRightFast(ccord);
            CHECK(cmap.get(ccord) == curEnt);
            CHECK(cmap.getUpdateMask(ccord) == bit);
            
            // map.dirRightFast(tcord, ctop);
            // CHECK(cmap.get(tcord) == curEnt);
            // CHECK(cmap.getUpdateMask(tcord) == bit);
            // CHECK(cmap.getTopMask(tcord, ctop) == bit);

            // map.dirRightFast(bcord, cbot);
            // CHECK(cmap.get(bcord) == curEnt);
            // CHECK(cmap.getUpdateMask(bcord) == bit);
            // CHECK(cmap.getBottomMask(bcord, cbot) == bit);
        }
    };

    mapSetAndCheck(omap, mts, tc1);
    mapSetAndCheck(omap, mts, tc2);
}

TEST_CASE("WaTor::Map .getUpdateMask(cord) .getTopMask(cord) .getBottomMask(cord)") {  // NOLINT
    std::vector<unsigned> numaList = {0, 1};
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 1}, {2, 3}};
    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma));
    using namespace WaTor;

    Map omap{37, 5, exp, std::make_unique<MockAllocStrategy>()};  // NOLINT

    auto ts1 = [](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned, unsigned posx) {
        const bool bit = (posx % 2 == 0);
        auto cur1 = map.getUpdateMask(numaInd, lineInd, posx);
        auto cur2 = map.getBottomMask(numaInd, lineInd, posx);
        auto cur3 = map.getTopMask(numaInd, lineInd, posx);
        cur1 = bit; cur2 = bit; cur3 = bit;
    };
    auto tc1 = [](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned posy, unsigned posx) {
        const bool bit = (posx % 2 == 0);
        const Map &cmap = map;
        Map::Cordinate cord = map.makeCordinate(numaInd, lineInd, posy, posx);
        bool cur1 = cmap.getUpdateMask(cord);
        bool cur2 = cmap.getTopMask(cord);
        bool cur3 = cmap.getBottomMask(cord);
        CHECK(cur1 == bit); CHECK(cur2 == bit); CHECK(cur3 == bit);
    };
    auto ts2 = [](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned posy, unsigned posx) {
        const bool nbit = (posx % 2 == 1);
        Map::Cordinate cord = map.makeCordinate(numaInd, lineInd, posy, posx);
        auto cur1 = map.getUpdateMask(cord);
        auto cur2 = map.getTopMask(cord);
        auto cur3 = map.getBottomMask(cord);
        cur1 = nbit; cur2 = nbit; cur3 = nbit;
    };
    auto tc2 = [](Map &map, unsigned numaInd, unsigned lineInd, 
                                         unsigned, unsigned posx) {
        const Map &cmap = map;
        const bool nbit = (posx % 2 == 1);
        bool cur1 = cmap.getUpdateMask(numaInd, lineInd, posx);
        bool cur2 = cmap.getTopMask(numaInd, lineInd, posx);
        bool cur3 = cmap.getBottomMask(numaInd, lineInd, posx);
        CHECK(cur1 == nbit); CHECK(cur2 == nbit); CHECK(cur3 == nbit);
    };

    mapSetAndCheck(omap, ts1, tc1);
    mapSetAndCheck(omap, ts2, tc2);
}

TEST_CASE("WaTor::Map .saveMap") {  // NOLINT
    std::vector<unsigned> numaList = {0, 1};
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 1}, {2, 3}};
    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma));
    using namespace WaTor;

    Map map{31, 5, exp, std::make_unique<MockAllocStrategy>()}; // NOLINT
    const Map &cmap = map;

    std::ostringstream ostr;
    SECTION("Without header") {
        cmap.saveMap(ostr, false);
        std::string fstr = ostr.str();

        unsigned mapSizeCalc = (31*5+3)/4;  // NOLINT
        CHECK(fstr.size() == mapSizeCalc);
        bool allZero = true;
        for(std::size_t i=0; i<fstr.size() && allZero; ++i) {
            allZero = (fstr[i] == 0);
        }
        CHECK(allZero == true);
    }
    SECTION("With header") {
        const Tile dfish{Entity::FISH, 0, 0};
        const Tile dshark{Entity::SHARK, 1, 1};

        for(unsigned numaInd=0; numaInd<map.getMapNumaCnt(); ++numaInd) {
            MapNuma &numa = map.getMapNuma(numaInd);
            for(unsigned lineInd=0; lineInd<numa.getLineCnt(); ++lineInd) {
                MapLine &line = numa.getLine(lineInd);
                for(unsigned posy=0; posy<line.getHeight(); ++posy) {
                    for(unsigned posx=0; posx<line.getWidth(); ++posx) {
                        // chess pattern
                        const Tile curEnt{((posx + posy)%2 == 0) ? dfish : dshark};
                        map.get(numaInd, lineInd, posy, posx) = curEnt;
                    }
                }
            }
        }

        cmap.saveMap(ostr, true);
        CHECK(!ostr.str().empty());
    }
}

#ifdef __unix__

#include <fcntl.h>

TEST_CASE("WaTor::Map .saveMap posixFostream") { // NOLINT
    // cannot test it without another process/thread
    
    std::vector<unsigned> numaList = {0, 1};
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 1}, {2, 3}};
    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma));
    using namespace WaTor;

    Map map{31, 5, exp, std::make_unique<MockAllocStrategy>()}; // NOLINT
    const Map &cmap = map;

    const std::size_t bufSize = GENERATE(0U, 13);
    PosixFostream fout("/dev/null", O_WRONLY | O_CLOEXEC, 0777, bufSize); // NOLINT

    SECTION("No header") {
        cmap.saveMap(fout, false);
    }
    SECTION("With header") {
        cmap.saveMap(fout, true);
    }
}

#endif // __unix__ 
