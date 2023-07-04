#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <ostream>
#include <thread>
#include <random>

#include "execution_planner.hpp"

#include "wator_map.hpp"
#include "wator_rules.hpp"
#include "wator_gamecg.hpp"

#include <config.h>

namespace {

#ifdef WATOR_CPU_PIN 
void pinThreadToFirstCpu(const ExecutionPlanner &exp) {
    unsigned fcpu = exp.getCpuListPerNuma(exp.getNumaList().front()).front();

    cpu_set_t cpuMask;
    CPU_ZERO(&cpuMask);
    CPU_SET(fcpu, &cpuMask); // NOLINT
    int ret = sched_setaffinity(0, sizeof(cpuMask), &cpuMask);
    assert(ret == 0); // TODO: yea, this is bad, only for debug
}
#else 
void pinThreadToFirstCpu(const ExecutionPlanner &exp) { }
#endif

void printHelp(std::ostream &out) {
    out << "Usage: \n"
           "    parwatorCG workerCnt enableUseOfHT mapWidth mapHeight initFishCnt "
           "initSharkCnt fishBreedTime sharkBreedTime sharkStarveTime iterCnt\n";
}

}

int main(int argc, char *argv[])
{
    if(argc != 11) {
        std::clog << "Invalid number of arguments!\n";
        printHelp(std::clog);
        std::clog.flush();
        return 1;
    }

    unsigned workerCnt;
    bool enableUseOfHT;
    unsigned mapWidth;
    unsigned mapHeight;
    unsigned initFishCnt;
    unsigned initSharkCnt;
    unsigned fishBreedTime;
    unsigned sharkBreedTime;
    unsigned sharkStarveTime;
    unsigned iterCnt;
    try { 
        unsigned curArg = 1;
        workerCnt = static_cast<unsigned>(std::strtol(argv[curArg++], nullptr, 10));
        enableUseOfHT = (std::strcmp(argv[curArg], "1") == 0 || 
                                std::strcmp(argv[curArg], "true") == 0) ? true : false; ++curArg;
        mapWidth = static_cast<unsigned>(std::strtol(argv[curArg++], nullptr, 10));
        mapHeight = static_cast<unsigned>(std::strtol(argv[curArg++], nullptr, 10));
        initFishCnt = static_cast<unsigned>(std::strtol(argv[curArg++], nullptr, 10));
        initSharkCnt = static_cast<unsigned>(std::strtol(argv[curArg++], nullptr, 10));
        fishBreedTime = static_cast<unsigned>(std::strtol(argv[curArg++], nullptr, 10));
        sharkBreedTime = static_cast<unsigned>(std::strtol(argv[curArg++], nullptr, 10));
        sharkStarveTime = static_cast<unsigned>(std::strtol(argv[curArg++], nullptr, 10));
        iterCnt = static_cast<unsigned>(std::strtol(argv[10], nullptr, 10));
    } catch(std::exception &ex) {
        std::clog << ex.what() << '\n';
        printHelp(std::clog);
        std::clog.flush();
        return 1;
    }
    if(iterCnt == 0) {
        return 0;
    }

    ExecutionPlanner::initInst(workerCnt, enableUseOfHT);

    const ExecutionPlanner &exp = ExecutionPlanner::getInst();

    pinThreadToFirstCpu(exp);

    exp.printStats(std::clog);

    //WaTor::Rules rules{1920, 1337, 4769, 476, 3, 10, 3};
    //WaTor::Rules rules{200, 400, 4760, 470, 3, 10, 3};
    WaTor::Rules rules{mapWidth, mapHeight, initFishCnt, initSharkCnt, 
                       fishBreedTime, sharkBreedTime, sharkStarveTime}; 

    std::random_device rdv{};

    // WaTor::Map map(rules, exp, rdv());
    
    std::fstream fmap("/tmp/gamemap.map", std::fstream::out | std::fstream::trunc);
    
    // WaTor::GameCG game(rules, exp, rdv());
    WaTor::GameCG game(rules, exp, rdv());

    game.getMap().saveMap(fmap, true);

    for(unsigned i=0; i<iterCnt-1; ++i) {
        game.doIteration();

        game.getMap().saveMap(fmap);
    }

    fmap.close();


    return 0;
}
