#pragma once

#include "execution_planner.hpp"
#include "wator_map.hpp"
#include "wator_rules.hpp"
#include "worker.hpp"
#include "../src/lfsr_engine.hpp" // TODO: this!!!

#include <memory>
#include <random>
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

        auto findTile(const std::array<Map::DirHelperData, 4> &dirs, 
                        Map::Entity entSearch, unsigned &resSize) const
            -> std::array<Map::DirHelperData, 4> ;

        void updateFish(const Map::Cordinate &curCord, Map::Tile &curTile,
                        const std::array<Map::DirHelperData, 4> &dirs);

        void updateShark(const Map::Cordinate &curCord, Map::Tile &curTile,
                        const std::array<Map::DirHelperData, 4> &dirs);

        void updateMidLine(const Map::Cordinate &curCord);

        void updateUpLine(const Map::Cordinate &curCord);

        void updateDownLine(const Map::Cordinate &curCord);

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
