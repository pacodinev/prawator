#include <bits/chrono.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <numeric>
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
           "initSharkCnt fishBreedTime sharkBreedTime sharkStarveTime iterCnt mapSavePath\n";
}

void printStats(WaTor::GameCG &game, std::ostream &out) {
    out << "All simulation time: " << game.getAllRunTime().count() << " us = "
        << static_cast<double>(game.getAllRunTime().count())/1000000.0 << " s\n"
           "Run duration per worker: ";
    auto rdwpw = game.getAllRunDurationPerWorker();
    for (const auto & rdw : rdwpw) {
        out << rdw.count() << "us ";
    }
    out << "\n" 
           "Sync wait duration per worker: ";
    auto wdwpw = game.getWaitingTimePerWorker();
    for (const auto & wdw : wdwpw) {
        out << wdw.count() << "us ";
    }
    out << "\n"
           "Percent of time waiting: ";
    assert(rdwpw.size() > 0 && rdwpw.size() == wdwpw.size());
    for(std::size_t i=0; i<rdwpw.size(); ++i) {
        double wtd = static_cast<double>(wdwpw[i].count());
        double tld = static_cast<double>((wdwpw[i]+rdwpw[i]).count());
        out << wtd*100.0/tld << "% ";
    }
    
    out << "\n"
           "Sync max wait duration: " << game.getMaxWaitingDelta().count() << "us\n"
           "Sync average wait duration: " << game.getAvgWaitingDelta().count() << "us\n"
           "Avg freq per Worker: ";
    std::vector<std::uint64_t> freqs = game.getAvgFreqPerWorker();
    for(const auto &freq : freqs) {
        out << freq << "kHz ";
    }
    
    std::uint64_t avg_freq = std::accumulate(freqs.begin(), freqs.end(), 
                                        static_cast<std::uint64_t>(0))/freqs.size();

    out << "\n"
           "Average frequency: " << static_cast<double>(avg_freq)/1000000.0 << "GHz\n";
}

}

int main(int argc, char *argv[])
{
    if(argc != 12 && argc != 13) {
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
    const char *mapFilePath;
    std::random_device rdv{};
    unsigned seed = rdv();
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
        mapFilePath = argv[11];
        if(argc == 13) {
            seed = static_cast<unsigned>(std::strtol(argv[12], nullptr, 10));
        }
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

    WaTor::Rules rules{mapWidth, mapHeight, initFishCnt, initSharkCnt, 
                       fishBreedTime, sharkBreedTime, sharkStarveTime}; 


    std::fstream fmap(mapFilePath, std::fstream::out | std::fstream::trunc);
    if(!fmap.is_open()) {
        std::clog << "Could not create and open file: " << mapFilePath << "\n";
        return 1;
    }
    
    auto clockStart = std::chrono::steady_clock::now();
    WaTor::GameCG game(rules, exp, seed);
    auto clockEnd = std::chrono::steady_clock::now();
    std::chrono::microseconds diff = std::chrono::duration_cast<std::chrono::microseconds>(clockEnd - clockStart);
    std::clog << "Allocating and generating map took: " 
              << static_cast<double>(diff.count())/1000000.0 << " s\n";
    std::clog.flush();

    std::chrono::microseconds saveMapTime{0};
    clockStart = std::chrono::steady_clock::now();
    game.getMap().saveMap(fmap, true);
    clockEnd = std::chrono::steady_clock::now();
    saveMapTime += std::chrono::duration_cast<std::chrono::microseconds>(clockEnd-clockStart);

    for(unsigned i=0; i<iterCnt-1; ++i) {
        game.doIteration();

        clockStart = std::chrono::steady_clock::now();
        game.getMap().saveMap(fmap);
        clockEnd = std::chrono::steady_clock::now();
        saveMapTime += std::chrono::duration_cast<std::chrono::microseconds>(clockEnd-clockStart);
    }

    clockStart = std::chrono::steady_clock::now();
    fmap.close();
    clockEnd = std::chrono::steady_clock::now();
    saveMapTime += std::chrono::duration_cast<std::chrono::microseconds>(clockEnd-clockStart);

    std::clog << "Saving map took: " 
              << static_cast<double>(saveMapTime.count())/1000000.0 << " s\n";
    std::clog.flush();

    printStats(game, std::cout);


    return 0;
}
