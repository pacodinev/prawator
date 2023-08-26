#include <catch2/catch.hpp>

#include "wator/entity.hpp"
#include "wator/tile.hpp"

TEST_CASE("Wator::Tile water usage") {
    using namespace WaTor;

    {
        Tile tile; // default constructor should be water

        CHECK(tile.getEntity() == Entity::WATER);
    } {
        Tile tile;

        tile.set(Entity::WATER, 0, 0);

        CHECK(tile.getEntity() == Entity::WATER);
    } {
        Tile tile{Entity::WATER, 0, 0};

        Tile tile2{tile};

        CHECK(tile == tile2);
    } 
}

TEST_CASE("Wator::Tile fish usage") {  // NOLINT
    using namespace WaTor;

    for(unsigned i=0; i<=Tile::MAX_AGE; ++i) {
        {
            Tile tile{Entity::FISH, i, 0};

            CHECK(tile.getEntity() == Entity::FISH);
            CHECK(tile.getAge() == i);
        } {
            Tile tile;
            tile.set(Entity::FISH, i, 0);

            CHECK(tile.getEntity() == Entity::FISH);
            CHECK(tile.getAge() == i);
        } {
            Tile tile;
            tile.set(Entity::FISH, 0, 0);
            tile.setAge(i);

            CHECK(tile.getEntity() == Entity::FISH);
            CHECK(tile.getAge() == i);
        } {
            Tile tile{Entity::FISH, i, 0};

            Tile tile2{tile};

            CHECK(tile == tile2);
        } 
    }
}

TEST_CASE("Wator::Tile shark usage") { // NOLINT
    using namespace WaTor;

    for(unsigned i=0; i<=Tile::MAX_AGE; ++i) {
        for(unsigned j=0; j<=Tile::MAX_LAST_ATE; ++j) {
            {
                Tile tile{Entity::SHARK, i, j};

                CHECK(tile.getEntity() == Entity::SHARK);
                CHECK(tile.getAge() == i);
                CHECK(tile.getLastAte() == j);
            } {
                Tile tile;
                tile.set(Entity::SHARK, i, j);

                CHECK(tile.getEntity() == Entity::SHARK);
                CHECK(tile.getAge() == i);
                CHECK(tile.getLastAte() == j);
            } {
                Tile tile;
                tile.set(Entity::SHARK, 0, 0);
                tile.setAge(i);
                tile.setLastAte(j);

                CHECK(tile.getEntity() == Entity::SHARK);
                CHECK(tile.getAge() == i);
                CHECK(tile.getLastAte() == j);
            } {
                Tile tile{Entity::SHARK, i, j};

                Tile tile2{tile};

                CHECK(tile == tile2);
            } 
        }
    }
}
