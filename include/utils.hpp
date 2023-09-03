#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sched.h>
#include <string>
#include <system_error>
#include <utility>
#include <cerrno>

#include <pthread.h>

#include <config.h>
#include <vector>

#ifdef WATOR_NUMA
#include <numaif.h>
#include <numa.h>
#include <unistd.h>
#endif

namespace Utils {
    // TODO: put in source file
    // 0 if cannot read
    // return frequency in kHz
    [[nodiscard]] inline std::uint64_t readCurCpuFreq(unsigned cpu) { 
        using namespace std::filesystem;

        std::uint64_t ret {0};

        path cpuinfoCpuFreqPath{"/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cpufreq/cpuinfo_cur_freq"};
        if(is_regular_file(cpuinfoCpuFreqPath)) {
            std::fstream fin{cpuinfoCpuFreqPath.c_str(), std::fstream::in};
            fin >> ret;
            if(!fin.fail()) {
                return ret;
            }
        }

        path scalingCpuFreqPath{"/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cpufreq/scaling_max_freq"};
        if(is_regular_file(scalingCpuFreqPath)) {
            std::fstream fin{scalingCpuFreqPath.c_str(), std::fstream::in};
            fin >> ret;
            if(!fin.fail()) {
                return ret;
            }
        }

        return ret; // 0 in case of error
    }

    [[nodiscard]] inline auto getThisThreadStack() 
            -> std::pair<void*, std::size_t> {
        pthread_t selfTid = pthread_self();

        std::pair<void*, std::size_t> res;

        int ret = 0;

        pthread_attr_t tattr;
        ret = pthread_getattr_np(selfTid, &tattr);
        if (ret != 0) {
            throw std::system_error(errno, std::system_category(), "pthread_getattr_np");
        }

        ret = pthread_attr_getstack(&tattr, &res.first, &res.second);
        if (ret != 0) {
            pthread_attr_destroy(&tattr);
            throw std::system_error(errno, std::system_category(), "pthrad_attr_getstack");
        }

        pthread_attr_destroy(&tattr);

        return res;
    }

#ifdef WATOR_NUMA
    inline void mapThisThreadStackToNuma(unsigned numaNode) {
        if(numa_available() < 0) {
            return;
        }

        numa_set_localalloc();

        std::pair<void*, std::size_t> thdStack = getThisThreadStack();

        constexpr std::size_t maxStackUsage = static_cast<const std::size_t>(32)*1024*1024;
        thdStack.second = std::min<std::size_t>(thdStack.second, maxStackUsage); // we probably will not use more than 32MiB of stack

        long pageSizeS = sysconf(_SC_PAGESIZE);
        if(pageSizeS < 0) {
            throw std::system_error(errno, std::system_category(), "sysconf(_SC_PAGESIZE)");
        }
        std::size_t pageSize = static_cast<std::size_t>(pageSizeS);

        thdStack.second = (thdStack.second + (pageSize - 1))/pageSize*pageSize; // round to nearest page

        std::size_t numPages = thdStack.second/pageSize;
        std::vector<void*> pages(numPages);
        std::vector<int> nodes(numPages, static_cast<int>(numaNode));
        std::vector<int> status(numPages, std::numeric_limits<int>::min());

        for(std::size_t page=0; page<numPages; ++page) {
            pages[page] = static_cast<char*>(thdStack.first) + page*pageSize;
        }

        int ret = numa_move_pages(0, numPages, pages.data(), nodes.data(), status.data(), MPOL_MF_MOVE); // NOLINT
        if(ret < 0) {
            throw std::system_error(errno, std::system_category(), "numa_move_pages");
        }

        for(std::size_t page=0; page<numPages; ++page) {
            if(status[page] < 0 && status[page] != -EFAULT) {
                std::clog << "Failed to move page: " << pages[page] << ", reason: " << status[page] << std::endl;
            } else if(status[page] > 0 && static_cast<unsigned>(status[page]) != numaNode) {
                std::clog << "Move for page: " << pages[page] << "requested: " << numaNode
                          << "but put instead on: " << status[page] << std::endl;
            }
        }
    }
#else 
    inline void mapThisThreadStackToNuma(unsigned /*numaNode*/) {}
#endif

#if !defined(NDEBUG) && defined (WATOR_NUMA)
    inline void assertMemLocalFunc(const void* ptr, std::size_t size, const char file[], unsigned line) { // NOLINT
        (void)size; // TODO: use size

        if(numa_available() < 0) {
            return;
        }

        int numaNode = std::numeric_limits<int>::max();
        int ret = numa_move_pages(0, 1, const_cast<void**>(&ptr), nullptr, &numaNode, 0); // NOLINT
        if(ret < 0 || numaNode < 0 || numaNode == std::numeric_limits<int>::max()) {
            std::clog << "assertMemLocal: numa_move_pages failed" << std::endl;
            return;
        }

        cpu_set_t my_set;
        CPU_ZERO(&my_set);
        ret = sched_getaffinity(0, sizeof(cpu_set_t), &my_set);
        if(ret < 0) {
            std::clog << "assertMemLocal: sched_getaffinity failed" << std::endl;
            return;
        }

        if(CPU_COUNT(&my_set) != 1) {
            std::clog << "assertMemLocal: thread is not pinned to one core! "
                      << file << ":" << line << std::endl;
            return;
        }

        unsigned pinnedCpu = std::numeric_limits<unsigned>::max();
        for(unsigned cpu=0; cpu<CPU_SETSIZE; ++cpu) {
            if(CPU_ISSET(cpu, &my_set)) {
                pinnedCpu = cpu;
                break;
            }
        }
        if(pinnedCpu == std::numeric_limits<unsigned>::max()) { return; }

        int cpuNumaNode = numa_node_of_cpu(static_cast<int>(pinnedCpu));
        if(cpuNumaNode < 0) { return; }
        if(numaNode != cpuNumaNode) {
            std::string msg = "Non local access to numa memory ";
            msg += std::to_string(reinterpret_cast<std::size_t>(ptr)); // NOLINT
            msg += " memory node ";
            msg += std::to_string(numaNode);
            msg += " task node ";
            msg += std::to_string(cpuNumaNode);
            msg += " at ";
            msg += file;
            msg += ":";
            msg += std::to_string(line);
            msg += "\n";
            std::clog << msg;
        }
    }

    template<class T>
    inline void assertMemLocalFunc(const T &val, const char file[], unsigned line) { // NOLINT
        assertMemLocalFunc(&val, sizeof(T), file, line);
    }

    #define assertMemLocal(val) Utils::assertMemLocalFunc(val, __FILE__, __LINE__)  // NOLINT
    #define assertMemLocalPtr(ptr, size) Utils::assertMemLocalFunc(ptr, size, __FILE__, __LINE__)  // NOLINT
#else
    #define assertMemLocal(val)
    #define assertMemLocalPtr(ptr, size)
#endif
}
