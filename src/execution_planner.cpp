#include "execution_planner.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <locale>
#include <set>
#include <stdexcept>
#include <vector>
#include <fstream>

#include <numa.h>

#include <config.h>

static constexpr const char* linuxSysFSCPUPath{"/sys/devices/system/cpu"};

std::optional<ExecutionPlanner> ExecutionPlanner::instance; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

std::vector<unsigned> ExecutionPlanner::getCPUList() 
{
    std::vector<unsigned> res;
    using namespace std::filesystem;
    const path cpuSysFS{linuxSysFSCPUPath};

    for(const auto& dirEntry : directory_iterator{cpuSysFS})
    {
        if(!dirEntry.is_directory()) { continue; }

        const std::string &dirEntryName{dirEntry.path().filename().string()};
        if(dirEntryName.find("cpu") != 0) { continue; }

        const std::string cpuIDStr{dirEntryName.substr(3)};

        std::locale loc;
        auto isDigit = [&loc](char chr) -> bool {
            return std::isdigit(chr, loc);
        };

        if(!std::all_of(cpuIDStr.begin(), cpuIDStr.end(), isDigit)) { continue; }
        
        unsigned long cpuID{std::stoul(cpuIDStr)};
        res.push_back(static_cast<unsigned>(cpuID));

    }

    res.shrink_to_fit();
    std::sort(res.begin(), res.end());

    return res;
}

auto ExecutionPlanner::HTInfo::getInfo(const std::vector<unsigned> &cpuList) 
    -> ExecutionPlanner::HTInfo {
    ExecutionPlanner::HTInfo res;
    const unsigned lastCPU = cpuList.back();
    res.m_cpiIDS.resize(lastCPU+1, NOCPU);

    for(const unsigned cpu : cpuList) {
        const std::string cpuPPIDPath{
            "/sys/devices/system/cpu/cpu" +
            std::to_string(cpu) +
            "/topology/physical_package_id"
        };
        const std::string cpuCoreIDPath{
            "/sys/devices/system/cpu/cpu" +
            std::to_string(cpu) +
            "/topology/core_id"
        };
        std::fstream cpuPPIDFile{cpuPPIDPath, std::fstream::in};
        std::fstream cpuCoreIDFile{cpuCoreIDPath, std::fstream::in};
        unsigned PPID;
        unsigned coreID;
        cpuPPIDFile >> PPID;
        cpuCoreIDFile >> coreID;
        if(cpuPPIDFile.fail() || cpuCoreIDFile.fail()) {
            throw std::runtime_error("Failed to read " + cpuPPIDPath + " or " + cpuCoreIDPath);
        }
        cpuCoreIDFile.close();

        res.m_cpiIDS[cpu] = {PPID, coreID};
    }

    res.m_cpiIDS.shrink_to_fit();
    return res;
}

void ExecutionPlanner::solveNoNumaHt(unsigned numThreads)
{
    m_isNuma = false;
    std::vector<unsigned> cpuList = getCPUList();

    if(numThreads > cpuList.size()) {
        throw std::runtime_error("Machine has lower number of CPUs than requested");
    }

    // add a virtual NUMA node
    m_numaList.push_back(0);
    m_numaList.shrink_to_fit();

    m_cpuPerNuma.resize(1);
    m_cpuPerNuma[0].assign(cpuList.begin(), cpuList.begin()+numThreads);
}

void ExecutionPlanner::solveNoNumaNoHt(unsigned numThreads)
{
    m_isNuma = false;
    std::vector<unsigned> cpuList = getCPUList();

    if(numThreads > cpuList.size()) {
        throw std::runtime_error("Machine has lower number of CPUs than requested");
    }

    // add a virtual NUMA node
    m_numaList.push_back(0);
    m_numaList.shrink_to_fit();

    HTInfo hti = HTInfo::getInfo(cpuList);

    std::set<HTInfo::CpuIDS> used;

    m_cpuPerNuma.resize(1);
    m_cpuPerNuma[0].reserve(numThreads);

    for(unsigned cpu : cpuList) {
        HTInfo::CpuIDS cpuIDs = hti.m_cpiIDS[cpu];
        assert(cpuIDs != HTInfo::NOCPU);
        if(used.count(cpuIDs) > 0) { continue; }
        used.insert(cpuIDs);
        m_cpuPerNuma[0].push_back(cpu);
        if(m_cpuPerNuma[0].size() == numThreads) {
            break; 
        }
    }

    if(m_cpuPerNuma[0].size() != numThreads) {
        throw std::runtime_error("Machine has lower number of CPUs than requested");
    }
}

#ifdef WATOR_NUMA

void ExecutionPlanner::solveNumaHt(unsigned numThreads)
{
#ifdef WATOR_NUMA_OPTIMIZE 
    if(numa_num_task_nodes() <= 1) {
        // just lie :)
        solveNoNumaHt(numThreads);
        return;
    }
#endif

    m_isNuma = true;
    numa_exit_on_error = 1;

    int maxNumaNode = numa_max_node();
    std::vector<unsigned> cpuList = getCPUList();

    m_cpuPerNuma.resize(maxNumaNode+1);

    unsigned curAllocCpus = 0;

    struct bitmask *cpuMask = numa_allocate_cpumask();

    for(int numaNode=0; numaNode<=maxNumaNode; ++numaNode) {
        numa_node_to_cpus(numaNode, cpuMask);
        for(unsigned cpu : cpuList) {
            if(numa_bitmask_isbitset(cpuMask, static_cast<unsigned>(cpu)) == 1) {
                m_cpuPerNuma[numaNode].push_back(cpu);
                ++curAllocCpus;
                if(curAllocCpus == numThreads) {
                    break;
                }
            }
        }
        if(curAllocCpus == numThreads) {
            break;
        }
    }

    numa_bitmask_free(cpuMask);

    if(curAllocCpus != numThreads) {
        throw std::runtime_error("Machine has lower number of CPUs than requested");
    }

    for(unsigned i=0; i<m_cpuPerNuma.size(); ++i) {
        std::vector<unsigned> &numa = m_cpuPerNuma[i];
        numa.shrink_to_fit();
        if(!numa.empty()) {
            m_numaList.push_back(i);
        }
    }

    m_numaList.shrink_to_fit();
}

void ExecutionPlanner::solveNumaNoHt(unsigned numThreads)
{
#ifdef WATOR_NUMA_OPTIMIZE 
    if(numa_num_task_nodes() <= 1) {
        // just lie
        solveNoNumaNoHt(numThreads);
        return;
    }
#endif

    m_isNuma = true;
    std::vector<unsigned> cpuList = getCPUList();
    HTInfo hti = HTInfo::getInfo(cpuList);
    
    std::set<HTInfo::CpuIDS> used;

    numa_exit_on_error = 1;

    int maxNumaNode = numa_max_node();

    m_cpuPerNuma.resize(maxNumaNode+1);

    unsigned curAllocCpus = 0;

    struct bitmask *cpuMask = numa_allocate_cpumask();

    for(int numaNode=0; numaNode<=maxNumaNode; ++numaNode) {
        numa_node_to_cpus(numaNode, cpuMask);
        for(unsigned cpu : cpuList) {
            HTInfo::CpuIDS cpuIDs = hti.m_cpiIDS[cpu];
            assert(cpuIDs != HTInfo::NOCPU);
            if(numa_bitmask_isbitset(cpuMask, static_cast<unsigned>(cpu)) == 1 &&
                used.count(cpuIDs) == 0) {
                used.insert(cpuIDs);
                m_cpuPerNuma[numaNode].push_back(cpu);
                ++curAllocCpus;
                if(curAllocCpus == numThreads) {
                    break;
                }
            }
        }
        if(curAllocCpus == numThreads) {
            break;
        }
    }

    numa_bitmask_free(cpuMask);

    if(curAllocCpus != numThreads) {
        throw std::runtime_error("Machine has lower number of CPUs than requested");
    }

    for(unsigned i=0; i<m_cpuPerNuma.size(); ++i) {
        std::vector<unsigned> &numa = m_cpuPerNuma[i];
        numa.shrink_to_fit();
        if(!numa.empty()) {
            m_numaList.push_back(i);
        }
    }

    m_numaList.shrink_to_fit();
}

#else //WATOR_NUMA
void ExecutionPlanner::solveNumaHt(unsigned numThreads) {
    solveNoNumaHt(numThreads);
}

void ExecutionPlanner::solveNumaNoHt(unsigned numThreads) {
    solveNoNumaNoHt(numThreads);
}
#endif

ExecutionPlanner::ExecutionPlanner(unsigned numThreads, bool enableHT) {
#ifdef WATOR_NUMA
    if(numa_available() < 0 && enableHT) {
        solveNoNumaHt(numThreads);
    } else if(numa_available() < 0 && !enableHT) {
        solveNoNumaNoHt(numThreads);
    } else if(numa_available() >= 0 && enableHT) {
        solveNumaHt(numThreads);
    } else {
        solveNumaNoHt(numThreads);
    }
#else 
    if(enableHT) {
        solveNoNumaHt(numThreads);
    } else { // if(!enableHT) 
        solveNoNumaNoHt(numThreads);
    }
#endif

    this->m_cpuCnt = numThreads;
}

void ExecutionPlanner::printStats(std::ostream &out) const {
    if(isNuma())
    {
        out << "ExecutionPlanner: NUMA is enabled!\n";
        
        for(unsigned numa : getNumaList()) {
            out << "ExecutionPlanner: NUMA" << numa 
                << ' ';
            for(unsigned cpu: getCpuListPerNuma(numa)) {
                out << " CPU" << cpu;
            }
            out << '\n';
        }
    } else {
        out << "ExecutionPlanner: NUMA is NOT supported!\n";
        
        out << "ExecutionPlanner:";
        for(unsigned cpu: getCpuListPerNuma(0)) {
            out << " CPU" << cpu;
        }
        out << '\n';
    }
}


