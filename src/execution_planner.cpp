#include "execution_planner.hpp"

#include "config.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <locale>
#include <set>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <map>

#ifdef WATOR_NUMA
#include <numa.h>
#endif

static constexpr const char* linuxSysFSCPUPath{"/sys/devices/system/cpu"};

std::optional<ExecutionPlanner> ExecutionPlanner::instance; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

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

}

ExecutionPlanner::ExecutionPlanner(std::vector<unsigned> numaList, 
                              std::vector<std::vector<unsigned>> cpuPerNuma) 
    : m_numaList(std::move(numaList)), m_cpuPerNuma(std::move(cpuPerNuma)),
      m_cpuCnt{0}, m_isNuma{m_numaList.size() > 1 || (m_numaList.size() == 1 && m_numaList.back() != 0)} {
    assert(!m_numaList.empty());
    assert(!m_cpuPerNuma.empty());
    assert(m_numaList.size() == m_cpuPerNuma.size());
    assert(std::is_sorted(m_numaList.begin(), m_numaList.end()) == true);
    // m_cpuCnt is zero
    for(unsigned i=0; i<m_numaList.size(); ++i) {
        const std::vector<unsigned> &curCpuList = m_cpuPerNuma[i];
        m_cpuCnt += curCpuList.size();
        assert(std::is_sorted(curCpuList.begin(), curCpuList.end()) == true);
    }
#ifndef NDEBUG
    std::vector<unsigned> allCpuList;
    for(unsigned i=0; i<m_numaList.size(); ++i) {
        const std::vector<unsigned> &curCpuList = m_cpuPerNuma[i];
        assert(std::is_sorted(curCpuList.begin(), curCpuList.end()) == true);
        std::copy(curCpuList.begin(), curCpuList.end(), std::back_inserter(allCpuList));
    }

    std::sort(allCpuList.begin(), allCpuList.end());
    assert(areDifferent(allCpuList.begin(), allCpuList.end()));
#endif
    
}


namespace {
    std::vector<unsigned> getCPUList() 
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

    unsigned getPhysicalPackageId(unsigned cpu) {
        const std::string cpuPPIDPath{
            "/sys/devices/system/cpu/cpu" +
            std::to_string(cpu) +
            "/topology/physical_package_id"
        };
        std::fstream cpuPPIDFile{cpuPPIDPath, std::fstream::in};
        unsigned PPID;
        cpuPPIDFile >> PPID;
        if(cpuPPIDFile.fail()) {
            throw std::runtime_error("Failed to read " + cpuPPIDPath);
        }
        cpuPPIDFile.close();

        return PPID;
    }

    unsigned getCoreID(unsigned cpu) {
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

        return coreID;
    }
}

ExecutionPlanner::NumaList ExecutionPlanner::getCpuArchitecture(bool isNuma) {
    NumaList res;
#ifdef WATOR_NUMA
    if(isNuma) {
        res.resize(numa_max_node()+1);
    } else { 
        res.resize(1);
    }
#else
    res.resize(1);
#endif

    std::vector<unsigned> cpuList = getCPUList();

    // physical_package_id, core_id
    using PpidCoreID = std::pair<unsigned, unsigned>;

    std::map<PpidCoreID, ThreadList*> cpuToSiblingList;

    for(unsigned cpu : cpuList) {
        PpidCoreID curCpuIDs = std::make_pair(getPhysicalPackageId(cpu),
                                              getCoreID(cpu));
        if(cpuToSiblingList.count(curCpuIDs) > 0) {
            ThreadList &curThdList = *cpuToSiblingList[curCpuIDs];
            curThdList.push_back(cpu);
        } else {
            unsigned curNuma = 0;
#ifdef WATOR_NUMA 
            if(isNuma) {
                curNuma = numa_node_of_cpu(cpu);
            }
#endif

            res.at(curNuma).push_back({cpu});
            ThreadList *curThdList = &res.at(curNuma).back();
            cpuToSiblingList[curCpuIDs] = curThdList;
        }
    }

    return res;
}

void ExecutionPlanner::buildFromCPUList(const NumaList &cpuArch, std::vector<unsigned> &cpuList) {
    m_cpuPerNuma.resize(cpuArch.size());
    m_cpuCnt = static_cast<unsigned>(cpuList.size());
    for(unsigned numa=0; numa<cpuArch.size(); ++numa) {
        bool firstUseNuma{true};
        for(unsigned cpu=0; cpu<cpuArch[numa].size(); ++cpu) {
            for(unsigned thread=0; thread<cpuArch[numa][cpu].size(); ++thread) {
                unsigned curCpu = cpuArch[numa][cpu][thread];

                if(std::binary_search(cpuList.begin(), cpuList.end(), curCpu)) {
                    if(firstUseNuma) {
                        firstUseNuma = false;
                        m_numaList.push_back(numa);
                    }

                    m_cpuPerNuma[numa].push_back(curCpu);
                }
            }
        }
    }
}

void ExecutionPlanner::buildFromCPUArch(const NumaList &cpuArch, unsigned numThreads, bool enableHT) {
    std::vector<std::vector<ThreadList::const_iterator>> usedThreads;
    usedThreads.resize(cpuArch.size());
    for(std::size_t i=0; i<cpuArch.size(); ++i) {
        usedThreads[i].resize(cpuArch[i].size());
        for(std::size_t j=0; j<cpuArch[i].size(); ++j) {
            usedThreads[i][j] = cpuArch[i][j].cbegin();
        }
    }

    std::vector<unsigned> useCpus;

    while(useCpus.size() < numThreads) {
        bool addedCPU{false};
        for(unsigned numa=0; numa<cpuArch.size(); ++numa) {
            for(unsigned cpu=0; cpu<cpuArch[numa].size(); ++cpu) {
                if(usedThreads[numa][cpu] == cpuArch[numa][cpu].cend()) {
                    continue;
                }
                if(!enableHT && usedThreads[numa][cpu] != cpuArch[numa][cpu].cbegin()) {
                    continue;
                }

                useCpus.push_back(*usedThreads[numa][cpu]);
                ++usedThreads[numa][cpu];
                addedCPU = true;

                if(useCpus.size() == numThreads) {
                    break;
                }
            }
            if(useCpus.size() == numThreads) {
                break;
            }
        }

        if(!addedCPU && useCpus.size() < numThreads) {
            throw std::runtime_error("Machine has lower number of CPUs than requested");
        }
    }

    std::sort(useCpus.begin(), useCpus.end());

    buildFromCPUList(cpuArch, useCpus);
}

// numThreads should be atleast 1
ExecutionPlanner::ExecutionPlanner(unsigned numThreads, bool enableHT) {
    assert(numThreads > 0);

#ifdef WATOR_NUMA 
    m_isNuma = (numa_available() >= 0); // NOLINT
    numa_exit_on_warn = 1;
    numa_exit_on_error = 1;
#ifdef WATOR_NUMA_OPTIMIZE
    if(m_isNuma && numa_max_node() == 0) { // TODO: check errors
        // just lie!
        m_isNuma = false;
    }
#endif
#else
    m_isNuma = false;
#endif

    NumaList cpuArch = getCpuArchitecture(m_isNuma);

    buildFromCPUArch(cpuArch, numThreads, enableHT);
}

void ExecutionPlanner::printStats(std::ostream &out) const {
    out << "ExecutionPlanner: NUMA is " << (isNuma() ? "enabled\n" : "NOT supported\n");
        
    for(unsigned numa : getNumaList()) {
        out << "ExecutionPlanner: ";
        if(isNuma()) {
            out << "NUMA" << numa << ' ';
        }
        for(unsigned cpu: getCpuListPerNuma(numa)) {
            out << " CPU" << cpu;
        }
        out << '\n';
    }
}


