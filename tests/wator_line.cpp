#include <catch2/catch.hpp>
#include <memory_resource>

#include "wator/entity.hpp"
#include "wator/map_line.hpp"
#include "wator/tile.hpp"

using namespace WaTor;

namespace {
}

TEST_CASE("WaTor::MapLine constructor") {  // NOLINT
    MapLine res{3, 4, std::pmr::get_default_resource()};

    CHECK(res.getHeight() == 3);
    CHECK(res.getWidth() == 4);
    CHECK(res.getAbsSize() == 12);

    for(unsigned i=0; i<res.getHeight(); ++i) {
        for(unsigned j=0; j<res.getWidth(); ++j) {
            Tile &tile = res.get(i, j);
            CHECK(tile.getEntity() == Entity::WATER);

            const Tile& ctile= static_cast<const MapLine&>(res).get(i, j);
            CHECK(ctile.getEntity() == Entity::WATER);
        }
    }
}

TEST_CASE("WaTor::MapLine get and getAbs") {  // NOLINT
    MapLine res{3, 4, std::pmr::get_default_resource()};

    CHECK(res.getHeight() == 3);
    CHECK(res.getWidth() == 4);
    CHECK(res.getAbsSize() == 12);

    Tile defFish{Entity::FISH, 0, 0};
    Tile defShark{Entity::SHARK, 1, 1};

    SECTION("get r/w") {
        for(unsigned i=0; i<res.getHeight(); ++i) {
            for(unsigned j=0; j<res.getWidth(); ++j) {
                res.get(i, j) = (i%2 == 0) ? defFish : defShark;
            }
        }
        for(unsigned i=0; i<res.getHeight(); ++i) {
            for(unsigned j=0; j<res.getWidth(); ++j) {
                Tile curEnt = (i%2 == 0) ? defFish : defShark;
                const MapLine &cres = res;
                CHECK(curEnt == cres.get(i, j));
            }
        }
    }
    SECTION("getAbs r/w") {
        for(std::size_t i=0; i<res.getAbsSize(); ++i) {
            res.getAbs(i) = (i%2 == 0) ? defFish : defShark;
        }
        for(std::size_t i=0; i<res.getAbsSize(); ++i) {
            Tile curEnt = (i%2 == 0) ? defFish : defShark;
            const MapLine &cres = res;
            CHECK(curEnt == cres.getAbs(i));
        }
    }
}

TEST_CASE("WaTor::MapLine mask functions") { // NOLINT
    MapLine res{3, 4, std::pmr::get_default_resource()};

    for(unsigned i=0; i<res.getWidth(); ++i) {
        res.getTopMask(i) = (i%2 == 1);
        res.getUpdateMask(i) = (i%2 == 0);
        res.getBottomMask(i) = (i%2 == 1);
    }

    for(unsigned i=0; i<res.getWidth(); ++i) {
        const MapLine &cres = res;
        CHECK(cres.getTopMask(i) == (i%2 == 1));
        CHECK(cres.getUpdateMask(i) == (i%2 == 0));
        CHECK(cres.getBottomMask(i) == (i%2 == 1));
    }
}

TEST_CASE("WaTor::MapLine tileIter") { // NOLINT
    MapLine res{3, 4, std::pmr::get_default_resource()};

    Tile defFish{Entity::FISH, 0, 0};
    Tile defShark{Entity::SHARK, 1, 1};

    MapLine::TileIter titer = res.getTileIter(0, 0);
    for(std::size_t i=0; i<res.getAbsSize(); ++i) {
        *titer = (i%2 == 0) ? defFish : defShark;
        ++titer;
    }

    titer = res.getTileIter(0, 0);
    for(std::size_t i=0; i<res.getAbsSize(); ++i) {
        Tile curEnt = (i%2 == 0) ? defFish : defShark;

        CHECK(*titer == curEnt);
        ++titer;

        const MapLine &cres = res;
        CHECK(curEnt == cres.getAbs(i));
    }
}

TEST_CASE("WaTor::MapLine maskIter functions") { // NOLINT
    MapLine res{3, 4, std::pmr::get_default_resource()};

    MapLine::TopMaskIter tmiter{res.getTopMaskIter(0)};
    MapLine::UpdateMaskIter umiter{res.getUpdateMaskIter(0)};
    MapLine::BottomMaskIter bmiter{res.getBottomMaskIter(0)};
    for(unsigned i=0; i<res.getWidth(); ++i) {
        *tmiter = (i%2 == 1);
        ++tmiter;
        *umiter = (i%2 == 0);
        ++umiter;
        *bmiter = (i%2 == 1);
        ++bmiter;
    }

    tmiter = res.getTopMaskIter(0);
    umiter = res.getUpdateMaskIter(0);
    bmiter = res.getBottomMaskIter(0);
    for(unsigned i=0; i<res.getWidth(); ++i) {
        CHECK( *tmiter == (i%2 == 1));
        ++tmiter;
        CHECK( *umiter == (i%2 == 0));
        ++umiter;
        CHECK( *bmiter == (i%2 == 1));
        ++bmiter;

        const MapLine &cres = res;
        CHECK(cres.getTopMask(i) == (i%2 == 1));
        CHECK(cres.getUpdateMask(i) == (i%2 == 0));
        CHECK(cres.getBottomMask(i) == (i%2 == 1));
    }
}
