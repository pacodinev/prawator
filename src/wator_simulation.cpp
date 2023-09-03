#include "wator/simulation.hpp"
#include "wator/simulation_worker.hpp"
#include <chrono>
#include <numeric>

namespace WaTor {

Simulation::Simulation(const Rules &rules, const ExecutionPlanner &exp, unsigned seed)
    : m_rules(rules), m_exp(exp), m_map(m_rules, m_exp), m_rng(seed), 
      m_waitingTime(m_exp.getCpuCnt(), std::chrono::microseconds{0}) {
    m_workers = std::make_unique<Worker<SimulationWorker>[]>(m_exp.getCpuCnt()); // NOLINT
    
    unsigned cpuInd = 0;
    for(unsigned numaInd=0; numaInd<m_exp.getNumaList().size(); ++numaInd) {
        unsigned numaNode = m_exp.getNumaList()[numaInd];
        for(unsigned cpu : m_exp.getCpuListPerNuma(numaInd)) {
            if(cpuInd != 0) {
                if(m_exp.isNuma()) {
                    m_workers[cpuInd].startThread(cpu, numaNode);
                } else {
                    m_workers[cpuInd].startThread(cpu);
                }
            }
            ++cpuInd;
        }
    }

    m_map.randomize(m_rules, static_cast<unsigned>(m_rng()));
}

void Simulation::calcHalfIterStats() {
    ++m_halfIterCnt;

    using namespace std::chrono;
    microseconds maxTime = m_workers[0].getLastRunDuration();
    for(unsigned cpuInd=1; cpuInd<m_exp.getCpuCnt(); ++cpuInd) {
        microseconds lastDuration = m_workers[cpuInd].getLastRunDuration();
        maxTime = std::max(maxTime, lastDuration);
    }
    for(unsigned cpuInd=0; cpuInd<m_exp.getCpuCnt(); ++cpuInd) {
        microseconds lastDuration = m_workers[cpuInd].getLastRunDuration();
        m_waitingTime[cpuInd] += maxTime - lastDuration;
    }
}

void Simulation::doHalfIteration(bool odd) {
    unsigned cpuInd = 1;

    const unsigned uodd = static_cast<unsigned>(odd);
    for(unsigned i=0; i<m_exp.getNumaList().size(); ++i) {
        for(unsigned j=0; j<m_exp.getCpuListPerNuma(i).size(); ++j) {
            if(i == 0 && j == 0) {
                continue;
            }
            unsigned rnd = static_cast<unsigned>(m_rng());
            auto &&work = SimulationWorker{m_map, i, 2*j+uodd, m_rules, rnd};
            m_workers[cpuInd].pushWork(work);
            ++cpuInd;
        }
    }

    unsigned rnd = static_cast<unsigned>(m_rng());
    auto &&work = SimulationWorker{m_map, 0, uodd, m_rules, rnd};
    m_workers[0].pushWork(work);

    m_workers[0].runOnThisThread();

    for(unsigned i=1; i<m_exp.getCpuCnt(); ++i) {
        m_workers[i].waitFinish();
    }

    calcHalfIterStats();
}

void Simulation::doIteration() {
    auto clockStart = std::chrono::steady_clock::now();
    doHalfIteration(false);
    doHalfIteration(true);
    auto clockEnd = std::chrono::steady_clock::now();
    std::chrono::microseconds diff = std::chrono::duration_cast<std::chrono::microseconds>(clockEnd - clockStart);
    m_allTime += diff;
}

std::vector<std::uint64_t> Simulation::getAvgFreqPerWorker() const {
    std::vector<std::uint64_t> res(m_exp.getCpuCnt());
    for(unsigned i=0; i<m_exp.getCpuCnt(); ++i) {
        res[i] = m_workers[i].getAvgFreq();
    }
    return res;
}

std::uint64_t Simulation::getAvgFreq() const {
    std::uint64_t freqSum{0};
    for(unsigned i=0; i<m_exp.getCpuCnt(); ++i) {
        freqSum += m_workers[i].getAvgFreq();
    }
    return freqSum / m_exp.getCpuCnt();
}

std::chrono::microseconds Simulation::getWeightedWaitingTime() const {
    std::chrono::microseconds res = 
                    std::accumulate(m_waitingTime.begin(), m_waitingTime.end(), 
                    std::chrono::microseconds{0});
    res /= static_cast<unsigned>(m_waitingTime.size());
    return res;
}

}
