#pragma once

#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>
#include <cassert>

class ExecutionPlanner {

public:
    static std::optional<ExecutionPlanner> instance; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    static void initInst(unsigned numThreads, bool enableHT) {
        if(instance.has_value()) {
            throw std::runtime_error("Instance already initialized");
        }
        instance.emplace(numThreads, enableHT);
    }

    static const ExecutionPlanner& getInst() {
        return instance.value(); // NOLINT(bugprone-unchecked-optional-access)
    }

private:
    // private variables
    std::vector<unsigned> m_numaList;
    std::vector<std::vector<unsigned>> m_cpuPerNuma;
    unsigned m_cpuCnt;
    bool m_isNuma;


    // private member functions
    using ThreadList = std::vector<unsigned>;
    using CpuList = std::vector<ThreadList>;
    using NumaList = std::vector<CpuList>;

    static NumaList getCpuArchitecture(bool isNuma);

    // cpuList should be sorted!
    void buildFromCPUList(const NumaList &cpuArch, std::vector<unsigned> &cpuList);

    void buildFromCPUArch(const NumaList &cpuArch, unsigned numThreads, bool enableHT);

    ExecutionPlanner(std::vector<unsigned> numaList, std::vector<std::vector<unsigned>> cpuPerNuma);

public:

    ExecutionPlanner(unsigned numThreads, bool enableHT);

    static ExecutionPlanner makeMock(std::vector<unsigned> numaList, 
                        std::vector<std::vector<unsigned>> cpuPerNuma) {
        return ExecutionPlanner{std::move(numaList), std::move(cpuPerNuma)};
    }

    [[nodiscard]] bool isNuma() const { return m_isNuma; }
    [[nodiscard]] auto getNumaList() const
    -> const std::vector<unsigned>& { return m_numaList; }
    // for NonNUMA system use numaInd = 0
    // .getNumaList()[numaInd] == numaNode <=> .getCpuListPerNuma(numaInd)
    [[nodiscard]] auto getCpuListPerNuma(unsigned numaInd) const
    -> const std::vector<unsigned>& {
        assert(isNuma() || numaInd == 0);

        return m_cpuPerNuma[numaInd];
    }
    [[nodiscard]] unsigned getCpuCnt() const { return m_cpuCnt; }

    void printStats(std::ostream &out) const;

};
