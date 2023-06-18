#include <iostream>
#include <thread>
#include <random>

#include "execution_planner.hpp"

#include "wator_map.hpp"
#include "wator_rules.hpp"
#include "wator_gamecg.hpp"

int main()
{
    //ExecutionPlanner::initInst(std::thread::hardware_concurrency()/2, false);
    ExecutionPlanner::initInst(9, true);

    const ExecutionPlanner &exp = ExecutionPlanner::getInst();

    exp.printStats(std::clog);

    WaTor::Rules rules{1920, 1337, 4769, 476, 3, 10, 3};

    std::random_device rdv{};

    // WaTor::Map map(rules, exp, rdv());
    
    WaTor::GameCG game(rules, exp, rdv());

    game.doIteration();

    return 0;
}
