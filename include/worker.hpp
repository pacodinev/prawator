#pragma once

#include <sched.h>

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <thread>
#include <optional>
#include <mutex>
#include <string>
#include <filesystem>

#include <config.h>

template<class T>
class Worker {
private:
    std::optional<T> m_work;
    mutable std::mutex m_lock;
    mutable std::condition_variable m_waitForTask, m_taskReady;
    std::optional<std::thread> m_thread;
    std::chrono::microseconds m_runDuration = std::chrono::microseconds{0};
    std::uint64_t m_avgFreq = 0; // in kHz
    unsigned m_cpuPin = 0;
    bool m_isRunnig = false, m_timeToDie = false;

    // 0 if cannot read
    [[nodiscard]] std::uint64_t readCurCpuFreq() const { 
        using namespace std::filesystem;

        std::uint64_t ret {0};

        path cpuinfoCpuFreqPath{"/sys/devices/system/cpu/cpu" + std::to_string(m_cpuPin) + "/cpufreq/cpuinfo_cur_freq"};
        if(is_regular_file(cpuinfoCpuFreqPath)) {
            std::fstream fin{cpuinfoCpuFreqPath.c_str(), std::fstream::in};
            fin >> ret;
            if(!fin.fail()) {
                return ret;
            }
        }

        path scalingCpuFreqPath{"/sys/devices/system/cpu/cpu" + std::to_string(m_cpuPin) + "/cpufreq/scaling_max_freq"};
        if(is_regular_file(scalingCpuFreqPath)) {
            std::fstream fin{scalingCpuFreqPath.c_str(), std::fstream::in};
            fin >> ret;
            if(!fin.fail()) {
                return ret;
            }
        }

        return ret; // 0 in case of error
    }

    void calcAvgFreq(std::chrono::microseconds newTime) {

        std::uint64_t curCpuFreq = readCurCpuFreq();
        
        if(curCpuFreq == 0) { return; }

        std::chrono::microseconds allTime = m_runDuration + newTime;
        
        std::uint64_t newAvgFreq = (m_avgFreq*m_runDuration.count() + curCpuFreq*newTime.count())
                                    /allTime.count();

        m_avgFreq = newAvgFreq;
    }

#ifdef WATOR_CPU_PIN
    void setCpuMask() noexcept {
        cpu_set_t cpuMask;
        CPU_ZERO(&cpuMask);
        CPU_SET(m_cpuPin, &cpuMask); // NOLINT
        int ret = sched_setaffinity(0, sizeof(cpuMask), &cpuMask);
        assert(ret == 0); // TODO: yea, this is bad, only for debug
    }
#else
    void setCpuMask() noexcept { }
#endif

    void workFn() noexcept {
        setCpuMask();

        std::unique_lock<std::mutex> ulk(m_lock);

        while(true) {
            while(!m_work.has_value() && !m_timeToDie) {
                m_waitForTask.wait(ulk);
            }
            if(!m_work.has_value() && m_timeToDie) {
                return;
            }

            // there is work
            m_isRunnig = true;
            ulk.unlock();

            auto clockStart = std::chrono::steady_clock::now();
            try {
                m_work->operator()();
            } catch(...) { /* do nothing */ }
            auto clockEnd = std::chrono::steady_clock::now();

            // work is finished
            ulk.lock();
            std::chrono::microseconds newTime = std::chrono::duration_cast<std::chrono::microseconds>(clockEnd - clockStart);
            m_runDuration += newTime;
            m_isRunnig = false;
            m_work.reset();

            m_taskReady.notify_all();
        }
    }
    
    static void workerCall(Worker<T> *work) noexcept {
        work->workFn();
    }

public:

    Worker() = default;

    explicit Worker(unsigned cpuPin) : m_cpuPin(cpuPin) {
        std::unique_lock<std::mutex> ulk(m_lock);

        m_thread = std::thread(workerCall, this);
    }

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    // Worker(Worker&&) = default; // NOLINT
    // Worker& operator=(Worker&&) = default; // NOLINT
    
    ~Worker() noexcept {
        std::unique_lock<std::mutex> ulk(m_lock);

        if(!m_thread.has_value()) {
            return; 
        }

        std::thread &thd = m_thread.value();

        assert(thd.joinable());

        m_timeToDie = true;
        m_waitForTask.notify_one();
        ulk.unlock();

        thd.join();
    }

    void pushWork(T work) {
        std::unique_lock<std::mutex> ulk(m_lock);
        while(m_isRunnig) {
            m_taskReady.wait(ulk);
        }

        m_work.emplace(std::move(work));
        m_waitForTask.notify_one();
    }

    void waitFinish() const {
        std::unique_lock<std::mutex> ulk(m_lock);
        while(m_isRunnig) {
            m_taskReady.wait(ulk);
        }
    }
    
    std::chrono::microseconds getRunDuration() const {
        std::unique_lock<std::mutex> ulk(m_lock);
        return m_runDuration;
    }

    // in kHz
    std::uint64_t getAvgFreq() const {
        std::unique_lock<std::mutex> ulk(m_lock);
        return m_avgFreq;
    }

    void clearStats() {
        std::unique_lock<std::mutex> ulk(m_lock);
        m_runDuration = std::chrono::microseconds{0};
        m_avgFreq = 0;
    }
};
