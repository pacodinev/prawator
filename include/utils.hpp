#pragma once

#include <filesystem>
#include <fstream>
#include <string>

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
}
