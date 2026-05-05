#include "Scalability.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace scalability {

// ============= ThreadPool Implementation =============

ThreadPool::ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    condition.wait(lock, [this] { 
                        return stop || !tasks.empty(); 
                    });
                    
                    if (stop && tasks.empty()) return;
                    
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    stop = true;
    condition.notify_all();
    
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::waitAll() {
    // Wait until queue is empty (simplified)
    while (true) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (tasks.empty()) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ============= ParallelEntityProcessor Implementation =============

ParallelEntityProcessor::ParallelEntityProcessor(ThreadPool& threadPool)
    : pool(threadPool) {}

void ParallelEntityProcessor::setEntities(std::vector<Entity*>& ents) {
    std::lock_guard<std::mutex> lock(entityMutex);
    entities = ents;
}

void ParallelEntityProcessor::parallelUpdate(
    double deltaTime,
    std::function<void(Entity*, double)> updateFunc) {
    
    std::vector<std::future<void>> futures;
    
    {
        std::lock_guard<std::mutex> lock(entityMutex);
        
        for (auto* entity : entities) {
            futures.push_back(pool.enqueue([entity, deltaTime, updateFunc]() {
                updateFunc(entity, deltaTime);
            }));
        }
    }
    
    // Wait for all updates to complete
    for (auto& future : futures) {
        future.get();
    }
}

void ParallelEntityProcessor::parallelDecisionMaking(
    std::function<void*(Entity*)> decisionFunc) {
    
    std::vector<std::future<void*>> futures;
    
    {
        std::lock_guard<std::mutex> lock(entityMutex);
        
        for (auto* entity : entities) {
            futures.push_back(pool.enqueue([entity, decisionFunc]() {
                return decisionFunc(entity);
            }));
        }
    }
    
    // Wait for all decisions
    for (auto& future : futures) {
        future.get();
    }
}

// ============= GPUAccelerator Implementation =============

GPUAccelerator::GPUAccelerator() : available(false), deviceId(-1) {
    // Placeholder: would initialize CUDA/OpenCL here
    // Check for GPU availability
#ifdef USE_CUDA
    // cudaGetDeviceCount(&deviceCount);
    // available = deviceCount > 0;
#endif
}

void GPUAccelerator::transferToDevice(const std::vector<Entity*>& entities) {
    if (!available) return;
    // Placeholder: would copy entity data to GPU memory
}

void GPUAccelerator::updateEntities(double deltaTime) {
    if (!available) return;
    // Placeholder: would launch GPU kernel for entity updates
}

void GPUAccelerator::transferFromDevice(std::vector<Entity*>& entities) {
    if (!available) return;
    // Placeholder: would copy results back from GPU
}

void GPUAccelerator::computeSpatialInteractions() {
    if (!available) return;
    // Placeholder: GPU-accelerated spatial computations
}

// ============= WorkStealer Implementation =============

bool WorkStealer::WorkQueue::tryPop(std::function<void()>& task) {
    std::lock_guard<std::mutex> lock(mutex);
    if (tasks.empty()) return false;
    task = std::move(tasks.front());
    tasks.pop_front();
    return true;
}

void WorkStealer::WorkQueue::push(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex);
    tasks.push_back(std::move(task));
}

bool WorkStealer::WorkQueue::empty() const {
    return tasks.empty();
}

size_t WorkStealer::WorkQueue::size() const {
    return tasks.size();
}

WorkStealer::WorkStealer(size_t numQueues) : rng(std::random_device{}()) {
    for (size_t i = 0; i < numQueues; ++i) {
        queues.push_back(std::make_unique<WorkQueue>());
    }
}

void WorkStealer::pushTask(size_t queueId, std::function<void()> task) {
    if (queueId < queues.size()) {
        queues[queueId]->push(std::move(task));
    }
}

bool WorkStealer::stealTask(size_t thiefId, std::function<void()>& task) {
    std::uniform_int_distribution<size_t> dist(0, queues.size() - 1);
    
    // Try random queues
    for (size_t attempts = 0; attempts < queues.size(); ++attempts) {
        size_t victimId = dist(rng);
        if (victimId != thiefId) {
            if (queues[victimId]->tryPop(task)) {
                return true;
            }
        }
    }
    
    return false;
}

void WorkStealer::workerLoop(size_t workerId) {
    while (true) {
        std::function<void()> task;
        
        // Try own queue first
        if (queues[workerId]->tryPop(task)) {
            task();
            continue;
        }
        
        // Try to steal from others
        if (stealTask(workerId, task)) {
            task();
            continue;
        }
        
        // No work available, wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ============= EntityStorage Implementation =============

void EntityStorage::reserve(size_t n) {
    ids.reserve(n);
    positionsX.reserve(n);
    positionsY.reserve(n);
    positionsZ.reserve(n);
    velocitiesX.reserve(n);
    velocitiesY.reserve(n);
    velocitiesZ.reserve(n);
    health.reserve(n);
    stress.reserve(n);
    happiness.reserve(n);
    states.reserve(n);
}

void EntityStorage::addEntity(int id, float x, float y, float z) {
    ids.push_back(id);
    positionsX.push_back(x);
    positionsY.push_back(y);
    positionsZ.push_back(z);
    velocitiesX.push_back(0.0f);
    velocitiesY.push_back(0.0f);
    velocitiesZ.push_back(0.0f);
    health.push_back(100.0f);
    stress.push_back(0.0f);
    happiness.push_back(50.0f);
    states.push_back(0);
}

void EntityStorage::removeEntity(size_t index) {
    if (index >= ids.size()) return;
    
    // Swap with last and pop (order not preserved)
    size_t last = ids.size() - 1;
    
    ids[index] = ids[last];
    positionsX[index] = positionsX[last];
    positionsY[index] = positionsY[last];
    positionsZ[index] = positionsZ[last];
    velocitiesX[index] = velocitiesX[last];
    velocitiesY[index] = velocitiesY[last];
    velocitiesZ[index] = velocitiesZ[last];
    health[index] = health[last];
    stress[index] = stress[last];
    happiness[index] = happiness[last];
    states[index] = states[last];
    
    ids.pop_back();
    positionsX.pop_back();
    positionsY.pop_back();
    positionsZ.pop_back();
    velocitiesX.pop_back();
    velocitiesY.pop_back();
    velocitiesZ.pop_back();
    health.pop_back();
    stress.pop_back();
    happiness.pop_back();
    states.pop_back();
}

void EntityStorage::updatePositionsBatch(const std::vector<float>& dx,
                                         const std::vector<float>& dy,
                                         const std::vector<float>& dz) {
    if (dx.size() != positionsX.size() || 
        dy.size() != positionsY.size() ||
        dz.size() != positionsZ.size()) {
        return;
    }
    
    for (size_t i = 0; i < positionsX.size(); ++i) {
        positionsX[i] += dx[i];
        positionsY[i] += dy[i];
        positionsZ[i] += dz[i];
    }
}

// ============= SIMDOperations Implementation =============

void SIMDOperations::computeDistancesSIMD(const float* x1, const float* y1,
                                          const float* x2, const float* y2,
                                          float* distances, size_t count) {
    // Placeholder: would use SSE/AVX intrinsics here
    // Scalar fallback
    for (size_t i = 0; i < count; ++i) {
        float dx = x1[i] - x2[i];
        float dy = y1[i] - y2[i];
        distances[i] = std::sqrt(dx * dx + dy * dy);
    }
}

void SIMDOperations::updateStatsSIMD(float* stats, const float* deltas, 
                                     size_t count) {
    // Placeholder: would use SIMD intrinsics
    // Scalar fallback
    for (size_t i = 0; i < count; ++i) {
        stats[i] += deltas[i];
    }
}

void SIMDOperations::compareAndSelectSIMD(const float* values, float threshold,
                                          bool* results, size_t count) {
    // Placeholder: would use SIMD comparison
    // Scalar fallback
    for (size_t i = 0; i < count; ++i) {
        results[i] = values[i] > threshold;
    }
}

// ============= SpatialPartitioner Implementation =============

SpatialPartitioner::SpatialPartitioner(float cellSz, float worldWidth, float worldHeight)
    : cellSize(cellSz) {
    gridWidth = static_cast<int>(std::ceil(worldWidth / cellSize));
    gridHeight = static_cast<int>(std::ceil(worldHeight / cellSize));
}

void SpatialPartitioner::partition(std::vector<Entity*>& entities,
                                   std::vector<std::vector<Entity*>>& partitions) {
    partitions.clear();
    partitions.resize(gridWidth * gridHeight);
    
    for (auto* entity : entities) {
        // Get entity position (placeholder - would access actual entity data)
        float x = 0.0f, y = 0.0f; // Would get from entity
        
        int cellX = static_cast<int>(std::floor(x / cellSize));
        int cellY = static_cast<int>(std::floor(y / cellSize));
        
        cellX = std::clamp(cellX, 0, gridWidth - 1);
        cellY = std::clamp(cellY, 0, gridHeight - 1);
        
        size_t cellIndex = cellY * gridWidth + cellX;
        partitions[cellIndex].push_back(entity);
    }
}

std::vector<Entity*> SpatialPartitioner::getNearbyEntities(
    Entity* entity,
    const std::vector<Entity*>& allEntities) {
    
    std::vector<Entity*> nearby;
    // Placeholder: would get cell and neighbors
    return nearby;
}

// ============= LoadBalancer Implementation =============

size_t LoadBalancer::getNextTaskIndex() {
    return nextTask.fetch_add(1);
}

void LoadBalancer::balanceWork(std::vector<std::thread>& workers,
                               std::function<void(size_t)> workFunc) {
    // Dynamic load balancing with work stealing
    std::atomic<size_t> taskIndex{0};
    size_t totalTasks = workers.size() * 100; // Example
    
    for (auto& worker : workers) {
        worker = std::thread([&taskIndex, totalTasks, workFunc]() {
            while (true) {
                size_t idx = taskIndex.fetch_add(1);
                if (idx >= totalTasks) break;
                workFunc(idx);
            }
        });
    }
    
    for (auto& worker : workers) {
        worker.join();
    }
}

} // namespace scalability
