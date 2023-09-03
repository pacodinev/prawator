#pragma once

#include <deque>
#include <limits>
#include <memory_resource>
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
#include <queue>

#include "numa_allocator.hpp"
#include "utils.hpp"

#include <config.h>

template<class T>
class Worker {
private:
    std::optional<NumaAllocator> m_numaAlloc;
    std::queue<T, std::pmr::deque<T>> m_workQueue;
    mutable std::mutex m_lock;
    mutable std::condition_variable m_taskEnqueued, m_queueEmpty;
    std::optional<std::thread> m_thread;
    std::chrono::microseconds m_runDuration = std::chrono::microseconds{0};
    std::chrono::microseconds m_lastDuration = std::chrono::microseconds{0};
    std::uint64_t m_sumFreq = 0; // in kHz
    std::uint64_t m_lastFreq = 0; // in kHz
    unsigned m_cpuNum = 0;
    unsigned m_numaNode = std::numeric_limits<unsigned>::max();
    bool m_timeToDie = false;
    bool m_doCpuPin = false;


    void calcStats() {
        m_runDuration += m_lastDuration;
        m_sumFreq += m_lastFreq*m_lastDuration.count();
    }

    void setCpuMask() noexcept {
        if(m_doCpuPin) {
            cpu_set_t cpuMask;
            CPU_ZERO(&cpuMask);
            CPU_SET(m_cpuNum, &cpuMask); // NOLINT
            int ret = sched_setaffinity(0, sizeof(cpuMask), &cpuMask);
            assert(ret == 0); // TODO: yea, this is bad, only for debug

            if(m_numaNode != std::numeric_limits<unsigned>::max()) {
                Utils::mapThisThreadStackToNuma(m_numaNode);
            }
        }
    }

    // expects m_lock to be locked
    // and m_work is not empty
    void doWork(std::unique_lock<std::mutex> &ulk) noexcept {
        assert(!m_workQueue.empty());

        if(m_workQueue.empty()) { 
            return;
        }
    
        T &work = m_workQueue.front();

        ulk.unlock();

        auto clockStart = std::chrono::steady_clock::now();
        try {
            work();
        } catch(...) { /* do nothing */ }
        auto clockEnd = std::chrono::steady_clock::now();
        // work is finished
        ulk.lock();
        m_lastFreq = Utils::readCurCpuFreq(m_cpuNum);

        m_workQueue.pop(); // delete work object

        m_lastDuration = std::chrono::duration_cast<std::chrono::microseconds>(clockEnd - clockStart);
        calcStats();
    }

    void workFn() noexcept {
        std::unique_lock<std::mutex> ulk(m_lock);

        setCpuMask();

        while(!m_workQueue.empty() || !m_timeToDie) {
            while(m_workQueue.empty() && !m_timeToDie) {
                m_taskEnqueued.wait(ulk);
            }
            
            if(!m_workQueue.empty()) {
                doWork(ulk);
                if(m_workQueue.empty()) {
                    m_queueEmpty.notify_all();
                }
            }
        }
    }
    
    static void workerCall(Worker<T> *work) noexcept {
        work->workFn();
    }

public:

    Worker() : m_workQueue(std::pmr::get_default_resource()) {}
    explicit Worker(unsigned numaNode) 
        : m_numaAlloc(numaNode), m_workQueue(&(m_numaAlloc.value())), m_numaNode(numaNode) {}

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    Worker(Worker&&) noexcept = default;
    Worker& operator=(Worker&&) noexcept = default;
    
    ~Worker() noexcept {
        std::unique_lock<std::mutex> ulk(m_lock);

        if(!m_thread.has_value()) {
            return; 
        }

        std::thread &thd = m_thread.value();

        assert(thd.joinable());

        m_timeToDie = true;
        m_taskEnqueued.notify_one();
        ulk.unlock();

        thd.join();
    }

    void startThread(unsigned cpuNum, bool doCpuPin = false) {
        std::unique_lock<std::mutex> ulk(m_lock);
        assert(!m_thread.has_value());

        m_cpuNum = cpuNum;
        m_doCpuPin = doCpuPin;

        m_thread = std::thread(workerCall, this);
    }

    void runOnThisThread(unsigned cpuNum) { // TODO: cpuPin
        std::unique_lock<std::mutex> ulk(m_lock);
        assert(!m_thread.has_value()); // it would probably work (if bellow is uncommented) but 
                                       // probably the programmer made mistake

        m_cpuNum = cpuNum;

        while(!m_workQueue.empty()) {
            doWork(ulk);
        }

        // m_queueEmpty.notify_one();
    }

    void pushWork(T work) {
        std::unique_lock<std::mutex> ulk(m_lock);

        m_workQueue.emplace(std::move(work));
        m_taskEnqueued.notify_one();
    }

    void waitFinish() const {
        std::unique_lock<std::mutex> ulk(m_lock);
        if(m_workQueue.empty()) {
            return;
        }
        while(!m_workQueue.empty()) {
            m_queueEmpty.wait(ulk);
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
