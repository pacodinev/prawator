#include <bits/chrono.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <numeric>
#include <ostream>
#include <string>
#include <thread>
#include <random>

#include "execution_planner.hpp"

#include "posixFostream.hpp"
#include "wator/map.hpp"
#include "wator/rules.hpp"
#include "wator/simulation.hpp"
#include "utils.hpp"

#include <argparse/argparse.hpp>

#include <config.h>
#include <unistd.h>

namespace {

#ifdef WATOR_CPU_PIN 
void pinThreadToFirstCpu(const ExecutionPlanner &exp) {
    unsigned fcpu = exp.getCpuListPerNuma(0).front();

    cpu_set_t cpuMask;
    CPU_ZERO(&cpuMask);
    CPU_SET(fcpu, &cpuMask); // NOLINT
    int ret = sched_setaffinity(0, sizeof(cpuMask), &cpuMask);
    assert(ret == 0); // TODO: yea, this is bad, only for debug

    // TODO: remove theese ifdefs
#ifdef WATOR_NUMA
    if(exp.isNuma()) {
        unsigned numaNode = exp.getNumaList()[0];
        Utils::mapThisThreadStackToNuma(numaNode);
    }
#endif
}
#else 
void pinThreadToFirstCpu(const ExecutionPlanner &exp) { }
#endif

argparse::ArgumentParser buildArgParser() {
    argparse::ArgumentParser res("parwator", "beta");

    res.add_argument("--height")
        .help("Height of the ocean").required().scan<'u', unsigned>();
    res.add_argument("--width")
        .help("Width of the ocean").required().scan<'u', unsigned>();
    res.add_argument("--itercnt")
        .help("Number of chronons to simulate").required().scan<'u', unsigned>();
    res.add_argument("--fish")
        .help("The initial number of fish in the ocean, default value is 1/10-th of ocean cells").scan<'u', std::size_t>();
    res.add_argument("--sharks")
        .help("The initial number of sharks in the ocean, default value is 1/30-th of ocean cells").scan<'u', std::size_t>();
    res.add_argument("--fishbreed")
        .help("The number of chronons have to pass for fish to be able to breed").default_value(3U).scan<'u', unsigned>();
    res.add_argument("--sharkbreed")
        .help("The number of chronons have to pass for shark to be able to breed").default_value(10U).scan<'u', unsigned>();
    res.add_argument("--sharkstarve")
        .help("The number of chronons have to pass for a shark must not eat to die").default_value(3U).scan<'u', unsigned>();
    res.add_argument("--workers", "--threads")
        .help("Number of threads to run the simulation on, by default it uses all")
        .default_value(static_cast<unsigned>(std::thread::hardware_concurrency()))
        .scan<'u', unsigned>();
    res.add_argument("--disable-ht", "-H")
        .help("Disables the use of hyperthreaded cores").default_value(false).implicit_value(true);
    res.add_argument("--seed")
        .help("Provides seed for random number generation, warning: output is depending also on thread count")
        .scan<'u', unsigned>();
    res.add_argument("--output")
        .help("Where to output the saved map").default_value(std::string{"/dev/null"});
    res.add_argument("--benchmark")
        .help("Gives significantly shorted output").default_value(false).implicit_value(true);

    return res;
}

void printStats(WaTor::Simulation &game, std::chrono::microseconds mapAllocDur, 
                std::chrono::microseconds mapSaveDur, bool isBench) {
    if(!isBench) {
        std::cout << "Allocating map and randomizing took: " 
                  << static_cast<double>(mapAllocDur.count())/1000000 << " s\n"
                     "Saving map took: " 
                  << static_cast<double>(mapSaveDur.count())/1000000 << " s\n"
                     "All simulation time: " 
                  << static_cast<double>(game.getAllRunTime().count())/1000000 << " s\n"
                     "Average CPU clock frequency :" << static_cast<double>(game.getAvgFreq())/1000 << "MHz\n";
        double percentWaiting = static_cast<double>(game.getWeightedWaitingTime().count());
        percentWaiting /= static_cast<double>(game.getAllRunTime().count());
        percentWaiting *= 100;
        std::cout << "Threads waiting to sync resulted in " << percentWaiting 
                  << "% of the time being wasted!\n";
    } else {
        std::cout << static_cast<double>(game.getAllRunTime().count())/1000000 << " "
                  << static_cast<double>(game.getAvgFreq())/1000 << " ";

        double percentWaiting = static_cast<double>(game.getWeightedWaitingTime().count());
        percentWaiting /= static_cast<double>(game.getAllRunTime().count());
        percentWaiting *= 100;

        std::cout << percentWaiting << "\n";
    }
}

}


int main(int argc, char *argv[])
{
    argparse::ArgumentParser arg = buildArgParser();
    arg.parse_args(argc, argv);

    ExecutionPlanner::initInst(arg.get<unsigned>("--workers"), 
                               !arg.get<bool>("--disable-ht"));

    const ExecutionPlanner &exp = ExecutionPlanner::getInst();

    pinThreadToFirstCpu(exp);

    if(!arg.get<bool>("--benchmark")) {
        exp.printStats(std::clog);
    }

    std::size_t mapSize = static_cast<std::size_t>(arg.get<unsigned>("--height")) *
                          arg.get<unsigned>("--width");
    std::size_t defaultFishCnt = std::max<std::size_t>(mapSize/10, 1);
    std::size_t defaultSharkCnt = std::max<std::size_t>(mapSize/30, 1);
    std::size_t initFishCnt = arg.present<std::size_t>("--fish") ? arg.get<std::size_t>("--fish") : defaultFishCnt;
    std::size_t initSharkCnt = arg.present<std::size_t>("--sharks") ? arg.get<std::size_t>("--sharks") : defaultSharkCnt;
    WaTor::Rules rules{arg.get<unsigned>("--height"), 
                       arg.get<unsigned>("--width"), 
                       initFishCnt, initSharkCnt, 
                       arg.get<unsigned>("--fishbreed"),
                       arg.get<unsigned>("--sharkbreed"),
                       arg.get<unsigned>("--sharkstarve")}; 
    
    const char* mapFilePath = arg.get("--output").c_str();

#ifdef __unix__
    PosixFostream fmap{mapFilePath, O_CREAT | O_TRUNC | O_WRONLY, 0777, 1U << 17};
#else
    std::fstream fmap(mapFilePath, std::fstream::out | std::fstream::trunc);
    if(!fmap.is_open()) {
        std::clog << "Could not create and open file: " << mapFilePath << "\n";
        return 1;
    }
#endif // __unix
    
    std::random_device rnd;
    auto clockStart = std::chrono::steady_clock::now();
    unsigned seed = arg.present<unsigned>("--seed") ? arg.get<unsigned>("--seed") : rnd();
    WaTor::Simulation game(rules, exp, seed);
    auto clockEnd = std::chrono::steady_clock::now();
    std::chrono::microseconds mapAllocDur = std::chrono::duration_cast<std::chrono::microseconds>(clockEnd - clockStart);

    std::chrono::microseconds saveMapDur{0};
    clockStart = std::chrono::steady_clock::now();
#ifdef __unix__
        game.getMap().saveMap(fmap, true);
#else
        game.getMap().saveMap(fmap, true);
#endif // __unix__
    clockEnd = std::chrono::steady_clock::now();
    saveMapDur += std::chrono::duration_cast<std::chrono::microseconds>(clockEnd-clockStart);

    unsigned iterCnt = arg.get<unsigned>("--itercnt");
    for(unsigned i=0; i<iterCnt-1; ++i) {
        game.doIteration();

        clockStart = std::chrono::steady_clock::now();
        game.getMap().saveMap(fmap);
        clockEnd = std::chrono::steady_clock::now();
        saveMapDur += std::chrono::duration_cast<std::chrono::microseconds>(clockEnd-clockStart);
    }

    clockStart = std::chrono::steady_clock::now();
#ifdef __unix__
    fmap.flush();
#else
    fmap.close();
#endif // __unix__
    clockEnd = std::chrono::steady_clock::now();
    saveMapDur += std::chrono::duration_cast<std::chrono::microseconds>(clockEnd-clockStart);

    printStats(game, mapAllocDur, saveMapDur, arg.get<bool>("--benchmark"));


    return 0;
}
