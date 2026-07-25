#ifndef WORLD_LEXICON_H
#define WORLD_LEXICON_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

// Per-region procedural language. Each region (cradle) gets its own syllable
// inventory at creation, so names from different homelands sound distinct —
// the "alien but familiar" feel. Inventories drift over time and blend when
// cultures merge (conquest/schism), like real language families.
class Lexicon {
public:
    // Build a language for every region id [0, regionCount).
    void initRegions(int regionCount, uint64_t masterSeed);

    // Generate names. regionId < 0 falls back to a neutral language.
    std::string genName(int regionId, char sex);
    std::string genTribeName(int regionId);
    std::string genReligionName(int regionId);
    std::string genPlaceName(int regionId);

    // Mutate a region's inventory slightly (call periodically -> drift).
    void drift(int regionId, uint64_t tick);

    // Blend src's language into dst (conquest/merger -> creolisation).
    void blend(int dstRegion, int srcRegion, float strength);

    // ── IV-P3 (Track IV): can these two peoples understand each other? ───────
    // Mutual intelligibility as the share of sound-stuff two languages hold in
    // common (Jaccard over onsets, nuclei and codas). 1 = the same tongue,
    // 0 = nothing shared. Languages that started as one and have only drifted
    // stay high; ones seeded separately start near zero and only converge if
    // their speakers actually meet. This is the number the rest of the
    // simulation asks before letting anything cross a language boundary:
    // words, techniques, treaties, trust. See plans/parallel-earth-upgrade.md
    // §7 IV-P3.
    float intelligibility(int regionA, int regionB) const;

    // ── IV-P3: ethnogenesis ──────────────────────────────────────────────────
    // Fork a new tongue from an existing one and immediately drift it. This is
    // what a language SPLIT is: a people that breaks away takes its parents'
    // speech with it and then stops sharing the changes. Languages were keyed
    // to regions, and in a single-cradle world that meant every people spoke
    // the same tongue and no barrier could ever exist; forking per breakaway
    // people is what lets a language family actually branch.
    // Returns the new language id (an index into the same table).
    int cloneLanguage(int srcLangId, uint64_t salt);

    int regionCount() const { return (int)langs.size(); }

private:
    struct Language {
        std::vector<std::string> onsets;   // leading consonant clusters
        std::vector<std::string> nuclei;   // vowels
        std::vector<std::string> codas;    // trailing consonants ("" allowed)
        std::vector<std::string> maleSuf;
        std::vector<std::string> femSuf;
        uint64_t rngState = 0;
    };
    std::vector<Language> langs;

    static Language makeLanguage(uint64_t seed);
    std::string syllable(Language& L, uint64_t& s);
    std::string word(Language& L, uint64_t& s, int minSyl, int maxSyl);
    Language& langFor(int regionId);
};

extern Lexicon* g_lexicon;

#endif // WORLD_LEXICON_H
