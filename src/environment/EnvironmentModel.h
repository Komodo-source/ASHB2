#ifndef ENVIRONMENT_MODEL_H
#define ENVIRONMENT_MODEL_H

#include <vector>
#include <map>
#include <string>
#include <set>
#include <memory>
#include <functional>
#include <cmath>

namespace environment {

// Seasonal configuration
enum class Season {
    SPRING,
    SUMMER,
    AUTUMN,
    WINTER
};

struct SeasonalConfig {
    Season season;
    float temperature;      // 0-100
    float daylightHours;    // 0-24
    float precipitation;    // 0-100
    float resourceModifier; // Multiplier for resource availability
    
    std::string toString() const;
    static SeasonalConfig fromMonth(int month);
};

// Resource types
enum class ResourceType {
    FOOD,
    WATER,
    SHELTER,
    INFORMATION,
    ENERGY,
    MATERIALS
};

struct ResourceNode {
    int id;
    ResourceType type;
    float quantity;
    float maxQuantity;
    float regenerationRate;
    float x, y, z;  // Position
    bool renewable;
    int ownerId;    // -1 if unowned
    
    ResourceNode();
    ResourceNode(ResourceType t, float qty, float regen, 
                float px, float py, float pz);
    
    void update(float deltaTime);
    float harvest(float amount);
    bool isDepleted() const;
};

// Environmental state
struct EnvironmentalState {
    SeasonalConfig currentSeason;
    float globalTemperature;
    float humidity;
    float airQuality;
    float ambientNoise;
    int timeOfDay;        // 0-23 hours
    int dayOfYear;        // 1-365
    std::map<std::string, float> regionalModifiers;
    
    EnvironmentalState();
    void advanceTime(float hours);
    void updateSeasonalEffects();
};

// Resource dynamics manager
class ResourceManager {
private:
    std::vector<std::shared_ptr<ResourceNode>> resources;
    std::map<ResourceType, float> globalAvailability;
    float competitionFactor;
    
public:
    ResourceManager();
    
    void addResource(std::shared_ptr<ResourceNode> resource);
    void removeResource(int resourceId);
    
    // Find nearby resources
    std::vector<std::shared_ptr<ResourceNode>> findNearbyResources(
        float x, float y, float z, float radius, 
        ResourceType type = ResourceType::FOOD);
    
    // Harvest resource
    float harvestResource(int resourceId, int harvesterId, float amount);
    
    // Claim/release resources
    bool claimResource(int resourceId, int entityId);
    void releaseResource(int resourceId);
    
    // Update all resources
    void update(float deltaTime, const EnvironmentalState& env);
    
    // Get statistics
    float getTotalResource(ResourceType type) const;
    float getAvailabilityIndex(ResourceType type) const;
    
    // Competition modeling
    void setCompetitionFactor(float factor);
    float calculateCompetitionCost(int entityId, ResourceType type);
};

// Cultural transmission system
struct CulturalTrait {
    std::string name;
    std::string category;  // "belief", "practice", "norm", "skill"
    float prevalence;      // 0-1 in population
    float transmissionRate;
    float mutationRate;
    std::vector<std::string> prerequisites;
    std::function<float(float)> fitnessFunction;  // How trait affects survival

    // ── IV-P1 (Track IV): a trait is a THING, and it has an identity ──────────
    // `id` is its slot in the world catalogue, which is what a person actually
    // carries (one bit per trait in Entity::cultureTraits) — so "what culture
    // does this person have" is a concrete answer, not a score. `prestigious`
    // marks the traits that *signal* standing (letters, manners, fine dress);
    // holding them cultivates the cultural capital III-P4 reproduces.
    int  id          = -1;
    bool prestigious = false;
    // Which traits this one is an ALTERNATIVE to. A people burns its dead or
    // buries them; it does not do both, and taking up one way is how the other
    // is given up. Without rival families a culture just accumulates every
    // practice in the world and every people ends up holding all of them —
    // which is exactly how a long run converged on a single global culture.
    // -1 = a standalone practice nothing competes with.
    int  family      = -1;
};

struct CulturalGroup {
    int id;
    std::string name;
    std::vector<CulturalTrait> traits;
    std::vector<int> memberIds;
    float cohesion;      // 0-1
    float openness;      // Willingness to adopt external traits
    std::map<int, float> memberInfluence;  // Entity ID -> influence level
    
    void updateTraits();
    float calculateTraitAdoption(const CulturalTrait& trait);
};

// ── IV-P1 (Track IV): culture as transmissible content ───────────────────────
// Culture stops being a score and becomes a set of *things people do*: a fixed
// world catalogue of practices, beliefs, taboos, tastes and fashions, each of
// which a person either carries or does not. A culture is then a trait set, two
// cultures can be compared (Jaccard), and cultural change is what happens when
// traits move between heads — vertically (parent→child), horizontally (peer→
// peer) and obliquely (elder→young), with a mutation rate that invents new ones.
//
// The one non-obvious rule is the tipping point. Centola's experiments found
// that a committed minority does not convert a population gradually: below
// roughly a quarter of it, a new norm stays a quirk and usually dies; at that
// critical mass it cascades to the majority in short order. That threshold is
// what gives cultural history its punctuated shape — long stretches where
// nothing takes, then a decade in which everyone suddenly does the new thing.
// See plans/parallel-earth-upgrade.md §7 IV-P1.
//
// This class owns the catalogue and the maths; the civilisation layer drives
// it (CivilizationEngine::updateCulturalTraits) so every roll comes from the
// engine's seeded stream and the run stays replayable.
class CulturalTransmissionSystem {
private:
    std::vector<CulturalGroup> groups;
    std::map<int, int> entityToGroup;  // Entity ID -> Group ID
    std::vector<CulturalTrait> traitLibrary;

    float verticalTransmissionRate;   // Parent to child
    float horizontalTransmissionRate; // Peer to peer
    float obliqueTransmissionRate;    // Elder to young

public:
    CulturalTransmissionSystem();

    // A person's culture is a bit per catalogue trait, so it costs 8 bytes,
    // compares with an AND, and serialises as one number.
    static const int MAX_TRAITS = 64;
    // Centola et al. (2018): the committed-minority share at which a norm
    // stops being a quirk and cascades.
    static const int CRITICAL_MASS_PCT = 25;

    // The world's trait catalogue (fixed, ordered — index == trait id).
    const std::vector<CulturalTrait>& catalogue() const { return traitLibrary; }
    const CulturalTrait& trait(int id) const { return traitLibrary[id]; }
    int  traitCount() const { return (int)traitLibrary.size(); }

    static unsigned long long bit(int id) { return 1ull << id; }
    // Every OTHER trait that is an alternative to this one — the set a culture
    // must give up to take this one on. 0 for a standalone practice.
    unsigned long long rivals(int id) const;
    static bool holds(unsigned long long set, int id) { return (set & bit(id)) != 0ull; }
    static int  count(unsigned long long set);
    // Jaccard distance between two trait sets: 0 = the same culture, 1 = no
    // shared practice at all. Two empty cultures are identical, not distant.
    static float distance(unsigned long long a, unsigned long long b);

    // How likely a non-carrier is to pick this trait up today, given how many
    // of the people around them already carry it. Below the critical mass this
    // is deliberately feeble (most novelties fizzle); at or above it, adoption
    // multiplies — the cascade. `receptiveness` folds in how open the person and
    // their people are to a new way of doing things.
    float adoptionChance(int traitId, float prevalence, float receptiveness) const;
    // The matching abandonment odds: a practice too few people share is
    // embarrassing to keep up, and gets quietly dropped.
    float abandonChance(int traitId, float prevalence) const;

    float verticalRate()   const { return verticalTransmissionRate; }
    float horizontalRate() const { return horizontalTransmissionRate; }
    float obliqueRate()    const { return obliqueTransmissionRate; }

    void addGroup(const CulturalGroup& group);
    void assignEntityToGroup(int entityId, int groupId);

    // Transmission events
    void transmitVertically(int parentId, int childId);
    void transmitHorizontally(int sourceId, int targetId);
    void transmitObliquely(int elderId, int youthId);

    // Cultural evolution
    void updateCulturalTraits(float deltaTime);

    // Innovation and mutation
    void introduceInnovation(int groupId, const CulturalTrait& trait);

    // Get cultural state of entity
    std::vector<CulturalTrait> getEntityTraits(int entityId) const;

    // Calculate cultural distance between entities
    float calculateCulturalDistance(int entityId1, int entityId2) const;
};

// Institutional structures
enum class InstitutionType {
    FAMILY,
    EDUCATION,
    GOVERNMENT,
    RELIGION,
    ECONOMY,
    HEALTHCARE
};

struct Institution {
    int id = -1;
    InstitutionType type = InstitutionType::FAMILY;
    std::string name;
    std::vector<int> memberIds;
    std::map<std::string, float> rules;
    std::map<std::string, float> resources;
    float legitimacy = 0.5f;   // 0-1, how much members accept the institution
    float efficiency = 0.5f;   // How well it achieves its purpose
    int foundingDay = 0;

    // ── II-P2 (Track II): what makes an institution more than a label ────────
    // An institution belongs to a PEOPLE (so rival tribes hold rival schools),
    // and it can hold knowledge in its own right. `archive` is the whole point
    // of II-P2: a school or guild remembers a technique independently of any
    // living member, so knowledge stops being a property of individuals who can
    // all die in one bad winter. See plans/parallel-earth-upgrade.md §5 II-P2.
    int tribeId = -1;
    std::set<std::string> archive;   // techs/crafts the institution itself holds

    void updateLegitimacy();
    float enforceRule(const std::string& ruleName, int entityId);
    void distributeResources();
};

class InstitutionalSystem {
private:
    std::vector<std::shared_ptr<Institution>> institutions;
    std::map<int, std::vector<int>> entityMemberships;  // Entity -> Institutions

public:
    InstitutionalSystem();

    // Returns the new institution's id (-1 never happens; kept int so callers
    // can immediately addMember/archive against it).
    int  createInstitution(InstitutionType type, const std::string& name,
                           int tribeId = -1, int foundingDay = 0);
    void addMember(int institutionId, int entityId);
    void removeMember(int institutionId, int entityId);

    // Institutional effects on entities
    float getInstitutionalSupport(int entityId) const;
    std::vector<std::shared_ptr<Institution>> getEntityInstitutions(int entityId) const;

    // Rule enforcement
    float enforceNorms(int entityId, const std::string& action);

    // Update all institutions
    void update(float deltaTime);

    // ── II-P2 accessors used by the civilisation layer ───────────────────────
    // Lookup is by (people, kind): a tribe holds at most one of each kind.
    Institution*       find(int tribeId, InstitutionType type);
    const Institution* find(int tribeId, InstitutionType type) const;
    // True when ANY surviving institution has this technique written down —
    // the test `loseTechnology` asks before letting a collapse erase it.
    bool archiveHolds(const std::string& techName) const;
    // Wind an institution up (legitimacy collapse, or its people died out).
    void dissolve(int institutionId);
    void dissolveTribe(int tribeId);
    // Deterministic iteration for the civ tick / report (insertion order).
    const std::vector<std::shared_ptr<Institution>>& all() const { return institutions; }

    // Statistics
    size_t getInstitutionCount() const;
    size_t getMembershipCount(int entityId) const;
};

// Environmental feedback loops
struct FeedbackLoop {
    std::string name;
    bool positive;  // true = reinforcing, false = balancing
    
    std::function<float(const EnvironmentalState&)> sensor;
    std::function<void(EnvironmentalState&, float)> effect;
    
    float gain;
    float delay;
    float accumulatedEffect;
};

class EnvironmentalFeedbackSystem {
private:
    std::vector<FeedbackLoop> loops;
    EnvironmentalState* state;
    
public:
    explicit EnvironmentalFeedbackSystem(EnvironmentalState* envState);
    
    void addFeedbackLoop(const FeedbackLoop& loop);
    void update(float deltaTime);
    
    // Common feedback loops
    void setupPopulationPressureLoop();
    void setupResourceDepletionLoop();
    void setupClimateFeedbackLoop();
};

// World simulation
class WorldEnvironment {
private:
    EnvironmentalState state;
    ResourceManager resourceManager;
    CulturalTransmissionSystem cultureSystem;
    InstitutionalSystem institutionalSystem;
    EnvironmentalFeedbackSystem feedbackSystem;
    
    float worldSize;
    std::string climateZone;
    
public:
    WorldEnvironment(float size = 1000.0f, 
                    const std::string& climate = "temperate");
    
    // Time progression
    void tick(float deltaTime);
    void advanceDay();
    void advanceSeason();
    
    // Accessors
    EnvironmentalState& getState() { return state; }
    const EnvironmentalState& getState() const { return state; }
    ResourceManager& getResources() { return resourceManager; }
    CulturalTransmissionSystem& getCulture() { return cultureSystem; }
    InstitutionalSystem& getInstitutions() { return institutionalSystem; }
    
    // Environmental queries
    float getTemperatureAt(float x, float y) const;
    float getResourceDensity(ResourceType type, float x, float y, float radius) const;
    Season getCurrentSeason() const { return state.currentSeason.season; }
    
    // Serialization
    void saveState(const std::string& filename) const;
    void loadState(const std::string& filename);
};

} // namespace environment

#endif // ENVIRONMENT_MODEL_H
