#pragma once

#include "execution_planner.hpp"
#include "map.hpp"
#include "entity.hpp"
#include "wator_rules.hpp"
#include "worker.hpp"
#include "../src/lfsr_engine.hpp" // TODO: this!!!

#include <cstdint>
#include <memory>
#include <random>
#include <type_traits>
#include <vector>

namespace WaTor {

class GameCG {
private:

    struct GamePerThdWork {
    private:
        Map *m_map;
        Rules m_rules;
        linear_feedback_shift_register_engine<std::uint32_t, 0xDEADBEEF> rng;
        unsigned m_numaInd, m_line;

        auto findTiles(const std::array<Map::Cordinate, 4> &dirs,
                        Entity entSearch, unsigned &resSize) const
            -> std::array<unsigned, 4> ;

        auto findTilesFish(const std::array<Map::Cordinate, 4> &dirs,
                        unsigned &resSize) const
            -> std::array<unsigned, 4> ;

        auto findTilesShark(const std::array<Map::Cordinate, 4> &dirs,
                        unsigned &resSize, bool &ate) const
            -> std::array<unsigned, 4> ;

        template<class T, bool midInLine>
        [[nodiscard]] T updateFish(const Map::Cordinate &curCord, Tile &curTile,
                        const std::array<Map::Cordinate, 4> &dirs);

        template<class T, bool midInLine>
        [[nodiscard]] T updateShark(const Map::Cordinate &curCord, Tile &curTile,
                        const std::array<Map::Cordinate, 4> &dirs);

        template<unsigned vertLevel>
        void updateEntity(const Map::Cordinate &curCord);

        void calcStats();

    public:

        void operator() ();

        GamePerThdWork(Map *_map, unsigned _numaInd, unsigned _line, const Rules &_rules, unsigned seed);
    };

    std::unique_ptr<std::optional<Worker<GamePerThdWork>>[]> m_workers; // NOLINT
    Rules m_rules;
    const ExecutionPlanner *m_exp;
    Map m_map;
    std::mt19937 m_rng;
    std::vector<std::chrono::microseconds> m_waitingTime;
    std::chrono::microseconds m_worker0LastDuration = std::chrono::microseconds{0};
    std::chrono::microseconds m_worker0AllDuration = std::chrono::microseconds{0};
    std::chrono::microseconds m_lastWatingDelta = std::chrono::microseconds{0};
    std::chrono::microseconds m_sumWatingDelta = std::chrono::microseconds{0};
    std::chrono::microseconds m_maxWatingDelta = std::chrono::microseconds{0};
    std::chrono::microseconds m_allTime = std::chrono::microseconds{0};
    std::uint64_t m_worker0LastFreq{0};
    std::uint64_t m_worker0SumFreq{0};
    std::uint64_t m_halfIterCnt{0};
    unsigned m_worker0Cpu{0};

    void calcStats();

    void doHalfIteration(bool odd);

public:

    GameCG(const Rules &rules, const ExecutionPlanner &exp, unsigned seed);

    [[nodiscard]] const Map& getMap() const noexcept { return m_map; }
    [[nodiscard]] Map& getMap() noexcept { return m_map; }

    void doIteration();

    // in kHz
    [[nodiscard]] std::vector<std::uint64_t> getAvgFreqPerWorker() const;

    [[nodiscard]] std::vector<std::chrono::microseconds> getAllRunDurationPerWorker() const;

    [[nodiscard]] std::vector<std::chrono::microseconds> getWaitingTimePerWorker() const {
        return m_waitingTime;
    }

    [[nodiscard]] std::chrono::microseconds getMaxWaitingDelta() const {
        return m_maxWatingDelta;
    }

    [[nodiscard]] std::chrono::microseconds getAvgWaitingDelta() const {
        return m_sumWatingDelta/m_halfIterCnt;
    }

    [[nodiscard]] std::chrono::microseconds getAllRunTime() const {
        return m_allTime;
    }

    [[nodiscard]] std::chrono::microseconds getWorker0AllRunDuration() const {
        return m_worker0AllDuration;
    }

    // in kHz
    [[nodiscard]] std::uint64_t getWorker0AvgFreq() const {
        return m_worker0SumFreq/static_cast<std::uint64_t>(m_worker0AllDuration.count());
    }

    [[nodiscard]] std::chrono::microseconds getWorker0LastRunDuration() const {
        return m_worker0LastDuration;
    }

    // in kHz
    [[nodiscard]] std::uint64_t getWorker0LastFreq() const {
        return m_worker0LastFreq;
    }

    void clearWorker0Stats() {
        m_worker0AllDuration = std::chrono::microseconds{0};
        m_worker0SumFreq = 0;
    }
};

}
