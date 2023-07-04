#pragma once

#include "execution_planner.hpp"
#include "wator_map.hpp"
#include "wator_rules.hpp"
#include "worker.hpp"
#include "../src/lfsr_engine.hpp" // TODO: this!!!

#include <memory>
#include <random>
#include <type_traits>
#include <vector>

namespace WaTor {

class GameCG {
private:

    struct GamePerThdWork {
    private:
        Map *map;
        Rules rules;
        Map::Cordinate cord;
        // unsigned seed; // not zero
        linear_feedback_shift_register_engine<std::uint32_t, 0xDEADBEEF> rng;

        auto findTiles(const std::array<Map::Cordinate, 4> &dirs,
                        Map::Entity entSearch, unsigned &resSize) const
            -> std::array<unsigned, 4> ;

        auto findTilesFish(const std::array<Map::Cordinate, 4> &dirs,
                        unsigned &resSize) const
            -> std::array<unsigned, 4> ;

        auto findTilesShark(const std::array<Map::Cordinate, 4> &dirs,
                        unsigned &resSize, bool &ate) const
            -> std::array<unsigned, 4> ;

        template<class T, bool midInLine>
        [[nodiscard]] T updateFish(const Map::Cordinate &curCord, Map::Tile &curTile,
                        const std::array<Map::Cordinate, 4> &dirs);

        template<class T, bool midInLine>
        [[nodiscard]] T updateShark(const Map::Cordinate &curCord, Map::Tile &curTile,
                        const std::array<Map::Cordinate, 4> &dirs);
        template<unsigned vertLevel>
        void updateEntity(const Map::Cordinate &curCord);

    public:

        void operator() ();

        GamePerThdWork(Map *_map, unsigned _numaInd, unsigned _line, const Rules &_rules, unsigned seed);
    };

    std::unique_ptr<std::optional<Worker<GamePerThdWork>>[]> m_workers; // NOLINT
    Rules m_rules;
    const ExecutionPlanner *m_exp;
    Map m_map;
    std::mt19937 m_rng;

    void doHalfIterationNuma(bool odd);
    void doHalfIterationNoNuma(bool odd);

public:

    GameCG(const Rules &rules, const ExecutionPlanner &exp, unsigned seed);

    [[nodiscard]] const Map& getMap() const noexcept { return m_map; }
    [[nodiscard]] Map& getMap() noexcept { return m_map; }

    void doIteration();
};

}
