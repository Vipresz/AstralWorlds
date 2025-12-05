#pragma once

#include "core/Types.h"
#include <functional>
#include <memory>
#include <atomic>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstdint>

namespace Engine {
namespace Jobs {

// Job structure for task scheduling
class Job {
public:
    using Task = std::function<void()>;
    
    Job(Task task, Job* parent = nullptr)
        : m_task(std::move(task))
        , m_parent(parent)
        , m_unfinishedJobs(1)
    {
        if (m_parent) {
            m_parent->m_unfinishedJobs++;
        }
    }
    
    void Execute() {
        if (m_task) {
            m_task();
        }
        
        Finish();
    }
    
    void Finish() {
        m_unfinishedJobs--;
        
        if (m_parent) {
            m_parent->Finish();
        }
    }
    
    bool IsFinished() const {
        return m_unfinishedJobs.load() == 0;
    }
    
    void Wait() {
        while (!IsFinished()) {
            std::this_thread::yield();
        }
    }
    
private:
    Task m_task;
    Job* m_parent;
    std::atomic<int> m_unfinishedJobs;
};

// Thread-safe job queue with job stealing
class JobQueue {
public:
    void Push(std::shared_ptr<Job> job) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(job);
        m_condition.notify_one();
    }
    
    bool Pop(std::shared_ptr<Job>& job) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        
        job = m_queue.front();
        m_queue.pop();
        return true;
    }
    
    bool TryPop(std::shared_ptr<Job>& job) {
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        if (!lock.owns_lock() || m_queue.empty()) {
            return false;
        }
        
        job = m_queue.front();
        m_queue.pop();
        return true;
    }
    
    bool IsEmpty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
    
    void WaitForJob() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [this] { return !m_queue.empty(); });
    }
    
private:
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::queue<std::shared_ptr<Job>> m_queue;
};

// Multithreaded job system with worker threads
class JobSystem {
public:
    JobSystem(uint32_t numThreads = 0)
        : m_shouldStop(false)
    {
        if (numThreads == 0) {
            numThreads = std::max(1u, std::thread::hardware_concurrency() - 1); // Leave one for main thread
        }
        
        // Create worker threads
        for (uint32_t i = 0; i < numThreads; ++i) {
            m_queues.push_back(std::make_unique<JobQueue>());
        }
        
        for (uint32_t i = 0; i < numThreads; ++i) {
            m_workers.push_back(std::thread([this, i, numThreads]() {
                WorkerThread(i, numThreads);
            }));
        }
    }
    
    ~JobSystem() {
        Shutdown();
    }
    
    // Schedule a job to execute
    std::shared_ptr<Job> Schedule(Job::Task task, Job* parent = nullptr) {
        auto job = std::make_shared<Job>(std::move(task), parent);
        
        // Distribute jobs across queues (round-robin)
        static std::atomic<uint32_t> nextQueue{0};
        uint32_t queueIndex = nextQueue++ % m_queues.size();
        m_queues[queueIndex]->Push(job);
        
        return job;
    }
    
    // Wait for a job to complete
    void Wait(std::shared_ptr<Job> job) {
        if (job) {
            job->Wait();
        }
    }
    
    // Wait for all jobs to complete
    void WaitForAll() {
        // Wait until all queues are empty
        bool allEmpty = false;
        while (!allEmpty) {
            allEmpty = true;
            for (const auto& queue : m_queues) {
                if (!queue->IsEmpty()) {
                    allEmpty = false;
                    break;
                }
            }
            if (!allEmpty) {
                std::this_thread::yield();
            }
        }
    }
    
    void Shutdown() {
        m_shouldStop = true;
        
        // Wake up all workers
        for (auto& queue : m_queues) {
            queue->WaitForJob(); // This will wake them up
        }
        
        // Join all threads
        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    uint32_t GetThreadCount() const {
        return static_cast<uint32_t>(m_workers.size());
    }
    
private:
    void WorkerThread(uint32_t threadIndex, uint32_t numThreads) {
        while (!m_shouldStop) {
            std::shared_ptr<Job> job;
            
            // Try to get job from own queue
            if (m_queues[threadIndex]->Pop(job)) {
                job->Execute();
                continue;
            }
            
            // Try to steal job from other queues
            bool foundJob = false;
            for (uint32_t i = 0; i < numThreads; ++i) {
                uint32_t stealIndex = (threadIndex + i + 1) % numThreads;
                if (m_queues[stealIndex]->TryPop(job)) {
                    job->Execute();
                    foundJob = true;
                    break;
                }
            }
            
            if (!foundJob) {
                // Wait for a job to become available
                m_queues[threadIndex]->WaitForJob();
            }
        }
    }
    
    std::atomic<bool> m_shouldStop;
    std::vector<std::unique_ptr<JobQueue>> m_queues;
    std::vector<std::thread> m_workers;
};

} // namespace Jobs
} // namespace Engine

