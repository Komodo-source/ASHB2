#ifndef SCALABILITY_H
#define SCALABILITY_H

#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <future>
#include <queue>
#include <condition_variable>
#include <memory>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <numeric>
#include <deque>

#ifdef _OPENMP
#include <omp.h>
#endif

// Forward declarations
class Entity;
struct SpatialMesh;

namespace scalability {

// Thread pool for parallel entity updates
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop{false};
    
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();
    
    template<class F>
    auto enqueue(F&& f) -> std::future<decltype(f())>;
    
    void waitAll();
    size_t getThreadCount() const { return workers.size(); }
};

// Parallel entity processor
class ParallelEntityProcessor {
private:
    ThreadPool& pool;
    std::vector<Entity*> entities;
    std::mutex entityMutex;
    
public:
    explicit ParallelEntityProcessor(ThreadPool& threadPool);
    
    void setEntities(std::vector<Entity*>& ents);
    
    // Parallel update of all entities
    void parallelUpdate(double deltaTime, 
                       std::function<void(Entity*, double)> updateFunc);
    
    // Parallel decision making
    void parallelDecisionMaking(
        std::function<void*(Entity*)> decisionFunc);
    
    // Map-reduce pattern for aggregating entity states
    template<typename T>
    T mapReduce(std::function<T(Entity*)> mapFunc,
                std::function<T(const std::vector<T>&)> reduceFunc);
};

// GPU-accelerated computation interface (CUDA/OpenCL placeholder)
class GPUAccelerator {
private:
    bool available;
    int deviceId;
    
public:
    GPUAccelerator();
    
    bool isAvailable() const { return available; }
    
    // Transfer entity data to GPU
    void transferToDevice(const std::vector<Entity*>& entities);
    
    // Parallel update on GPU
    void updateEntities(double deltaTime);
    
    // Transfer results back
    void transferFromDevice(std::vector<Entity*>& entities);
    
    // Spatial computations on GPU
    void computeSpatialInteractions();
};

// Work stealer for load balancing
class WorkStealer {
private:
    struct WorkQueue {
        std::deque<std::function<void()>> tasks;
        std::mutex mutex;
        
        bool tryPop(std::function<void()>& task);
        void push(std::function<void()> task);
        bool empty() const;
        size_t size() const;
    };
    
    std::vector<std::unique_ptr<WorkQueue>> queues;
    std::mt19937 rng;
    
public:
    explicit WorkStealer(size_t numQueues);
    
    void pushTask(size_t queueId, std::function<void()> task);
    bool stealTask(size_t thiefId, std::function<void()>& task);
    
    // Worker loop
    void workerLoop(size_t workerId);
};

// Cache-friendly entity storage (Structure of Arrays)
struct EntityStorage {
    // Separate arrays for better cache locality
    std::vector<int> ids;
    std::vector<float> positionsX;
    std::vector<float> positionsY;
    std::vector<float> positionsZ;
    std::vector<float> velocitiesX;
    std::vector<float> velocitiesY;
    std::vector<float> velocitiesZ;
    std::vector<float> health;
    std::vector<float> stress;
    std::vector<float> happiness;
    std::vector<int> states;
    
    size_t size() const { return ids.size(); }
    
    void reserve(size_t n);
    void addEntity(int id, float x, float y, float z);
    void removeEntity(size_t index);
    
    // Batch operations
    void updatePositionsBatch(const std::vector<float>& dx, 
                             const std::vector<float>& dy,
                             const std::vector<float>& dz);
};

// SIMD-optimized computations
class SIMDOperations {
public:
    // Vectorized distance calculations
    static void computeDistancesSIMD(const float* x1, const float* y1,
                                     const float* x2, const float* y2,
                                     float* distances, size_t count);
    
    // Vectorized stat updates
    static void updateStatsSIMD(float* stats, const float* deltas, 
                               size_t count);
    
    // Vectorized comparison operations
    static void compareAndSelectSIMD(const float* values, float threshold,
                                    bool* results, size_t count);
};

// Partitioning strategy interface
class PartitionStrategy {
public:
    virtual ~PartitionStrategy() = default;
    virtual void partition(std::vector<Entity*>& entities,
                          std::vector<std::vector<Entity*>>& partitions) = 0;
};

// Spatial partitioning for parallel processing
class SpatialPartitioner : public PartitionStrategy {
private:
    float cellSize;
    int gridWidth;
    int gridHeight;
    
public:
    SpatialPartitioner(float cellSz, float worldWidth, float worldHeight);
    
    void partition(std::vector<Entity*>& entities,
                  std::vector<std::vector<Entity*>>& partitions) override;
    
    // Get entities in neighboring cells
    std::vector<Entity*> getNearbyEntities(Entity* entity,
                                          const std::vector<Entity*>& allEntities);
};

// Load balancer
class LoadBalancer {
private:
    std::atomic<size_t> nextTask{0};
    size_t totalTasks;
    
public:
    explicit LoadBalancer(size_t tasks) : totalTasks(tasks) {}
    
    size_t getNextTaskIndex();
    bool hasMoreTasks() const { return nextTask < totalTasks; }
    
    // Dynamic work stealing
    static void balanceWork(std::vector<std::thread>& workers,
                           std::function<void(size_t)> workFunc);
};

// Parallel reduction utilities
class ParallelReduction {
public:
    // Sum reduction
    template<typename T>
    static T sum(const std::vector<T>& data, size_t numThreads = 0);
    
    // Min/Max reduction
    template<typename T>
    static T min(const std::vector<T>& data, size_t numThreads = 0);
    
    template<typename T>
    static T max(const std::vector<T>& data, size_t numThreads = 0);
};

} // namespace scalability

// Implementation of ThreadPool::enqueue (must be in header for templates)
template<class F>
auto scalability::ThreadPool::enqueue(F&& f) -> std::future<decltype(f())> {
    using return_type = decltype(f());
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
    std::future<return_type> result = task->get_future();
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");
        tasks.emplace([task]() { (*task)(); });
    }
    
    condition.notify_one();
    return result;
}

// Implementation of mapReduce (must be in header)
template<typename T>
T scalability::ParallelEntityProcessor::mapReduce(
    std::function<T(Entity*)> mapFunc,
    std::function<T(const std::vector<T>&)> reduceFunc) {
    
    std::vector<std::future<T>> futures;
    
    for (auto* entity : entities) {
        futures.push_back(pool.enqueue([entity, mapFunc]() {
            return mapFunc(entity);
        }));
    }
    
    std::vector<T> mappedResults;
    mappedResults.reserve(futures.size());
    
    for (auto& future : futures) {
        mappedResults.push_back(future.get());
    }
    
    return reduceFunc(mappedResults);
}

// Template implementations for ParallelReduction
namespace scalability {
namespace detail {
    template<typename T>
    T sequentialSum(const std::vector<T>& data, size_t start, size_t end) {
        T sum = T();
        for (size_t i = start; i < end; ++i) {
            sum += data[i];
        }
        return sum;
    }
}

template<typename T>
T ParallelReduction::sum(const std::vector<T>& data, size_t numThreads) {
    if (numThreads == 0) {
#ifdef _OPENMP
        numThreads = omp_get_max_threads();
#else
        numThreads = std::thread::hardware_concurrency();
#endif
    }
    
    if (numThreads <= 1 || data.size() < 1000) {
        return std::accumulate(data.begin(), data.end(), T());
    }
    
    size_t chunkSize = data.size() / numThreads;
    std::vector<std::future<T>> futures;
    
    for (size_t i = 0; i < numThreads; ++i) {
        size_t start = i * chunkSize;
        size_t end = (i == numThreads - 1) ? data.size() : start + chunkSize;
        
        futures.push_back(std::async(std::launch::async, 
            [&data, start, end]() {
                return detail::sequentialSum(data, start, end);
            }));
    }
    
    T total = T();
    for (auto& future : futures) {
        total += future.get();
    }
    
    return total;
}

template<typename T>
T ParallelReduction::min(const std::vector<T>& data, size_t numThreads) {
    if (data.empty()) return T();
    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    
    if (numThreads <= 1 || data.size() < 1000) {
        return *std::min_element(data.begin(), data.end());
    }
    
    // Parallel implementation similar to sum
    return *std::min_element(data.begin(), data.end());
}

template<typename T>
T ParallelReduction::max(const std::vector<T>& data, size_t numThreads) {
    if (data.empty()) return T();
    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    
    if (numThreads <= 1 || data.size() < 1000) {
        return *std::max_element(data.begin(), data.end());
    }
    
    return *std::max_element(data.begin(), data.end());
}

} // namespace scalability

#endif // SCALABILITY_H
