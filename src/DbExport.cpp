#include "./header/DbExport.h"
#include "./header/Entity.h"
#include "./header/Economics.h"
#include "./header/SocialOrder.h"

#include <fstream>
#include <iostream>
#include <string>
#include <cstdio>       // snprintf, rename
#include <cstdlib>      // getenv
#include <cmath>        // isfinite
#include <ctime>
#include <sys/stat.h>
#ifdef _WIN32
  #include <direct.h>   // _mkdir
  #include <process.h>  // _getpid
  #define ASHB_MKDIR(p) _mkdir(p)
  #define ASHB_GETPID() _getpid()
#else
  #include <sys/types.h>
  #include <unistd.h>   // getpid
  #define ASHB_MKDIR(p) mkdir(p, 0755)
  #define ASHB_GETPID() getpid()
#endif

namespace dbexport {
namespace {

// Rows per chunk. Small enough that the loader sees fresh data every few
// seconds of a headless run, large enough that a chunk is one worthwhile COPY
// rather than a transaction per entity.
const long long CHUNK_ROWS = 5000;

bool          g_enabled = false;
std::string   g_dir;
int           g_world   = 1;
std::string   g_label;
std::string   g_seed;

std::ofstream g_chunk;
long long     g_rows    = 0;
int           g_counter = 0;
std::string   g_stamp;   // "<epoch>_<pid>", fixed for the life of the process

// ── chunk naming ────────────────────────────────────────────────────────────
// Names must sort oldest-first (the loader ingests in glob order) and must
// never collide with a chunk left behind by an earlier run — std::rename fails
// on Windows when the destination exists, so a per-run counter starting at zero
// would silently drop data whenever the loader was behind. A fixed-width epoch
// and the pid make every name unique across runs and still lexicographically
// ordered, with no directory scan needed.
std::string chunkBase() {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s_%06d", g_stamp.c_str(), g_counter);
    return g_dir + "/" + buf;
}
std::string tmpPath()   { return chunkBase() + ".ndjson.tmp"; }
std::string finalPath() { return chunkBase() + ".ndjson"; }

void openChunk() {
    g_chunk.open(tmpPath().c_str(), std::ios::app);
    if (!g_chunk.is_open()) {
        std::cerr << "DBEXPORT: cannot open " << tmpPath() << " — export disabled\n";
        g_enabled = false;
    }
}

// ── JSON helpers ────────────────────────────────────────────────────────────
// Each appends `"key":value,`; the trailing comma is popped when the line is
// closed. Hand-rolled to keep the engine free of a JSON dependency, same as
// exportTickHistory in SaveLoad.cpp.

void esc(std::string& o, const std::string& s) {
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                // Control characters are illegal raw in JSON; a stray one in a
                // name or monologue would break the loader's parse for the
                // whole chunk.
                if (static_cast<unsigned char>(c) < 0x20) {
                    char u[8];
                    std::snprintf(u, sizeof u, "\\u%04x", c);
                    o += u;
                } else {
                    o += c;
                }
        }
    }
}

void key(std::string& o, const char* k) { o += '"'; o += k; o += "\":"; }

void kInt(std::string& o, const char* k, long long v) {
    char b[24]; std::snprintf(b, sizeof b, "%lld", v);
    key(o, k); o += b; o += ',';
}

// Engine sentinels (-1 "none", -2 "never checked", -99999 "never happened")
// become SQL NULL rather than magic numbers the dashboard would have to know.
void kIntN(std::string& o, const char* k, long long v, long long sentinel) {
    if (v == sentinel) { key(o, k); o += "null,"; return; }
    kInt(o, k, v);
}

void kF(std::string& o, const char* k, double v) {
    key(o, k);
    if (!std::isfinite(v)) { o += "null,"; return; }   // NaN/inf is not valid JSON
    char b[40]; std::snprintf(b, sizeof b, "%.4g", v);
    o += b; o += ',';
}

void kBool(std::string& o, const char* k, bool v) {
    key(o, k); o += (v ? "true," : "false,");
}

// Empty string means "unset" throughout the engine, so it maps to NULL.
void kStr(std::string& o, const char* k, const std::string& v) {
    key(o, k);
    if (v.empty()) { o += "null,"; return; }
    o += '"'; esc(o, v); o += "\",";
}

// unsigned long long → 64 literal bits, which is exactly what PostgreSQL's
// bit(64) takes. Avoids the sign reinterpretation a bigint round-trip would do.
void kBits(std::string& o, const char* k, unsigned long long v) {
    char b[65];
    for (int i = 0; i < 64; ++i) b[63 - i] = ((v >> i) & 1ull) ? '1' : '0';
    b[64] = '\0';
    key(o, k); o += '"'; o += b; o += "\",";
}

// Close the object, drop the trailing comma, hand the line to the chunk.
void emit(std::string& o) {
    if (!o.empty() && o.back() == ',') o.pop_back();
    o += "}\n";
    if (!g_chunk.is_open()) openChunk();
    if (!g_enabled) return;
    g_chunk << o;
    if (++g_rows >= CHUNK_ROWS) flush();
}

void beginRow(std::string& o, const char* table, int day) {
    o.clear();
    o += '{';
    kStr(o, "t", table);
    kInt(o, "world_id", g_world);
    kInt(o, "last_seen_day", day);
}

// The target of a pointed association. The pointer is authoritative — the
// cached Id can be stale after a load — but fall back to it when the pointer
// is null rather than dropping the edge.
int targetId(const Entity* p, int cachedId) {
    return p ? p->entityId : cachedId;
}

} // namespace

// ─── public API ─────────────────────────────────────────────────────────────

bool enabled() { return g_enabled; }

void configure(const std::string& spoolDir, int worldId,
               const std::string& label, const std::string& seed) {
    g_dir   = spoolDir;
    g_world = worldId;
    g_label = label;
    g_seed  = seed;

    // ASHB2_LABEL names the world in sim.world. Same scheduler-friendly idea as
    // ASHB_BACKUP_EVERY: the caller that owns the run (scripts/run_sim_to_db.py,
    // a cron entry) can name it without the engine growing a flag, and the
    // derived default — the save file, or "fresh world" — still applies when
    // nothing sets it.
    if (const char* envLabel = std::getenv("ASHB2_LABEL")) {
        if (*envLabel) g_label = envLabel;
    }

    // Existing directory is fine; anything else is fatal to the export only.
    struct stat st;
    if (stat(g_dir.c_str(), &st) != 0 && ASHB_MKDIR(g_dir.c_str()) != 0) {
        std::cerr << "DBEXPORT: cannot create spool dir '" << g_dir
                  << "' — export disabled\n";
        return;
    }

    char b[32];
    std::snprintf(b, sizeof b, "%010lld_%05d",
                  static_cast<long long>(std::time(nullptr)),
                  static_cast<int>(ASHB_GETPID()));
    g_stamp   = b;
    g_counter = 0;
    g_rows    = 0;
    g_enabled = true;

    std::cout << "DBEXPORT: spooling world " << g_world << " to " << g_dir
              << " (" << CHUNK_ROWS << " rows/chunk)\n";
}

void pushWorld(int day) {
    if (!g_enabled) return;
    std::string o;
    beginRow(o, "world", day);
    kStr(o, "label", g_label);
    kStr(o, "seed",  g_seed);
    emit(o);
}

void pushEntities(const std::vector<Entity>& entities, int day) {
    if (!g_enabled) return;

    std::string o;
    o.reserve(4096);

    for (const Entity& e : entities) {
        // ── pure attributes ─────────────────────────────────────────────────
        beginRow(o, "entity", day);
        kInt (o, "sim_id",      e.entityId);
        // No `alive` field, deliberately: the tick's batch erase (main.cpp,
        // "Batch erase — shifts surviving elements") drops every entity with
        // entityHealth <= 0 before this export runs, so being here at all IS
        // being alive and the flag would be a constant true. Death reaches the
        // database as absence, which the loader turns into alive = false.

        kStr (o, "name",        e.name);
        // An unset sex is NUL on entities built before the field was assigned.
        // Escaped as a NUL code point, Postgres rejects it outright and takes
        // the whole chunk down with it — so anything unprintable becomes NULL.
        { const bool ok = (e.entitySex > 0x20);
          kStr(o, "sex", ok ? std::string(1, e.entitySex) : std::string()); }
        kInt (o, "birth_day",   e.entityBDay);
        kInt (o, "birth_year",  e.entityBirthYear);
        kIntN(o, "web_char_id", e.webCharId, -1);

        kF   (o, "age",             e.entityAge);
        kInt (o, "life_stage",      static_cast<int>(e.entityLifeStage));
        kF   (o, "health",          e.entityHealth);
        kF   (o, "hunger",          e.entityHunger);
        kF   (o, "food_store",      e.foodStore);
        kF   (o, "hygiene",         e.entityHygiene);
        kF   (o, "fatigue_level",   e.fatigueLevel);
        kF   (o, "injury_level",    e.injuryLevel);
        kF   (o, "sleep_pressure",  e.sleepPressure);
        kF   (o, "sleep_quality",   e.sleepQuality);
        kInt (o, "antibody",        e.entityAntiBody);
        kIntN(o, "disease_type",    e.entityDiseaseType, -1);
        kF   (o, "base_immunity",   e.baseImmunity);

        kF   (o, "happiness",                  e.entityHapiness);
        kF   (o, "stress",                     e.entityStress);
        kF   (o, "stress_baseline",            e.stressBaseline);
        kF   (o, "mental_health",              e.entityMentalHealth);
        kF   (o, "loneliness",                 e.entityLoneliness);
        kF   (o, "boredom",                    e.entityBoredom);
        kF   (o, "general_anger",              e.entityGeneralAnger);
        kF   (o, "esteem",                     e.Esteem);
        kF   (o, "sense_of_purpose",           e.senseOfPurpose);
        kF   (o, "grief_intensity",            e.getGriefIntensity());
        kF   (o, "social_deficit",             e.socialDeficit);
        kInt (o, "days_without_social_action", e.dayWithoutSocialAction);
        kInt (o, "meeting_count",              e.meetingCount);

        kF(o, "qi",           e.qi);
        kF(o, "qi_potential", e.qiPotential);
        kF(o, "school_years", e.schoolYears);

        kF(o, "openness",          e.personality.openness);
        kF(o, "conscientiousness", e.personality.conscientiousness);
        kF(o, "extraversion",      e.personality.extraversion);
        kF(o, "agreeableness",     e.personality.agreeableness);
        kF(o, "neuroticism",       e.personality.neuroticism);

        kF(o, "value_family",       e.ValueSystem.familyOrientation);
        kF(o, "value_achievement",  e.ValueSystem.achievementDrive);
        kF(o, "value_spiritual",    e.ValueSystem.spiritualNeed);
        kF(o, "value_hedonism",     e.ValueSystem.hedonism);
        kF(o, "value_collectivism", e.ValueSystem.collectivism);

        kF  (o, "self_perceived_extraversion",  e.SelfConcept.perceivedExtraversion);
        kF  (o, "self_perceived_agreeableness", e.SelfConcept.perceivedAgreeableness);
        kF  (o, "self_perceived_neuroticism",   e.SelfConcept.perceivedNeuroticism);
        kStr(o, "self_primary_identity",        e.SelfConcept.primaryIdentity);
        kF  (o, "self_esteem",                  e.SelfConcept.selfEsteem);
        kF  (o, "self_efficacy",                e.SelfConcept.selfEfficacy);
        kF  (o, "self_calibration",             e.SelfConcept.calibration);

        kBool(o, "had_secure_attachment",     e.dv.hadSecureAttachment);
        kF   (o, "childhood_trauma_score",    e.dv.childhoodTraumaScore);
        kF   (o, "childhood_nurturing_score", e.dv.childhoodNurturingScore);
        kInt (o, "attachment_style",          static_cast<int>(e.dv.attachmentStyle));

        kF(o, "raw_anger",        e.emotionalState.rawAnger);
        kF(o, "expressed_anger",  e.emotionalState.expressedAnger);
        kF(o, "suppression_debt", e.emotionalState.suppressionDebt);

        kStr(o, "self_story",          e.narrativeIdentity.selfStory);
        kStr(o, "dominant_value",      e.narrativeIdentity.dominantValue);
        kF  (o, "narrative_coherence", e.narrativeIdentity.coherence);

        kBool(o, "creed_held",            e.creed.held);
        kF   (o, "creed_militarism",      e.creed.militarism);
        kF   (o, "creed_tolerance",       e.creed.tolerance);
        kF   (o, "creed_asceticism",      e.creed.asceticism);
        kF   (o, "creed_authority",       e.creed.authority);
        kF   (o, "creed_afterlife_focus", e.creed.afterlifeFocus);
        kF   (o, "creed_devotion",        e.creed.devotion);

        kF(o, "nutritional_status", e.biology.nutritionalStatus);
        kF(o, "hydration_level",    e.biology.hydrationLevel);
        kF(o, "energy_level",       e.biology.energyLevel);

        kIntN(o, "parent1_id",    e.parent1Id, -1);
        kIntN(o, "parent2_id",    e.parent2Id, -1);
        kIntN(o, "family_id",     e.familyId,  -1);
        kInt (o, "lineage_depth", e.lineageDepth);

        kIntN(o, "tribe_id",         e.tribeId,        -1);
        kIntN(o, "religion_id",      e.religionId,     -1);
        kIntN(o, "origin_region_id", e.originRegionId, -1);
        kF   (o, "dominance_rank",   e.dominanceRank);
        kInt (o, "social_class",     static_cast<int>(e.socialClass));
        kF   (o, "auctoritas",       e.auctoritas);
        kF   (o, "integrity",        e.integrity);
        kStr (o, "specialization",   e.specialization);
        kBool(o, "is_specialist",    e.isSpecialist);
        kIntN(o, "role_since_day",   e.roleSinceDay, -1);
        kF   (o, "cultural_capital", e.culturalCapital);
        kBits(o, "culture_traits",   e.cultureTraits);
        kBits(o, "committed_traits", e.committedTraits);
        kF   (o, "home_attachment",  e.homeAttachment);

        kF(o, "wealth", e.salary.token);
        kF(o, "pos_x",  e.posX);
        kF(o, "pos_y",  e.posY);

        kStr(o, "current_goal",    e.m_goals.empty() ? std::string() : e.m_goals.front().type);
        kStr(o, "last_action",     e.lastActionName);
        kStr(o, "inner_monologue", e.innerMonologue);
        emit(o);

        // ── pointed associations ────────────────────────────────────────────
        for (const entityPointedDesire& d : e.list_entityPointedDesire) {
            beginRow(o, "desire", day);
            kInt(o, "from_id", e.entityId);
            kInt(o, "to_id",   targetId(d.pointedEntity, d.Id));
            kF  (o, "desire",  d.desire);
            emit(o);
        }
        for (const entityPointedAnger& a : e.list_entityPointedAnger) {
            beginRow(o, "anger", day);
            kInt(o, "from_id", e.entityId);
            kInt(o, "to_id",   targetId(a.pointedEntity, a.Id));
            kF  (o, "anger",   a.anger);
            emit(o);
        }
        for (const entityPointedSocial& s : e.list_entityPointedSocial) {
            beginRow(o, "social", day);
            kInt(o, "from_id", e.entityId);
            kInt(o, "to_id",   targetId(s.pointedEntity, s.Id));
            kF  (o, "social",  s.social);
            emit(o);
        }
        for (const entityPointedCouple& c : e.list_entityPointedCouple) {
            beginRow(o, "couple", day);
            kInt(o, "from_id",       e.entityId);
            kInt(o, "to_id",         targetId(c.pointedEntity, c.id));
            kF  (o, "commitment",    c.commitment);
            kF  (o, "satisfaction",  c.satisfaction);
            kF  (o, "trust",         c.trust);
            kF  (o, "suspicion",     c.suspicion);
            kInt(o, "days_together", c.daysTogether);
            emit(o);
        }
        for (const MentalModelOfOther* m : e.list_MentalModelOfOther) {
            if (!m || !m->entityPointed) continue;   // a model of nobody is not an edge
            beginRow(o, "mental_model", day);
            kInt (o, "from_id",                  e.entityId);
            kInt (o, "to_id",                    m->entityPointed->entityId);
            kF   (o, "perceived_extraversion",   m->perceivedExtraversion);
            kF   (o, "perceived_agreeableness",  m->perceivedAgreeableness);
            kF   (o, "perceived_neuroticism",    m->perceivedNeuroticism);
            kF   (o, "estimated_happiness",      m->estimatedHappiness);
            kF   (o, "estimated_anger",          m->estimatedAnger);
            kF   (o, "estimated_stress",         m->estimatedStress);
            kF   (o, "trust_level",              m->trustLevel);
            kF   (o, "predictability",           m->predictability);
            kF   (o, "perceived_intentionality", m->perceivedIntentionality);
            kF   (o, "confidence",               m->confidence);
            kInt (o, "last_observed_day",        m->lastObservedDay);
            kF   (o, "last_interaction_day",     m->lastInteractionDay);
            kStr (o, "last_interaction_outcome", m->lastInteractionOutcome);
            kBool(o, "faked_by_them",            m->fakedByThem);
            kF   (o, "fake_trust_gain",          m->fakeTrustGain);
            emit(o);
        }
    }
}

void flush() {
    if (!g_chunk.is_open()) return;
    g_chunk.close();                   // buffered bytes hit disk before the rename
    const std::string from = tmpPath();
    const std::string to   = finalPath();
    if (std::rename(from.c_str(), to.c_str()) != 0) {
        std::cerr << "DBEXPORT: rename " << from << " -> " << to
                  << " failed; chunk left as .tmp\n";
    }
    ++g_counter;
    g_rows = 0;
}

void shutdown() {
    if (!g_enabled) return;
    flush();                           // publish the tail, or the loader never sees it
    g_enabled = false;
}

} // namespace dbexport
