#include "execution_planner.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <locale>
#include <stdexcept>
#include <vector>
#include <fstream>

#include <numa.h>

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
    res.m_cpuToCoreID.resize(lastCPU+1, NOCPU);

    for(const unsigned cpu : cpuList) {
        const std::string cpuCoreIDPath{
            "/sys/devices/system/cpu/cpu" +
            std::to_string(cpu) +
            "/topology/core_id"
        };
        std::fstream cpuCoreIDFile{cpuCoreIDPath, std::fstream::in};
        unsigned coreID;
        cpuCoreIDFile >> coreID;
        if(cpuCoreIDFile.fail()) {
            throw std::runtime_error("Failed to read " + cpuCoreIDPath);
        }
        cpuCoreIDFile.close();

        res.m_cpuToCoreID[cpu] = coreID;
    }

    res.m_cpuToCoreID.shrink_to_fit();
    return res;
}

void ExecutionPlanner::solveNoNumaHt(unsigned numThreads)
{
    std::vector<unsigned> cpuList = getCPUList();

    if(numThreads > cpuList.size()) {
        throw std::runtime_error("Machine has lower number of CPUs than requested");
    }

    m_numaList.clear();
    m_numaList.shrink_to_fit();

    m_cpuPerNuma.resize(1);
    m_cpuPerNuma[0].assign(cpuList.begin(), cpuList.begin()+numThreads);
}

void ExecutionPlanner::solveNoNumaNoHt(unsigned numThreads)
{
    std::vector<unsigned> cpuList = getCPUList();

    if(numThreads > cpuList.size()) {
        throw std::runtime_error("Machine has lower number of CPUs than requested");
    }

    m_numaList.clear();
    m_numaList.shrink_to_fit();

    HTInfo hti = HTInfo::getInfo(cpuList);
    
    unsigned maxCoreID = 
        *std::max_element(hti.m_cpuToCoreID.begin(), hti.m_cpuToCoreID.end(), 
            [](unsigned lhs, unsigned rhs) {
                if(lhs == HTInfo::NOCPU && rhs == HTInfo::NOCPU) { return false; }
                if(rhs == HTInfo::NOCPU) { return false; }
                if(lhs == HTInfo::NOCPU) { return true; }
                return lhs<rhs;
            });

    std::vector<bool> usedCoreID;
    usedCoreID.resize(maxCoreID+1, false);

    m_cpuPerNuma.resize(1);
    m_cpuPerNuma[0].reserve(numThreads);

    for(unsigned cpu : cpuList) {
        unsigned coreID = hti.m_cpuToCoreID[cpu];
        assert(coreID != HTInfo::NOCPU);
        if(usedCoreID[coreID]) { continue; }
        usedCoreID[coreID] = true;
        m_cpuPerNuma[0].push_back(cpu);
        if(m_cpuPerNuma[0].size() == numThreads) {
            break; 
        }
    }

    if(m_cpuPerNuma[0].size() != numThreads) {
        throw std::runtime_error("Machine has lower number of CPUs than requested");
    }
}

void ExecutionPlanner::solveNumaHt(unsigned numThreads)
{
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
    std::vector<unsigned> cpuList = getCPUList();
    HTInfo hti = HTInfo::getInfo(cpuList);
    
    unsigned maxCoreID = 
        *std::max_element(hti.m_cpuToCoreID.begin(), hti.m_cpuToCoreID.end(), 
            [](unsigned lhs, unsigned rhs) {
                if(lhs == HTInfo::NOCPU && rhs == HTInfo::NOCPU) { return false; }
                if(rhs == HTInfo::NOCPU) { return false; }
                if(lhs == HTInfo::NOCPU) { return true; }
                return lhs<rhs;
            });

    std::vector<bool> usedCoreID;
    usedCoreID.resize(maxCoreID+1, false);

    numa_exit_on_error = 1;

    int maxNumaNode = numa_max_node();

    m_cpuPerNuma.resize(maxNumaNode+1);

    unsigned curAllocCpus = 0;

    struct bitmask *cpuMask = numa_allocate_cpumask();

    for(int numaNode=0; numaNode<=maxNumaNode; ++numaNode) {
        numa_node_to_cpus(numaNode, cpuMask);
        for(unsigned cpu : cpuList) {
            if(numa_bitmask_isbitset(cpuMask, static_cast<unsigned>(cpu)) == 1 &&
                !usedCoreID[hti.m_cpuToCoreID[cpu]]) {
                usedCoreID[hti.m_cpuToCoreID[cpu]] = true;
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

ExecutionPlanner::ExecutionPlanner(unsigned numThreads, bool enableHT) {
    if(numa_available() < 0 && enableHT) {
        solveNoNumaHt(numThreads);
    } else if(numa_available() < 0 && !enableHT) {
        solveNoNumaNoHt(numThreads);
    } else if(numa_available() >= 0 && enableHT) {
        solveNumaHt(numThreads);
    } else {
        solveNumaNoHt(numThreads);
    }

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


