#include <algorithm>
#include <catch2/catch.hpp>
#include <thread>

#include "execution_planner.hpp"

namespace {

void checkEPResult(unsigned numThreads, bool enableHT) {
    const ExecutionPlanner epl{numThreads, enableHT};
    unsigned cpusTotal = 0;
    if(epl.isNuma()) {
        for(unsigned numaNode : epl.getNumaList()) {
            const std::vector<unsigned> &curCpuList = epl.getCpuListPerNuma(numaNode);
            cpusTotal += curCpuList.size();
            CHECK(std::is_sorted(curCpuList.begin(), curCpuList.end()) == true);
        }
    } else // NUMA is not supported
    {
        CHECK(epl.getNumaList().empty());
        const std::vector<unsigned> &curCpuList = epl.getCpuListPerNuma(0);
        cpusTotal += curCpuList.size();
        CHECK(std::is_sorted(curCpuList.begin(), curCpuList.end()) == true);
    }
    CHECK(cpusTotal == numThreads);

}

}

TEST_CASE("General use, single thread") {
    checkEPResult(1, true);

    checkEPResult(1, false);
}

TEST_CASE("General use, half threads") {
    checkEPResult(std::thread::hardware_concurrency()/2, true);

    checkEPResult(std::thread::hardware_concurrency()/2, false);
}

TEST_CASE("General use, all threads") {
    checkEPResult(std::thread::hardware_concurrency(), true);
}
