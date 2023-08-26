#include <algorithm>
#include <catch2/catch.hpp>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <thread>

#include "execution_planner.hpp"

namespace {

// works only for sorted container!!!
template<class It>
bool areDifferent(It beg, It end) {
    It prev = beg;
    ++beg; 
    while(beg != end) {
        if(*prev == *beg) {
            return false;
        }
        ++prev; ++beg;
    }

    return true;
}

void sanityCheckExecPlan(const ExecutionPlanner &exp) {  // NOLINT
    CHECK(std::is_sorted(exp.getNumaList().begin(), exp.getNumaList().end()));
    if(!exp.isNuma()) {
        CHECK(exp.getNumaList().size() == 1);
        CHECK(exp.getNumaList()[0] == 0);
    }
    for(unsigned numaInd=0; numaInd<exp.getNumaList().size(); ++numaInd) {
        // unsigned numaNode = epl.getNumaList()[numaInd];
        const std::vector<unsigned> &curCpuList = exp.getCpuListPerNuma(numaInd);
        CHECK(std::is_sorted(curCpuList.begin(), curCpuList.end()) == true);
        CHECK(!curCpuList.empty());
    }
}

void checkEPResult(unsigned numThreads, bool enableHT) { // NOLINT
    const ExecutionPlanner epl{numThreads, enableHT};

    sanityCheckExecPlan(epl);

    std::vector<unsigned> allCpuList;
    allCpuList.reserve(numThreads);
    std::size_t cpusTotal = 0;
    for(unsigned numaInd=0; numaInd<epl.getNumaList().size(); ++numaInd) {
        // unsigned numaNode = epl.getNumaList()[numaInd];
        const std::vector<unsigned> &curCpuList = epl.getCpuListPerNuma(numaInd);
        cpusTotal += curCpuList.size();
        std::copy(curCpuList.begin(), curCpuList.end(), std::back_inserter(allCpuList));
    }
    CHECK(cpusTotal == numThreads);

    std::sort(allCpuList.begin(), allCpuList.end());
    CHECK(areDifferent(allCpuList.begin(), allCpuList.end()));
}

}

TEST_CASE("General use, no HT") {
    unsigned hdc = std::thread::hardware_concurrency();

    // this test will fail on some strange ... architectures ...
    for(unsigned i=1; i<std::max(hdc/2, 1U); ++i) {
        checkEPResult(i, false);
    }
}

TEST_CASE("General use, with HT") {
    unsigned hdc = std::thread::hardware_concurrency();

    // this test will fail on some strange ... architectures ...
    for(unsigned i=1; i<hdc; ++i) {
        checkEPResult(i, true);
    }
}

TEST_CASE("Requested more cpus than machine has") {
    unsigned numThd = GENERATE(std::thread::hardware_concurrency() + 1,
                               2*std::thread::hardware_concurrency());
    bool enableHT  = GENERATE(false, true);

    bool throws{false};
    try {
        ExecutionPlanner exp{numThd, enableHT};
    } catch(...) {
        throws = true;
    }

    CHECK(throws == true);
}

TEST_CASE("Sanitiy checks for printStats") {
    std::ostringstream oss;

    unsigned hdc = std::thread::hardware_concurrency();
    for(unsigned i=1; i<hdc; ++i) {
        for(bool enHT : {false, true}) {
            if(!enHT && 2*i > hdc) {
                continue;
            }
            ExecutionPlanner exp{i, enHT};

            exp.printStats(oss);
            CHECK(!oss.str().empty());
        }
    }
}

TEST_CASE("makeMock") { // NOLINT
    {
        std::vector<unsigned> numaList = {0, 1, 5, 7};  // NOLINT
        std::vector<std::vector<unsigned>> cpusPerNuma = {{0, 3, 5, 69}, {10, 11, 13}, {21, 25}, {27}};  // NOLINT

        ExecutionPlanner exp = ExecutionPlanner::makeMock(numaList, cpusPerNuma);

        CHECK(exp.isNuma() == true);
        CHECK(exp.getCpuCnt() == 10);
        CHECK(exp.getNumaList() == numaList);
        for(unsigned numaInd=0; numaInd<exp.getNumaList().size(); ++numaInd) {
            CHECK(exp.getCpuListPerNuma(numaInd) == cpusPerNuma[numaInd]);
        }
    }
    {
        std::vector<unsigned> numaList = {0};
        std::vector<std::vector<unsigned>> cpusPerNuma = {{7, 9, 21, 27, 31}};  // NOLINT

        ExecutionPlanner exp = ExecutionPlanner::makeMock(numaList, cpusPerNuma);

        CHECK(exp.isNuma() == false);
        CHECK(exp.getCpuCnt() == 5);
        CHECK(exp.getNumaList() == numaList);
        for(unsigned numaInd=0; numaInd<exp.getNumaList().size(); ++numaInd) {
            CHECK(exp.getCpuListPerNuma(numaInd) == cpusPerNuma[numaInd]);
        }
    }
}
