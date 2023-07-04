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

#include "utils.hpp"

#include <config.h>

template<class T>
class Worker {
private:
    std::optional<T> m_work;
    mutable std::mutex m_lock;
    mutable std::condition_variable m_waitForTask, m_taskReady;
    std::optional<std::thread> m_thread;
    std::chrono::microseconds m_runDuration = std::chrono::microseconds{0};
    std::chrono::microseconds m_lastDuration = std::chrono::microseconds{0};
    std::uint64_t m_sumFreq = 0; // in kHz
    std::uint64_t m_lastFreq = 0; // in kHz
    unsigned m_cpuPin = 0;
    bool m_isRunnig = false, m_timeToDie = false;


    void calcStats() {
        if(m_lastFreq == 0) { return; }
        
        m_runDuration += m_lastDuration;
        m_sumFreq += m_lastFreq*m_lastDuration.count();
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
            m_lastFreq = Utils::readCurCpuFreq(m_cpuPin);
            m_lastDuration = std::chrono::duration_cast<std::chrono::microseconds>(clockEnd - clockStart);
            calcStats();
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
    
    [[nodiscard]] std::chrono::microseconds getAllRunDuration() const {
        std::unique_lock<std::mutex> ulk(m_lock);
        return m_runDuration;
    }

    // in kHz
    [[nodiscard]] std::uint64_t getAvgFreq() const {
        std::unique_lock<std::mutex> ulk(m_lock);
        return m_sumFreq/m_runDuration.count();
    }

    [[nodiscard]] std::chrono::microseconds getLastRunDuration() const {
        std::unique_lock<std::mutex> ulk(m_lock);
        return m_lastDuration;
    }

    // in kHz
    [[nodiscard]] std::uint64_t getLastFreq() const {
        std::unique_lock<std::mutex> ulk(m_lock);
        return m_lastFreq;
    }

    void clearStats() {
        std::unique_lock<std::mutex> ulk(m_lock);
        m_runDuration = std::chrono::microseconds{0};
        m_sumFreq = 0;
    }
};
