#pragma once

#include <chrono>
#include <cstdint>
#include <vector>
#include <memory>
#include <random>

#include "rules.hpp"
#include "map.hpp"
#include "simulation_worker.hpp"
#include "execution_planner.hpp"
#include "worker.hpp"

namespace WaTor {
    
class Simulation {
private:

    std::unique_ptr<Worker<SimulationWorker>[]> m_workers; // NOLINT
    Rules m_rules;
    const ExecutionPlanner &m_exp;
    Map m_map;
    std::mt19937 m_rng;
    std::chrono::microseconds m_allTime = std::chrono::microseconds{0};
    std::vector<std::chrono::microseconds> m_waitingTime;
    std::uint64_t m_halfIterCnt{0};

    // member functions
    void calcHalfIterStats();

    void doHalfIteration(bool odd);

public:
    
    Simulation(const Rules &rules, const ExecutionPlanner &exp, unsigned seed);

    [[nodiscard]] const Map& getMap() const noexcept { return m_map; }
    [[nodiscard]] Map& getMap() noexcept { return m_map; }

    void doIteration();

    [[nodiscard]] std::vector<std::uint64_t> getAvgFreqPerWorker() const;

    [[nodiscard]] std::uint64_t getAvgFreq() const;

    [[nodiscard]] std::chrono::microseconds getAllRunTime() const {
        return m_allTime;
    }

    [[nodiscard]] const std::vector<std::chrono::microseconds>& 
                                getWaitingTimePerThread() const {
        return m_waitingTime;
    }

    // weightedWaitingTime - the sum of all durations during a thread had to wait 
    // (waisting time) for another thread to finish work divided by the thread count
    // or the time an "average" thread had to wait for another
    [[nodiscard]] std::chrono::microseconds getWeightedWaitingTime() const;

};

}
