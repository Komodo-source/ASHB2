#include "./header/SemanticMemory.h"
#include "./header/Entity.h"
#include "./header/FreeWillSystem.h"
#include "./header/WorldSeed.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <queue>

// ============================================================================
// SemanticMemorySystem Implementation
// ============================================================================

SemanticMemorySystem::SemanticMemorySystem()
    : rng(static_cast<std::mt19937::result_type>(nextDeterministicSeed(0x53E3'4E3ull)))
{
    config = EmbeddingConfig();
}

SemanticMemorySystem::SemanticMemorySystem(const EmbeddingConfig& cfg)
    : config(cfg), rng(static_cast<std::mt19937::result_type>(nextDeterministicSeed(0x53E3'4E3ull)))
{}

// ============================================================================
// Embedding Generation
// ============================================================================

std::vector<float> SemanticMemorySystem::encodeEventType(const std::string& eventType) {
    std::vector<float> features(6, 0.0f);
    
    if (eventType == "loss_death" || eventType == "death") {
        features[0] = 1.0f;
    } else if (eventType == "positive_bond" || eventType == "bond" || eventType == "couple") {
        features[1] = 1.0f;
    } else if (eventType == "trauma" || eventType == "violence" || eventType == "attack") {
        features[2] = 1.0f;
    } else if (eventType == "achievement" || eventType == "career" || eventType == "success") {
        features[3] = 1.0f;
    } else if (eventType == "betrayal" || eventType == "conflict" || eventType == "argument") {
        features[4] = 1.0f;
    } else if (eventType == "birth" || eventType == "child" || eventType == "family") {
        features[5] = 1.0f;
    }
    
    if (std::all_of(features.begin(), features.end(), [](float f) { return f == 0.0f; })) {
        for (auto& f : features) f = 1.0f / features.size();
    }
    
    return features;
}

std::vector<float> SemanticMemorySystem::extractKeywordFeatures(const std::string& text) {
    std::vector<float> features(8, 0.0f);
    if (text.empty()) return features;
    
    features[0] = std::min(1.0f, text.length() / 200.0f);
    
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    size_t posCount = 0;
    std::vector<std::string> positiveWords = {"happy", "love", "joy", "grateful", "wonderful", 
                                               "beautiful", "excited", "peace", "hope", "kind"};
    for (const auto& word : positiveWords) {
        size_t pos = 0;
        while ((pos = lower.find(word, pos)) != std::string::npos) { posCount++; pos += word.length(); }
    }
    features[1] = std::min(1.0f, posCount / 5.0f);
    
    size_t negCount = 0;
    std::vector<std::string> negativeWords = {"hate", "angry", "scared", "pain", "sad", 
                                               "alone", "fear", "hurt", "dark", "death"};
    for (const auto& word : negativeWords) {
        size_t pos = 0;
        while ((pos = lower.find(word, pos)) != std::string::npos) { negCount++; pos += word.length(); }
    }
    features[2] = std::min(1.0f, negCount / 5.0f);
    
    size_t socialCount = 0;
    std::vector<std::string> socialWords = {"friend", "family", "together", "people", "group", 
                                             "community", "partner", "mother", "father", "child"};
    for (const auto& word : socialWords) {
        size_t pos = 0;
        while ((pos = lower.find(word, pos)) != std::string::npos) { socialCount++; pos += word.length(); }
    }
    features[3] = std::min(1.0f, socialCount / 5.0f);
    
    size_t threatCount = 0;
    std::vector<std::string> threatWords = {"danger", "threat", "enemy", "betray", "attack", 
                                             "kill", "hurt", "war", "conflict", "stranger"};
    for (const auto& word : threatWords) {
        size_t pos = 0;
        while ((pos = lower.find(word, pos)) != std::string::npos) { threatCount++; pos += word.length(); }
    }
    features[4] = std::min(1.0f, threatCount / 5.0f);
    
    size_t trustCount = 0;
    std::vector<std::string> trustWords = {"trust", "honest", "true", "loyal", "faith", 
                                            "rely", "depend", "safe", "secure", "believe"};
    for (const auto& word : trustWords) {
        size_t pos = 0;
        while ((pos = lower.find(word, pos)) != std::string::npos) { trustCount++; pos += word.length(); }
    }
    features[5] = std::min(1.0f, trustCount / 5.0f);
    
    features[6] = posCount > negCount ? 1.0f : (negCount > posCount ? -1.0f : 0.0f);
    
    std::set<char> uniqueChars;
    for (char c : text) uniqueChars.insert(c);
    features[7] = text.empty() ? 0.0f : std::min(1.0f, uniqueChars.size() / 20.0f);
    
    return features;
}

std::vector<float> SemanticMemorySystem::generateEmbedding(
    const std::string& eventType,
    float emotionalIntensity,
    bool isFormative,
    int dayAge,
    const std::string& narrative)
{
    std::vector<float> embedding;
    embedding.reserve(config.embeddingDimension);
    
    auto eventFeatures = encodeEventType(eventType);
    embedding.insert(embedding.end(), eventFeatures.begin(), eventFeatures.end());
    
    embedding.push_back(emotionalIntensity);
    embedding.push_back(isFormative ? 1.0f : 0.0f);
    embedding.push_back(std::tanh(emotionalIntensity * 0.5f));
    embedding.push_back(emotionalIntensity * (isFormative ? 1.5f : 1.0f));
    
    float recency = std::exp(-dayAge / 300.0f);
    embedding.push_back(recency);
    embedding.push_back(1.0f - recency);
    
    if (config.useKeywordEncoding) {
        auto keywordFeatures = extractKeywordFeatures(narrative);
        embedding.insert(embedding.end(), keywordFeatures.begin(), keywordFeatures.end());
    }
    
    size_t currentSize = embedding.size();
    if (currentSize < (size_t)config.embeddingDimension) {
        std::hash<std::string> hasher;
        size_t seed = hasher(eventType) ^ (hasher(narrative) << 1);
        
        for (int i = (int)currentSize; i < config.embeddingDimension; ++i) {
            size_t h = seed ^ (i * 0x9e3779b9);
            h = (h ^ (h >> 16)) * 0x85ebca6b;
            h = h ^ (h >> 13);
            float val = (h % 1000) / 1000.0f;
            embedding.push_back(val);
        }
    } else if (currentSize > (size_t)config.embeddingDimension) {
        embedding.resize(config.embeddingDimension);
    }
    
    float magnitude = 0.0f;
    for (float v : embedding) magnitude += v * v;
    magnitude = std::sqrt(magnitude);
    if (magnitude > 0.0001f) {
        for (auto& v : embedding) v /= magnitude;
    }
    
    return embedding;
}

// ============================================================================
// Similarity Computation
// ============================================================================

float SemanticMemorySystem::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0f;
    
    float dotProduct = 0.0f;
    float magA = 0.0f;
    float magB = 0.0f;
    
    for (size_t i = 0; i < a.size(); ++i) {
        dotProduct += a[i] * b[i];
        magA += a[i] * a[i];
        magB += b[i] * b[i];
    }
    
    magA = std::sqrt(magA);
    magB = std::sqrt(magB);
    
    if (magA < 0.0001f || magB < 0.0001f) return 0.0f;
    
    return dotProduct / (magA * magB);
}

float SemanticMemorySystem::applyTimeDecay(int memoryDay, float baseScore) {
    int currentDay = FreeWillSystem::day;
    int age = std::max(0, currentDay - memoryDay);
    float decayFactor = std::exp(-age / 300.0f);
    return baseScore * (config.recencyWeight * decayFactor + (1.0f - config.recencyWeight));
}

std::vector<float> SemanticMemorySystem::computeContextEmbedding(
    const std::string& contextType,
    int targetEntityId,
    const std::string& actionType)
{
    std::vector<float> ctxEmbed(config.embeddingDimension, 0.0f);
    
    if (contextType == "encounter_entity") {
        ctxEmbed[0] = 1.0f;
        ctxEmbed[1] = 0.5f;
    } else if (contextType == "location") {
        ctxEmbed[2] = 1.0f;
    } else if (contextType == "action_type") {
        ctxEmbed[3] = 1.0f;
    }
    
    if (!actionType.empty()) {
        auto actionFeatures = encodeEventType(actionType);
        for (size_t i = 0; i < actionFeatures.size() && i < ctxEmbed.size(); ++i) {
            ctxEmbed[i + 4] = actionFeatures[i];
        }
    }
    
    float mag = 0.0f;
    for (float v : ctxEmbed) mag += v * v;
    mag = std::sqrt(mag);
    if (mag > 0.0001f) {
        for (auto& v : ctxEmbed) v /= mag;
    }
    
    return ctxEmbed;
}

// ============================================================================
// Memory Summary Generation
// ============================================================================

std::string SemanticMemorySystem::buildMemorySummary(const std::vector<MemoryVector>& results) {
    if (results.empty()) return "";
    
    std::ostringstream summary;
    summary << "Recent relevant memories (" << results.size() << "): ";
    
    int count = 0;
    for (const auto& mem : results) {
        if (count >= 3) break;
        if (count > 0) summary << " | ";
        
        summary << mem.eventType;
        if (mem.entityInvolvedId >= 0) {
            summary << " [entity:" << mem.entityInvolvedId << "]";
        }
        if (!mem.internalNarrative.empty()) {
            std::string narr = mem.internalNarrative;
            if (narr.length() > 40) narr = narr.substr(0, 40) + "...";
            summary << " \"" << narr << "\"";
        }
        summary << " (intensity:" << std::fixed << std::setprecision(2) << mem.emotionalIntensity << ")";
        count++;
    }
    
    return summary.str();
}

// ============================================================================
// Memory Management
// ============================================================================

void SemanticMemorySystem::indexMemory(const std::string& eventType,
                                        int entityInvolvedId,
                                        float emotionalIntensity,
                                        bool isFormative,
                                        int simulationDay,
                                        const std::string& narrative,
                                        int memoryIndex)
{
    MemoryVector mv;
    mv.memoryIndex = memoryIndex;
    mv.eventType = eventType;
    mv.entityInvolvedId = entityInvolvedId;
    mv.emotionalIntensity = emotionalIntensity;
    mv.simulationDay = simulationDay;
    mv.isFormative = isFormative;
    mv.internalNarrative = narrative;
    
    mv.embedding = generateEmbedding(eventType, emotionalIntensity, 
                                      isFormative, simulationDay, narrative);
    
    memoryDatabase.push_back(mv);
    
    if (entityInvolvedId >= 0) {
        entityMemoryIndex[entityInvolvedId].push_back((int)memoryDatabase.size() - 1);
    }
}

void SemanticMemorySystem::rebuildFromLifeMemories(Entity* entity) {
    clear();
    
    for (size_t i = 0; i < entity->lifeMemories.size(); ++i) {
        const auto& lm = entity->lifeMemories[i];
        indexMemory(lm.eventType, lm.entityInvolvedId, lm.emotionalIntensity,
                    lm.isFormative, lm.simulationDay, lm.internalNarrative, (int)i);
    }
}

void SemanticMemorySystem::clear() {
    memoryDatabase.clear();
    entityMemoryIndex.clear();
}

// ============================================================================
// Memory Retrieval
// ============================================================================

MemorySearchResult SemanticMemorySystem::searchRelevantMemories(const MemoryQuery& query, int topK) {
    MemorySearchResult result;
    
    if (memoryDatabase.empty()) return result;
    
    std::vector<float> queryEmbed;
    if (!query.contextEmbedding.empty()) {
        queryEmbed = query.contextEmbedding;
    } else {
        queryEmbed = computeContextEmbedding(query.contextType, 
                                              query.targetEntityId, 
                                              query.actionType);
    }
    
    std::vector<std::pair<float, int>> scoredMemories;
    for (size_t i = 0; i < memoryDatabase.size(); ++i) {
        float sim = cosineSimilarity(queryEmbed, memoryDatabase[i].embedding);
        
        float entityBoost = 1.0f;
        if (query.targetEntityId >= 0 && memoryDatabase[i].entityInvolvedId == query.targetEntityId) {
            entityBoost = 1.5f;
        }
        
        float formativeBoost = memoryDatabase[i].isFormative ? 1.3f : 1.0f;
        float decayedScore = applyTimeDecay(memoryDatabase[i].simulationDay, sim);
        float emotionalBonus = 1.0f + memoryDatabase[i].emotionalIntensity * 0.3f;
        
        float finalScore = decayedScore * entityBoost * formativeBoost * emotionalBonus;
        scoredMemories.push_back({finalScore, (int)i});
    }
    
    std::sort(scoredMemories.begin(), scoredMemories.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    int limit = std::min(topK, (int)scoredMemories.size());
    for (int i = 0; i < limit; ++i) {
        MemoryVector mv = memoryDatabase[scoredMemories[i].second];
        mv.relevanceScore = scoredMemories[i].first;
        result.results.push_back(mv);
    }
    
    result.summaryContext = buildMemorySummary(result.results);
    
    return result;
}

MemorySearchResult SemanticMemorySystem::getMemoriesAboutEntity(int entityId, int topK) {
    MemorySearchResult result;
    
    auto it = entityMemoryIndex.find(entityId);
    if (it == entityMemoryIndex.end()) return result;
    
    std::vector<std::pair<float, int>> scoredMemories;
    for (int idx : it->second) {
        if (idx < (int)memoryDatabase.size()) {
            float score = memoryDatabase[idx].emotionalIntensity * 
                          (memoryDatabase[idx].isFormative ? 1.5f : 1.0f);
            scoredMemories.push_back({score, idx});
        }
    }
    
    std::sort(scoredMemories.begin(), scoredMemories.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    int limit = std::min(topK, (int)scoredMemories.size());
    for (int i = 0; i < limit; ++i) {
        MemoryVector mv = memoryDatabase[scoredMemories[i].second];
        mv.relevanceScore = scoredMemories[i].first;
        result.results.push_back(mv);
    }
    
    result.summaryContext = buildMemorySummary(result.results);
    
    return result;
}

MemorySearchResult SemanticMemorySystem::getSignificantMemories(int topK, float intensityThreshold) {
    MemorySearchResult result;
    
    std::vector<std::pair<float, int>> scored;
    for (size_t i = 0; i < memoryDatabase.size(); ++i) {
        if (memoryDatabase[i].emotionalIntensity >= intensityThreshold || memoryDatabase[i].isFormative) {
            float score = memoryDatabase[i].emotionalIntensity * 
                         (memoryDatabase[i].isFormative ? 2.0f : 1.0f);
            scored.push_back({score, (int)i});
        }
    }
    
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    int limit = std::min(topK, (int)scored.size());
    for (int i = 0; i < limit; ++i) {
        MemoryVector mv = memoryDatabase[scored[i].second];
        mv.relevanceScore = scored[i].first;
        result.results.push_back(mv);
    }
    
    result.summaryContext = buildMemorySummary(result.results);
    
    return result;
}

// ============================================================================
// Decision Integration
// ============================================================================

float SemanticMemorySystem::calculateMemoryActionBias(const std::string& actionName,
                                                       const MemoryQuery& query)
{
    if (memoryDatabase.empty()) return 0.0f;
    
    MemorySearchResult memories = searchRelevantMemories(query, 3);
    if (!memories.hasRelevantMemories()) return 0.0f;
    
    float totalBias = 0.0f;
    float totalWeight = 0.0f;
    
    for (const auto& mem : memories.results) {
        float weight = mem.relevanceScore * mem.emotionalIntensity;
        float actionBias = 0.0f;
        
        if (mem.eventType == "positive_bond") {
            if (actionName == "socialize" || actionName == "flirt" || 
                actionName == "make_friends" || actionName == "kind_action") {
                actionBias = 0.15f;
            }
        }
        
        if (mem.eventType == "trauma" || mem.eventType == "loss_death") {
            if (actionName == "socialize" || actionName == "flirt" || actionName == "make_friends") {
                actionBias = -0.2f;
            }
            if (actionName == "attack" || actionName == "kill" || actionName == "defend") {
                actionBias = 0.1f;
            }
        }
        
        if (mem.eventType == "betrayal") {
            if (actionName == "trust" || actionName == "cooperate" || actionName == "share") {
                actionBias = -0.15f;
            }
        }
        
        if (mem.eventType == "achievement") {
            if (actionName == "work" || actionName == "build_career" || actionName == "learn") {
                actionBias = 0.12f;
            }
        }
        
        totalBias += actionBias * weight;
        totalWeight += weight;
    }
    
    if (totalWeight > 0.0f) {
        return totalBias / totalWeight;
    }
    
    return 0.0f;
}

std::string SemanticMemorySystem::generateMemoryRationale(const MemorySearchResult& memories) {
    if (!memories.hasRelevantMemories()) return "No relevant memories to consider.";
    
    std::ostringstream rationale;
    rationale << "Entity recalls ";
    
    int count = 0;
    for (const auto& mem : memories.results) {
        if (count >= 2) break;
        if (count > 0) rationale << " and ";
        
        if (mem.eventType == "loss_death") rationale << "the loss";
        else if (mem.eventType == "positive_bond") rationale << "a positive bond";
        else if (mem.eventType == "trauma") rationale << "a traumatic event";
        else if (mem.eventType == "betrayal") rationale << "a betrayal";
        else if (mem.eventType == "achievement") rationale << "an achievement";
        else if (mem.eventType == "birth" || mem.eventType == "child" || mem.eventType == "family") rationale << "a family event";
        else rationale << "an event";
        
        if (mem.entityInvolvedId >= 0) {
            rationale << " involving entity " << mem.entityInvolvedId;
        }
        if (!mem.internalNarrative.empty()) {
            rationale << ": \"" << mem.internalNarrative << "\"";
        }
        count++;
    }
    
    return rationale.str();
}

// ============================================================================
// Text-based Persistence (matching project convention)
// ============================================================================

void SemanticMemorySystem::saveTo(std::ofstream& file) const {
    file << "SEMANTIC_MEMORY_V2:\n";
    file << "DB_SIZE:" << memoryDatabase.size() << "\n";
    for (const auto& mv : memoryDatabase) {
        file << "MV:" << mv.memoryIndex << ","
             << mv.eventType << ","
             << mv.entityInvolvedId << ","
             << mv.emotionalIntensity << ","
             << mv.simulationDay << ","
             << mv.isFormative << ","
             << mv.internalNarrative << "\n";
    }
    file << "IDX_SIZE:" << entityMemoryIndex.size() << "\n";
    for (const auto& [key, val] : entityMemoryIndex) {
        file << "IDX:" << key << ",";
        for (size_t i = 0; i < val.size(); ++i) {
            if (i > 0) file << ";";
            file << val[i];
        }
        file << "\n";
    }
}

void SemanticMemorySystem::loadFrom(std::ifstream& file) {
    clear();
    std::string line;
    
    // Read header
    std::getline(file, line); // SEMANTIC_MEMORY_V2:
    
    // Read database size
    std::getline(file, line);
    if (line.substr(0, 8) != "DB_SIZE:") return;
    size_t dbSize = std::stoul(line.substr(8));
    memoryDatabase.resize(dbSize);
    
    for (size_t i = 0; i < dbSize; ++i) {
        std::getline(file, line); // MV:...
        if (line.substr(0, 3) != "MV:") continue;
        std::string data = line.substr(3);
        
        auto& mv = memoryDatabase[i];
        size_t c1 = data.find(',');
        size_t c2 = data.find(',', c1 + 1);
        size_t c3 = data.find(',', c2 + 1);
        size_t c4 = data.find(',', c3 + 1);
        size_t c5 = data.find(',', c4 + 1);
        size_t c6 = data.find(',', c5 + 1);
        
        if (c1 != std::string::npos) mv.memoryIndex = std::stoi(data.substr(0, c1));
        if (c2 != std::string::npos) mv.eventType = data.substr(c1 + 1, c2 - c1 - 1);
        if (c3 != std::string::npos) mv.entityInvolvedId = std::stoi(data.substr(c2 + 1, c3 - c2 - 1));
        if (c4 != std::string::npos) mv.emotionalIntensity = std::stof(data.substr(c3 + 1, c4 - c3 - 1));
        if (c5 != std::string::npos) mv.simulationDay = std::stoi(data.substr(c4 + 1, c5 - c4 - 1));
        if (c6 != std::string::npos) mv.isFormative = (data.substr(c5 + 1, c6 - c5 - 1) == "1");
        if (c6 != std::string::npos) mv.internalNarrative = data.substr(c6 + 1);
        
        mv.embedding = std::vector<float>(config.embeddingDimension, 0.0f);
    }
    
    // Read entity index
    std::getline(file, line);
    if (line.substr(0, 9) != "IDX_SIZE:") return;
    size_t idxSize = std::stoul(line.substr(9));
    
    for (size_t i = 0; i < idxSize; ++i) {
        std::getline(file, line); // IDX:...
        if (line.substr(0, 4) != "IDX:") continue;
        std::string idxData = line.substr(4);
        size_t comma = idxData.find(',');
        if (comma == std::string::npos) continue;
        
        int key = std::stoi(idxData.substr(0, comma));
        std::string vals = idxData.substr(comma + 1);
        std::vector<int> vec;
        
        size_t start = 0, end = 0;
        while ((end = vals.find(';', start)) != std::string::npos) {
            vec.push_back(std::stoi(vals.substr(start, end - start)));
            start = end + 1;
        }
        if (start < vals.length()) vec.push_back(std::stoi(vals.substr(start)));
        
        entityMemoryIndex[key] = vec;
    }
}
