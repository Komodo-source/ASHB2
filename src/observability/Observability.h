#ifndef OBSERVABILITY_H
#define OBSERVABILITY_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <sstream>
#include <chrono>
#include <mutex>
#include <atomic>
#include <math.h>
#include <functional>

namespace observability {

// Event types for structured logging
enum class EventType {
    ENTITY_CREATED,
    ENTITY_DESTROYED,
    ACTION_SELECTED,
    ACTION_COMPLETED,
    NEED_CHANGED,
    EMOTION_CHANGED,
    RELATIONSHIP_CHANGED,
    RESOURCE_ACQUIRED,
    SOCIAL_INTERACTION,
    DECISION_MADE,
    STATE_TRANSITION,
    CUSTOM
};

// Structured event record
struct EventRecord {
    uint64_t id;
    EventType type;
    int64_t timestamp;      // Simulation time
    int64_t realTimestamp;  // Wall clock time
    int entityId;
    std::string eventType;
    std::map<std::string, std::string> properties;
    std::string jsonData;
    
    EventRecord();
    std::string toJson() const;
    std::string toCsv() const;
};

// Entity state snapshot for visualization
struct EntityStateSnapshot {
    int entityId;
    float posX, posY, posZ;
    std::map<std::string, float> needs;
    std::map<std::string, float> emotions;
    std::map<int, float> relationships;
    std::string currentAction;
    float health;
    float stress;
    float happiness;
    int64_t timestamp;
    
    std::string toJson() const;
};

// Stream writer interface
class EventStreamWriter {
public:
    virtual ~EventStreamWriter() = default;
    virtual void write(const EventRecord& event) = 0;
    virtual void flush() = 0;
    virtual void close() = 0;
};

// File-based stream writer
class FileEventWriter : public EventStreamWriter {
private:
    std::ofstream file;
    std::mutex writeMutex;
    std::string format;  // "json", "csv", "parquet"
    
public:
    explicit FileEventWriter(const std::string& filename, 
                            const std::string& fmt = "json");
    
    void write(const EventRecord& event) override;
    void flush() override;
    void close() override;
};

// In-memory circular buffer for real-time access
class CircularEventBuffer {
private:
    std::vector<EventRecord> buffer;
    size_t head;
    size_t count;
    size_t capacity;
    mutable std::mutex mutex;
    
public:
    explicit CircularEventBuffer(size_t cap = 10000);
    
    void push(const EventRecord& event);
    std::vector<EventRecord> getRecent(size_t n) const;
    std::vector<EventRecord> getByType(EventType type, size_t n) const;
    std::vector<EventRecord> getByEntity(int entityId, size_t n) const;
    void clear();
    size_t size() const;
};

// Event stream manager
class EventStreamManager {
private:
    std::vector<std::shared_ptr<EventStreamWriter>> writers;
    CircularEventBuffer liveBuffer;
    std::atomic<uint64_t> nextEventId{1};
    std::atomic<bool> enabled{true};
    
    static EventStreamManager* instance;
    
    EventStreamManager();
    
public:
    static EventStreamManager& getInstance();
    
    void addWriter(std::shared_ptr<EventStreamWriter> writer);
    void removeWriter(std::shared_ptr<EventStreamWriter> writer);
    
    // Log events
    void logEvent(EventType type, int entityId, 
                 const std::map<std::string, std::string>& properties = {});
    
    void logActionSelected(int entityId, const std::string& actionName,
                          float motivation, const std::string& reason = "");
    
    void logNeedChanged(int entityId, const std::string& needName,
                       float oldValue, float newValue);
    
    void logEmotionChanged(int entityId, const std::string& emotionName,
                          float oldValue, float newValue);
    
    void logRelationshipChanged(int entityId1, int entityId2,
                               const std::string& relationType,
                               float oldValue, float newValue);
    
    void logDecision(int entityId, const std::string& decisionType,
                    const std::vector<std::string>& options,
                    const std::string& chosenOption,
                    const std::string& reasoning = "");
    
    // Query recent events
    std::vector<EventRecord> getRecentEvents(size_t count) const;
    std::vector<EventRecord> queryEvents(EventType type, 
                                         int64_t startTime, 
                                         int64_t endTime) const;
    
    void setEnabled(bool enable);
    bool isEnabled() const { return enabled.load(); }
};

// Data export formats
enum class ExportFormat {
    CSV,
    JSON,
    PARQUET,
    SQLITE
};

// Exporter interface
class DataExporter {
public:
    virtual ~DataExporter() = default;
    virtual bool exportToFile(const std::string& filename) = 0;
    virtual std::string getFormatName() const = 0;
};

// CSV Exporter
class CsvExporter : public DataExporter {
private:
    std::vector<std::string> columns;
    std::vector<std::map<std::string, std::string>> rows;
    
public:
    explicit CsvExporter(const std::vector<std::string>& cols);
    
    void addRow(const std::map<std::string, std::string>& row);
    bool exportToFile(const std::string& filename) override;
    std::string getFormatName() const override { return "CSV"; }
};

// JSON Exporter
class JsonExporter : public DataExporter {
private:
    std::string arrayName;
    std::vector<std::map<std::string, std::string>> records;
    
public:
    explicit JsonExporter(const std::string& arrName = "records");
    
    void addRecord(const std::map<std::string, std::string>& record);
    bool exportToFile(const std::string& filename) override;
    std::string getFormatName() const override { return "JSON"; }
};

// State visualizer data structures
struct VisualizationFrame {
    int64_t timestamp;
    std::vector<EntityStateSnapshot> entityStates;
    std::map<std::string, float> globalMetrics;
    std::string metadata;
    
    std::string toJson() const;
};

// Real-time state observer
class StateObserver {
private:
    std::vector<EntityStateSnapshot> snapshots;
    std::map<std::string, float> metrics;
    mutable std::mutex mutex;
    std::atomic<int64_t> currentTimestamp{0};
    
public:
    void updateSnapshot(const EntityStateSnapshot& snapshot);
    void removeSnapshot(int entityId);
    
    std::vector<EntityStateSnapshot> getAllSnapshots() const;
    EntityStateSnapshot getSnapshot(int entityId) const;
    
    void setMetric(const std::string& name, float value);
    float getMetric(const std::string& name) const;
    
    void setCurrentTimestamp(int64_t ts);
    int64_t getCurrentTimestamp() const { return currentTimestamp.load(); }
    
    VisualizationFrame getCurrentFrame() const;
};

// Analytics engine for computing statistics
class AnalyticsEngine {
private:
    std::map<std::string, std::vector<double>> timeSeries;
    std::map<std::string, double> aggregates;
    
public:
    void recordDataPoint(const std::string& metric, double value, int64_t timestamp);
    
    // Statistical computations
    double computeMean(const std::string& metric) const;
    double computeVariance(const std::string& metric) const;
    double computeStdDev(const std::string& metric) const;
    double computeMin(const std::string& metric) const;
    double computeMax(const std::string& metric) const;
    double computePercentile(const std::string& metric, double p) const;
    
    // Correlation
    double computeCorrelation(const std::string& metric1, 
                             const std::string& metric2) const;
    
    // Trend analysis
    double computeTrend(const std::string& metric) const;  // Linear regression slope
    
    // Get time series
    std::vector<std::pair<int64_t, double>> getTimeSeries(const std::string& metric) const;
    
    // Aggregates
    void setAggregate(const std::string& name, double value);
    double getAggregate(const std::string& name) const;
    
    // Export analytics report
    std::string generateReport() const;
};

// Dashboard data provider
struct DashboardData {
    int totalEntities;
    int activeEntities;
    float averageHappiness;
    float averageStress;
    std::map<std::string, int> actionDistribution;
    std::map<std::string, float> needAverages;
    std::vector<std::pair<std::string, double>> topMetrics;
    int64_t simulationTime;
    float fps;
    
    std::string toJson() const;
};

// Profiler for performance monitoring
class Profiler {
private:
    struct ProfileData {
        std::string name;
        int64_t totalTime;
        int64_t callCount;
        int64_t minTime;
        int64_t maxTime;
    };
    
    std::map<std::string, ProfileData> profiles;
    std::map<std::string, int64_t> startTimes;
    mutable std::mutex mutex;
    
public:
    static Profiler& getInstance();
    void begin(const std::string& name);
    void end(const std::string& name);
    
    void record(const std::string& name, int64_t duration);
    
    std::map<std::string, ProfileData> getProfiles() const;
    std::string generateReport() const;
    void reset();
};

// Convenience macros for profiling
#ifdef ENABLE_PROFILING
#define PROFILE_SCOPE(name) \
    observability::ProfileScope _profile_scope_##__LINE__(name)
#else
#define PROFILE_SCOPE(name)
#endif

class ProfileScope {
private:
    std::string name;
    std::chrono::steady_clock::time_point start;
    
public:
    explicit ProfileScope(const std::string& n) 
        : name(n), start(std::chrono::steady_clock::now()) {}
    
    ~ProfileScope() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
        Profiler::getInstance().record(name, duration);
    }
};

} // namespace observability

#endif // OBSERVABILITY_H
