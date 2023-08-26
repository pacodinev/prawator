#include <algorithm>
#include <catch2/catch.hpp>
#include <cstddef>
#include <memory_resource>
#include <numeric>

#include "wator/map_line.hpp"
#include "wator/map_numa.hpp"

TEST_CASE("Wator::MapNuma General usage on NUMA") {  // NOLINT
    std::vector<unsigned> numaList = {0, 1};
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 1}, {10, 11}}; // NOLINT

    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma));

    using namespace WaTor;
    unsigned width = GENERATE(3U, 5U, 69U, 420U);
    MapNuma mapn{27, width, 1, exp, std::pmr::get_default_resource()}; // NOLINT

    // CHECK(mapn.getNumaIndex() == 1);
    CHECK(mapn.getLineCnt() == 4);
    for(unsigned line=0; line<mapn.getLineCnt(); ++line) {
        MapLine &curLine = mapn.getLine(line);
        const MapLine &ccurLine = static_cast<const MapNuma&>(mapn).getLine(line);
        if(line < 3) {
            CHECK(curLine.getHeight() == 7);
            CHECK(ccurLine.getHeight() == 7);
        } else {
            CHECK(curLine.getHeight() == 6);
            CHECK(ccurLine.getHeight() == 6);
        }
        CHECK(curLine.getWidth() == width);
        CHECK(ccurLine.getWidth() == width);
    }
}

TEST_CASE("Wator::MapNuma General usage NO NUMA") {  // NOLINT
    std::vector<unsigned> numaList = {0};
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 7, 13, 21}}; // NOLINT

    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma));

    using namespace WaTor;
    unsigned width = GENERATE(3U, 5U, 69U, 420U);
    MapNuma mapn{59, width, 0, exp, std::pmr::get_default_resource()}; // NOLINT

    // CHECK(mapn.getNumaIndex() == 0);
    CHECK(mapn.getLineCnt() == 8);
    for(unsigned line=0; line<mapn.getLineCnt(); ++line) {
        MapLine &curLine = mapn.getLine(line);
        const MapLine &ccurLine = static_cast<const MapNuma&>(mapn).getLine(line);
        if(line < 3) {
            CHECK(curLine.getHeight() == 8);
            CHECK(ccurLine.getHeight() == 8);
        } else {
            CHECK(curLine.getHeight() == 7);
            CHECK(ccurLine.getHeight() == 7);
        }
        CHECK(curLine.getWidth() == width);
        CHECK(ccurLine.getWidth() == width);
    }
}

TEST_CASE("Wator::MapNuma Verify line heights") {  // NOLINT
    std::vector<unsigned> numaList = {0, 1, 5};  // NOLINT
    std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 1}, {10, 11}, {20, 21, 22}}; // NOLINT

    ExecutionPlanner exp = ExecutionPlanner::makeMock(std::move(numaList), std::move(cpusPerNuma));

    using namespace WaTor;
    for(unsigned height=20; height<=30; ++height) { // NOLINT
        MapNuma mapn{height, 3, 1, exp, std::pmr::get_default_resource()}; // NOLINT

        std::vector<unsigned> heights;
        heights.reserve(4);
        for(unsigned line=0; line<mapn.getLineCnt(); ++line) {
            MapLine &curLine = mapn.getLine(line);
            heights.push_back(curLine.getHeight());
            CHECK(curLine.getWidth() == 3);
        }

        std::size_t sumHeight = std::accumulate(heights.begin(), heights.end(), static_cast<std::size_t>(0));
        CHECK(sumHeight == height);

        CHECK(std::is_sorted(heights.rbegin(), heights.rend()));
        CHECK((heights.front() == heights.back() || heights.front() == heights.back() + 1));
    }
}
