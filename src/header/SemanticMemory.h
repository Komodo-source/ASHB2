#ifndef SEMANTIC_MEMORY_H
#define SEMANTIC_MEMORY_H

#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>
#include <memory>
#include <set>

class Entity;

// ============================================================================
// Memory Vector: An embedded memory with metadata for similarity search
// ============================================================================
struct MemoryVector {
    // The embedding vector (features encoding the memory's semantics)
    std::vector<float> embedding;
    
    // Source memory metadata
    int memoryIndex;              // Index in the entity's lifeMemories vector
    std::string eventType;        // Type of event (e.g., "loss_death", "positive_bond", "trauma")
    int entityInvolvedId;         // Entity involved in the memory (-1 if none)
    float emotionalIntensity;     // How emotionally charged the memory is
    int simulationDay;            // When the event happened
    bool isFormative;             // Whether this was a formative experience
    std::string internalNarrative; // Text description of the memory
    
    // Retrieval metadata
    float relevanceScore;         // Computed similarity to query
    
    MemoryVector() 
        : memoryIndex(-1), entityInvolvedId(-1), emotionalIntensity(0.0f),
          simulationDay(0), isFormative(false), relevanceScore(0.0f) {}
};

// ============================================================================
// MemoryQuery: Parameters for querying the semantic memory
// ============================================================================
struct MemoryQuery {
    std::string contextType;      // e.g., "encounter_entity", "location", "action_type"
    int targetEntityId;           // Entity being encountered (-1 if none)
    std::string actionType;       // Type of action being considered
    std::vector<float> contextEmbedding; // Optional: embedding of current context
    
    MemoryQuery() : targetEntityId(-1) {}
};

// ============================================================================
// MemorySearchResult: Result from a semantic memory search
// ============================================================================
struct MemorySearchResult {
    std::vector<MemoryVector> results;
    std::string summaryContext;   // Brief textual summary of retrieved memories
    
    bool hasRelevantMemories() const { return !results.empty(); }
};

// ============================================================================
// EmbeddingConfig: Configuration for the embedding generation
// ============================================================================
struct EmbeddingConfig {
    int embeddingDimension;       // Size of embedding vectors
    bool useKeywordEncoding;      // Encode narrative keywords
    float emotionalWeight;        // Weight of emotional features
    float recencyWeight;          // Weight of recency in retrieval
    
    EmbeddingConfig() 
        : embeddingDimension(64), useKeywordEncoding(true),
          emotionalWeight(1.0f), recencyWeight(0.8f) {}
};

// ============================================================================
// SemanticMemorySystem: Main class for semantic memory management
// ============================================================================
class SemanticMemorySystem {
private:
    // Database of all embedded memories for the entity
    std::vector<MemoryVector> memoryDatabase;
    
    // Cache: entity ID -> list of related memory indices
    std::map<int, std::vector<int>> entityMemoryIndex;
    
    // Configuration
    EmbeddingConfig config;
    
    // Random generator for exploration noise in retrieval
    std::mt19937 rng;
    
    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    // Generate embedding from a LifeMemory (called via Entity reference)
    std::vector<float> generateEmbedding(const std::string& eventType,
                                          float emotionalIntensity,
                                          bool isFormative,
                                          int dayAge,
                                          const std::string& narrative);
    
    // Extract keywords from narrative text
    std::vector<float> extractKeywordFeatures(const std::string& text);
    
    // Encode event type into categorical features
    std::vector<float> encodeEventType(const std::string& eventType);
    
    // Compute cosine similarity between two vectors
    float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);
    
    // Apply decay to retrieval score based on memory age
    float applyTimeDecay(int memoryDay, float baseScore);
    
    // Build a text summary of retrieved memories
    std::string buildMemorySummary(const std::vector<MemoryVector>& results);
    
    // Compute context embedding from current state parameters
    std::vector<float> computeContextEmbedding(const std::string& contextType, 
                                                int targetEntityId = -1,
                                                const std::string& actionType = "");

public:
    SemanticMemorySystem();
    explicit SemanticMemorySystem(const EmbeddingConfig& cfg);
    
    // ========================================================================
    // Memory Management
    // ========================================================================
    
    // Index a new memory into the vector database
    void indexMemory(const std::string& eventType,
                     int entityInvolvedId,
                     float emotionalIntensity,
                     bool isFormative,
                     int simulationDay,
                     const std::string& narrative,
                     int memoryIndex);
    
    // Rebuild the entire memory database from an entity's life memories
    void rebuildFromLifeMemories(Entity* entity);
    
    // Clear all memories
    void clear();
    
    // ========================================================================
    // Memory Retrieval
    // ========================================================================
    
    // Search for memories most relevant to the current context
    MemorySearchResult searchRelevantMemories(const MemoryQuery& query, int topK = 5);
    
    // Get memories related to a specific entity
    MemorySearchResult getMemoriesAboutEntity(int entityId, int topK = 10);
    
    // Get the most emotionally significant memories
    MemorySearchResult getSignificantMemories(int topK = 3, float intensityThreshold = 0.6f);
    
    // ========================================================================
    // Decision Integration
    // ========================================================================
    
    // Calculate a memory bias for a specific action given current context
    float calculateMemoryActionBias(const std::string& actionName,
                                     const MemoryQuery& query);
    
    // Generate deliberation rationale text from recent memories
    std::string generateMemoryRationale(const MemorySearchResult& memories);
    
    // ========================================================================
    // Persistence
    // ========================================================================
    
    void saveTo(std::ofstream& file) const;
    void loadFrom(std::ifstream& file);
    
    // Debug / introspection
    int getMemoryCount() const { return (int)memoryDatabase.size(); }
    std::vector<MemoryVector> getAllMemories() const { return memoryDatabase; }
};

#endif // SEMANTIC_MEMORY_H
