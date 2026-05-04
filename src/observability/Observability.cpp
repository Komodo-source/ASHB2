#include "Observability.h"
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <ctime>

namespace observability {

// ============= EventRecord Implementation =============

EventRecord::EventRecord() 
    : id(0), type(EventType::CUSTOM), timestamp(0), 
      realTimestamp(0), entityId(-1) {}

std::string EventRecord::toJson() const {
    std::ostringstream oss;
    oss << "{\"id\":" << id
        << ",\"type\":\"" << eventType << "\""
        << ",\"timestamp\":" << timestamp
        << ",\"entityId\":" << entityId
        << ",\"properties\":{";
    
    bool first = true;
    for (const auto& [key, value] : properties) {
        if (!first) oss << ",";
        oss << "\"" << key << "\":\"" << value << "\"";
        first = false;
    }
    
    oss << "}}";
    return oss.str();
}

std::string EventRecord::toCsv() const {
    std::ostringstream oss;
    oss << id << "," << eventType << "," << timestamp << "," << entityId;
    
    for (const auto& [key, value] : properties) {
        oss << "," << value;
    }
    
    return oss.str();
}

// ============= EntityStateSnapshot Implementation =============

std::string EntityStateSnapshot::toJson() const {
    std::ostringstream oss;
    oss << "{\"entityId\":" << entityId
        << ",\"position\":[" << posX << "," << posY << "," << posZ << "]"
        << ",\"needs\":{";
    
    bool first = true;
    for (const auto& [name, value] : needs) {
        if (!first) oss << ",";
        oss << "\"" << name << "\":" << value;
        first = false;
    }
    
    oss << "},\"emotions\":{";
    first = true;
    for (const auto& [name, value] : emotions) {
        if (!first) oss << ",";
        oss << "\"" << name << "\":" << value;
        first = false;
    }
    
    oss << "},\"health\":" << health
        << ",\"stress\":" << stress
        << ",\"happiness\":" << happiness
        << ",\"currentAction\":\"" << currentAction << "\""
        << ",\"timestamp\":" << timestamp << "}";
    
    return oss.str();
}

// ============= FileEventWriter Implementation =============

FileEventWriter::FileEventWriter(const std::string& filename, const std::string& fmt)
    : format(fmt) {
    file.open(filename);
}

void FileEventWriter::write(const EventRecord& event) {
    std::lock_guard<std::mutex> lock(writeMutex);
    if (file.is_open()) {
        if (format == "json") {
            file << event.toJson() << "\n";
        } else if (format == "csv") {
            file << event.toCsv() << "\n";
        }
    }
}

void FileEventWriter::flush() {
    std::lock_guard<std::mutex> lock(writeMutex);
    if (file.is_open()) {
        file.flush();
    }
}

void FileEventWriter::close() {
    std::lock_guard<std::mutex> lock(writeMutex);
    if (file.is_open()) {
        file.close();
    }
}

// ============= CircularEventBuffer Implementation =============

CircularEventBuffer::CircularEventBuffer(size_t cap)
    : buffer(cap), head(0), count(0), capacity(cap) {}

void CircularEventBuffer::push(const EventRecord& event) {
    std::lock_guard<std::mutex> lock(mutex);
    buffer[head] = event;
    head = (head + 1) % capacity;
    if (count < capacity) count++;
}

std::vector<EventRecord> CircularEventBuffer::getRecent(size_t n) const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<EventRecord> result;
    n = std::min(n, count);
    
    for (size_t i = 0; i < n; ++i) {
        size_t idx = (head + capacity - n + i) % capacity;
        result.push_back(buffer[idx]);
    }
    
    return result;
}

std::vector<EventRecord> CircularEventBuffer::getByType(EventType type, size_t n) const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<EventRecord> result;
    
    for (size_t i = 0; i < count && result.size() < n; ++i) {
        size_t idx = (head + capacity - 1 - i) % capacity;
        if (buffer[idx].type == type) {
            result.push_back(buffer[idx]);
        }
    }
    
    return result;
}

std::vector<EventRecord> CircularEventBuffer::getByEntity(int entityId, size_t n) const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<EventRecord> result;
    
    for (size_t i = 0; i < count && result.size() < n; ++i) {
        size_t idx = (head + capacity - 1 - i) % capacity;
        if (buffer[idx].entityId == entityId) {
            result.push_back(buffer[idx]);
        }
    }
    
    return result;
}

void CircularEventBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    head = 0;
    count = 0;
}

size_t CircularEventBuffer::size() const {
    std::lock_guard<std::mutex> lock(mutex);
    return count;
}

// ============= EventStreamManager Implementation =============

EventStreamManager* EventStreamManager::instance = nullptr;

EventStreamManager::EventStreamManager() {}

EventStreamManager& EventStreamManager::getInstance() {
    static EventStreamManager instance;
    return instance;
}

void EventStreamManager::addWriter(std::shared_ptr<EventStreamWriter> writer) {
    writers.push_back(writer);
}

void EventStreamManager::removeWriter(std::shared_ptr<EventStreamWriter> writer) {
    writers.erase(std::remove(writers.begin(), writers.end(), writer), writers.end());
}

void EventStreamManager::logEvent(EventType type, int entityId,
                                   const std::map<std::string, std::string>& properties) {
    if (!enabled.load()) return;
    
    EventRecord record;
    record.id = nextEventId.fetch_add(1);
    record.type = type;
    record.entityId = entityId;
    record.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    record.properties = properties;
    
    liveBuffer.push(record);
    
    for (auto& writer : writers) {
        writer->write(record);
    }
}

void EventStreamManager::logActionSelected(int entityId, const std::string& actionName,
                                            float motivation, const std::string& reason) {
    logEvent(EventType::ACTION_SELECTED, entityId, {
        {"action", actionName},
        {"motivation", std::to_string(motivation)},
        {"reason", reason}
    });
}

void EventStreamManager::logNeedChanged(int entityId, const std::string& needName,
                                         float oldValue, float newValue) {
    logEvent(EventType::NEED_CHANGED, entityId, {
        {"need", needName},
        {"oldValue", std::to_string(oldValue)},
        {"newValue", std::to_string(newValue)}
    });
}

void EventStreamManager::logEmotionChanged(int entityId, const std::string& emotionName,
                                            float oldValue, float newValue) {
    logEvent(EventType::EMOTION_CHANGED, entityId, {
        {"emotion", emotionName},
        {"oldValue", std::to_string(oldValue)},
        {"newValue", std::to_string(newValue)}
    });
}

void EventStreamManager::logRelationshipChanged(int entityId1, int entityId2,
                                                 const std::string& relationType,
                                                 float oldValue, float newValue) {
    logEvent(EventType::RELATIONSHIP_CHANGED, entityId1, {
        {"targetEntity", std::to_string(entityId2)},
        {"relationType", relationType},
        {"oldValue", std::to_string(oldValue)},
        {"newValue", std::to_string(newValue)}
    });
}

void EventStreamManager::logDecision(int entityId, const std::string& decisionType,
                                      const std::vector<std::string>& options,
                                      const std::string& chosenOption,
                                      const std::string& reasoning) {
    std::ostringstream optionsStr;
    for (size_t i = 0; i < options.size(); ++i) {
        if (i > 0) optionsStr << ";";
        optionsStr << options[i];
    }
    
    logEvent(EventType::DECISION_MADE, entityId, {
        {"decisionType", decisionType},
        {"options", optionsStr.str()},
        {"chosenOption", chosenOption},
        {"reasoning", reasoning}
    });
}

std::vector<EventRecord> EventStreamManager::getRecentEvents(size_t count) const {
    return liveBuffer.getRecent(count);
}

std::vector<EventRecord> EventStreamManager::queryEvents(EventType type,
                                                          int64_t startTime,
                                                          int64_t endTime) const {
    auto events = liveBuffer.getRecent(liveBuffer.size());
    std::vector<EventRecord> filtered;
    
    for (const auto& event : events) {
        if (event.type == type && event.timestamp >= startTime && event.timestamp <= endTime) {
            filtered.push_back(event);
        }
    }
    
    return filtered;
}

void EventStreamManager::setEnabled(bool enable) {
    enabled.store(enable);
}

// ============= CsvExporter Implementation =============

CsvExporter::CsvExporter(const std::vector<std::string>& cols) : columns(cols) {}

void CsvExporter::addRow(const std::map<std::string, std::string>& row) {
    rows.push_back(row);
}

bool CsvExporter::exportToFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    
    // Write header
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) file << ",";
        file << columns[i];
    }
    file << "\n";
    
    // Write data
    for (const auto& row : rows) {
        for (size_t i = 0; i < columns.size(); ++i) {
            if (i > 0) file << ",";
            auto it = row.find(columns[i]);
            if (it != row.end()) {
                file << it->second;
            }
        }
        file << "\n";
    }
    
    return true;
}

// ============= JsonExporter Implementation =============

JsonExporter::JsonExporter(const std::string& arrName) : arrayName(arrName) {}

void JsonExporter::addRecord(const std::map<std::string, std::string>& record) {
    records.push_back(record);
}

bool JsonExporter::exportToFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    
    file << "{\n\"" << arrayName << "\":[\n";
    
    for (size_t i = 0; i < records.size(); ++i) {
        file << "  {";
        bool first = true;
        for (const auto& [key, value] : records[i]) {
            if (!first) file << ",";
            file << "\n    \"" << key << "\": \"" << value << "\"";
            first = false;
        }
        file << "\n  }";
        if (i < records.size() - 1) file << ",";
        file << "\n";
    }
    
    file << "]\n}\n";
    
    return true;
}

// ============= VisualizationFrame Implementation =============

std::string VisualizationFrame::toJson() const {
    std::ostringstream oss;
    oss << "{\"timestamp\":" << timestamp
        << ",\"entities\":[";
    
    for (size_t i = 0; i < entityStates.size(); ++i) {
        if (i > 0) oss << ",";
        oss << entityStates[i].toJson();
    }
    
    oss << "],\"metrics\":{";
    bool first = true;
    for (const auto& [name, value] : globalMetrics) {
        if (!first) oss << ",";
        oss << "\"" << name << "\":" << value;
        first = false;
    }
    
    oss << "},\"metadata\":\"" << metadata << "\"}";
    
    return oss.str();
}

// ============= StateObserver Implementation =============

void StateObserver::updateSnapshot(const EntityStateSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex);
    
    for (auto& existing : snapshots) {
        if (existing.entityId == snapshot.entityId) {
            existing = snapshot;
            return;
        }
    }
    
    snapshots.push_back(snapshot);
}

void StateObserver::removeSnapshot(int entityId) {
    std::lock_guard<std::mutex> lock(mutex);
    
    snapshots.erase(
        std::remove_if(snapshots.begin(), snapshots.end(),
            [entityId](const EntityStateSnapshot& s) {
                return s.entityId == entityId;
            }),
        snapshots.end());
}

std::vector<EntityStateSnapshot> StateObserver::getAllSnapshots() const {
    std::lock_guard<std::mutex> lock(mutex);
    return snapshots;
}

EntityStateSnapshot StateObserver::getSnapshot(int entityId) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    for (const auto& snapshot : snapshots) {
        if (snapshot.entityId == entityId) {
            return snapshot;
        }
    }
    
    return EntityStateSnapshot{};
}

void StateObserver::setMetric(const std::string& name, float value) {
    std::lock_guard<std::mutex> lock(mutex);
    metrics[name] = value;
}

float StateObserver::getMetric(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = metrics.find(name);
    return (it != metrics.end()) ? it->second : 0.0f;
}

void StateObserver::setCurrentTimestamp(int64_t ts) {
    currentTimestamp.store(ts);
}

VisualizationFrame StateObserver::getCurrentFrame() const {
    std::lock_guard<std::mutex> lock(mutex);
    
    VisualizationFrame frame;
    frame.timestamp = currentTimestamp.load();
    frame.entityStates = snapshots;
    frame.globalMetrics = metrics;
    
    return frame;
}

// ============= AnalyticsEngine Implementation =============

void AnalyticsEngine::recordDataPoint(const std::string& metric, double value, int64_t timestamp) {
    // Store as interleaved timestamp/value pairs in timeSeries
    auto& series = timeSeries[metric];
    series.push_back(value);
}

double AnalyticsEngine::computeMean(const std::string& metric) const {
    auto it = timeSeries.find(metric);
    if (it == timeSeries.end() || it->second.empty()) return 0.0;
    
    double sum = std::accumulate(it->second.begin(), it->second.end(), 0.0);
    return sum / it->second.size();
}

double AnalyticsEngine::computeVariance(const std::string& metric) const {
    auto it = timeSeries.find(metric);
    if (it == timeSeries.end() || it->second.size() < 2) return 0.0;
    
    double mean = computeMean(metric);
    double sumSq = 0.0;
    
    for (double val : it->second) {
        sumSq += (val - mean) * (val - mean);
    }
    
    return sumSq / (it->second.size() - 1);
}

double AnalyticsEngine::computeStdDev(const std::string& metric) const {
    return std::sqrt(computeVariance(metric));
}

double AnalyticsEngine::computeMin(const std::string& metric) const {
    auto it = timeSeries.find(metric);
    if (it == timeSeries.end() || it->second.empty()) return 0.0;
    
    return *std::min_element(it->second.begin(), it->second.end());
}

double AnalyticsEngine::computeMax(const std::string& metric) const {
    auto it = timeSeries.find(metric);
    if (it == timeSeries.end() || it->second.empty()) return 0.0;
    
    return *std::max_element(it->second.begin(), it->second.end());
}

double AnalyticsEngine::computePercentile(const std::string& metric, double p) const {
    auto it = timeSeries.find(metric);
    if (it == timeSeries.end() || it->second.empty()) return 0.0;
    
    std::vector<double> sorted = it->second;
    std::sort(sorted.begin(), sorted.end());
    
    size_t idx = static_cast<size_t>(p / 100.0 * (sorted.size() - 1));
    return sorted[idx];
}

double AnalyticsEngine::computeCorrelation(const std::string& metric1,
                                            const std::string& metric2) const {
    auto it1 = timeSeries.find(metric1);
    auto it2 = timeSeries.find(metric2);
    
    if (it1 == timeSeries.end() || it2 == timeSeries.end() ||
        it1->second.size() != it2->second.size() || it1->second.empty()) {
        return 0.0;
    }
    
    const auto& x = it1->second;
    const auto& y = it2->second;
    size_t n = x.size();
    
    double meanX = std::accumulate(x.begin(), x.end(), 0.0) / n;
    double meanY = std::accumulate(y.begin(), y.end(), 0.0) / n;
    
    double numerator = 0.0, sumSqX = 0.0, sumSqY = 0.0;
    
    for (size_t i = 0; i < n; ++i) {
        double dx = x[i] - meanX;
        double dy = y[i] - meanY;
        numerator += dx * dy;
        sumSqX += dx * dx;
        sumSqY += dy * dy;
    }
    
    double denominator = std::sqrt(sumSqX * sumSqY);
    return (denominator > 0) ? numerator / denominator : 0.0;
}

double AnalyticsEngine::computeTrend(const std::string& metric) const {
    // Simple linear regression slope
    auto it = timeSeries.find(metric);
    if (it == timeSeries.end() || it->second.size() < 2) return 0.0;
    
    const auto& y = it->second;
    size_t n = y.size();
    
    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumSqX = 0.0;
    
    for (size_t i = 0; i < n; ++i) {
        sumX += i;
        sumY += y[i];
        sumXY += i * y[i];
        sumSqX += i * i;
    }
    
    double denominator = n * sumSqX - sumX * sumX;
    return (denominator > 0) ? (n * sumXY - sumX * sumY) / denominator : 0.0;
}

std::vector<std::pair<int64_t, double>> AnalyticsEngine::getTimeSeries(const std::string& metric) const {
    std::vector<std::pair<int64_t, double>> result;
    
    auto it = timeSeries.find(metric);
    if (it == timeSeries.end()) return result;
    
    for (size_t i = 0; i < it->second.size(); ++i) {
        result.emplace_back(static_cast<int64_t>(i), it->second[i]);
    }
    
    return result;
}

void AnalyticsEngine::setAggregate(const std::string& name, double value) {
    aggregates[name] = value;
}

double AnalyticsEngine::getAggregate(const std::string& name) const {
    auto it = aggregates.find(name);
    return (it != aggregates.end()) ? it->second : 0.0;
}

std::string AnalyticsEngine::generateReport() const {
    std::ostringstream oss;
    oss << "Analytics Report\n==============\n\n";
    
    for (const auto& [metric, values] : timeSeries) {
        oss << metric << ":\n";
        oss << "  Mean: " << computeMean(metric) << "\n";
        oss << "  StdDev: " << computeStdDev(metric) << "\n";
        oss << "  Min: " << computeMin(metric) << "\n";
        oss << "  Max: " << computeMax(metric) << "\n";
        oss << "  Trend: " << computeTrend(metric) << "\n\n";
    }
    
    return oss.str();
}

// ============= DashboardData Implementation =============

std::string DashboardData::toJson() const {
    std::ostringstream oss;
    oss << "{\"totalEntities\":" << totalEntities
        << ",\"activeEntities\":" << activeEntities
        << ",\"averageHappiness\":" << averageHappiness
        << ",\"averageStress\":" << averageStress
        << ",\"simulationTime\":" << simulationTime
        << ",\"fps\":" << fps << "}";
    
    return oss.str();
}

// ============= Profiler Implementation =============

void Profiler::begin(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    startTimes[name] = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void Profiler::end(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto startIt = startTimes.find(name);
    if (startIt == startTimes.end()) return;
    
    int64_t endTime = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    int64_t duration = endTime - startIt->second;
    
    auto profIt = profiles.find(name);
    if (profIt == profiles.end()) {
        ProfileData data;
        data.name = name;
        data.totalTime = duration;
        data.callCount = 1;
        data.minTime = duration;
        data.maxTime = duration;
        profiles[name] = data;
    } else {
        profIt->second.totalTime += duration;
        profIt->second.callCount++;
        profIt->second.minTime = std::min(profIt->second.minTime, duration);
        profIt->second.maxTime = std::max(profIt->second.maxTime, duration);
    }
    
    startTimes.erase(startIt);
}

void Profiler::record(const std::string& name, int64_t duration) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto profIt = profiles.find(name);
    if (profIt == profiles.end()) {
        ProfileData data;
        data.name = name;
        data.totalTime = duration;
        data.callCount = 1;
        data.minTime = duration;
        data.maxTime = duration;
        profiles[name] = data;
    } else {
        profIt->second.totalTime += duration;
        profIt->second.callCount++;
        profIt->second.minTime = std::min(profIt->second.minTime, duration);
        profIt->second.maxTime = std::max(profIt->second.maxTime, duration);
    }
}

std::map<std::string, Profiler::ProfileData> Profiler::getProfiles() const {
    std::lock_guard<std::mutex> lock(mutex);
    return profiles;
}

std::string Profiler::generateReport() const {
    std::lock_guard<std::mutex> lock(mutex);
    
    std::ostringstream oss;
    oss << "Profiler Report\n===============\n\n";
    oss << std::left << std::setw(30) << "Function" 
        << std::right << std::setw(10) << "Calls"
        << std::setw(12) << "Total(ms)"
        << std::setw(10) << "Avg(us)"
        << std::setw(10) << "Min(us)"
        << std::setw(10) << "Max(us)" << "\n";
    oss << std::string(92, '-') << "\n";
    
    for (const auto& [name, data] : profiles) {
        double avgTime = static_cast<double>(data.totalTime) / data.callCount;
        oss << std::left << std::setw(30) << name
            << std::right << std::setw(10) << data.callCount
            << std::setw(12) << std::fixed << std::setprecision(2) << data.totalTime / 1000.0
            << std::setw(10) << std::fixed << std::setprecision(2) << avgTime
            << std::setw(10) << data.minTime
            << std::setw(10) << data.maxTime << "\n";
    }
    
    return oss.str();
}

void Profiler::reset() {
    std::lock_guard<std::mutex> lock(mutex);
    profiles.clear();
    startTimes.clear();
}

} // namespace observability
