#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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
}
