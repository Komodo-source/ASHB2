#include "Lexicon.h"
#include "../header/WorldSeed.h"
#include <algorithm>
#include <cctype>

Lexicon* g_lexicon = nullptr;

// Master phoneme pools drawn from when minting a region's language.
static const std::vector<std::string> ALL_ONSETS = {
    "b","d","t","k","g","p","m","n","s","sh","th","v","z","r","l","h","w","y",
    "br","tr","kr","gr","dr","st","sk","sp","kl","gl","fl","sn","vr","thr","chr","ng","mb"
};
static const std::vector<std::string> ALL_NUCLEI = {
    "a","e","i","o","u","ae","ei","ou","ia","au","eo","y","aa","ee","oo"
};
static const std::vector<std::string> ALL_CODAS = {
    "","","","n","r","s","l","m","k","th","sh","nd","rk","st","ng","x","z"
};
static const std::vector<std::string> ALL_MALE_SUF = {
    "","an","or","us","ek","ar","im","oth","ar","en","ud","ax","on"
};
static const std::vector<std::string> ALL_FEM_SUF = {
    "a","ia","el","yn","ra","is","ae","una","eth","ila","sa","ane"
};

// Pull `count` distinct items from a pool deterministically.
static std::vector<std::string> sample(const std::vector<std::string>& pool,
                                       int count, uint64_t& s) {
    std::vector<int> idx(pool.size());
    for (size_t i = 0; i < pool.size(); ++i) idx[i] = (int)i;
    for (int i = (int)idx.size() - 1; i > 0; --i) {
        s = splitmix64(s);
        int j = (int)(s % (uint64_t)(i + 1));
        std::swap(idx[i], idx[j]);
    }
    std::vector<std::string> out;
    count = std::min(count, (int)pool.size());
    for (int i = 0; i < count; ++i) out.push_back(pool[idx[i]]);
    return out;
}

Lexicon::Language Lexicon::makeLanguage(uint64_t seed) {
    Language L;
    uint64_t s = splitmix64(seed ? seed : 1);
    L.onsets = sample(ALL_ONSETS, 7 + (int)(splitmix64(s++) % 5), s);
    L.nuclei = sample(ALL_NUCLEI, 4 + (int)(splitmix64(s++) % 4), s);
    L.codas  = sample(ALL_CODAS,  5 + (int)(splitmix64(s++) % 5), s);
    L.maleSuf = sample(ALL_MALE_SUF, 4 + (int)(splitmix64(s++) % 4), s);
    L.femSuf  = sample(ALL_FEM_SUF,  4 + (int)(splitmix64(s++) % 4), s);
    L.rngState = s;
    return L;
}

void Lexicon::initRegions(int regionCount, uint64_t masterSeed) {
    langs.clear();
    langs.reserve(std::max(1, regionCount));
    for (int i = 0; i < std::max(1, regionCount); ++i) {
        uint64_t seed = splitmix64(masterSeed ^ (STREAM_NAMES + 0x1000ull * (i + 1)));
        langs.push_back(makeLanguage(seed));
    }
}

Lexicon::Language& Lexicon::langFor(int regionId) {
    if (langs.empty()) langs.push_back(makeLanguage(0xBEEF));
    if (regionId < 0 || regionId >= (int)langs.size()) return langs[0];
    return langs[regionId];
}

static std::string capitalize(std::string w) {
    if (!w.empty()) w[0] = (char)std::toupper((unsigned char)w[0]);
    return w;
}

static const std::string& pickFrom(const std::vector<std::string>& v, uint64_t& s) {
    static const std::string empty = "";
    if (v.empty()) return empty;
    s = splitmix64(s);
    return v[s % v.size()];
}

std::string Lexicon::syllable(Language& L, uint64_t& s) {
    return pickFrom(L.onsets, s) + pickFrom(L.nuclei, s) + pickFrom(L.codas, s);
}

std::string Lexicon::word(Language& L, uint64_t& s, int minSyl, int maxSyl) {
    s = splitmix64(s);
    int n = minSyl + (int)(s % (uint64_t)std::max(1, maxSyl - minSyl + 1));
    std::string w;
    for (int i = 0; i < n; ++i) w += syllable(L, s);
    return w;
}

std::string Lexicon::genName(int regionId, char sex) {
    Language& L = langFor(regionId);
    uint64_t s = L.rngState = splitmix64(L.rngState);
    std::string w = word(L, s, 1, 2);
    const std::string& suf = (sex == 'F' || sex == 'f')
        ? pickFrom(L.femSuf, s) : pickFrom(L.maleSuf, s);
    return capitalize(w + suf);
}

std::string Lexicon::genTribeName(int regionId) {
    Language& L = langFor(regionId);
    uint64_t s = L.rngState = splitmix64(L.rngState);
    return capitalize(word(L, s, 2, 3));
}

std::string Lexicon::genReligionName(int regionId) {
    Language& L = langFor(regionId);
    uint64_t s = L.rngState = splitmix64(L.rngState);
    return capitalize(word(L, s, 2, 3) + "ism");
}

std::string Lexicon::genPlaceName(int regionId) {
    Language& L = langFor(regionId);
    uint64_t s = L.rngState = splitmix64(L.rngState);
    return capitalize(word(L, s, 2, 3));
}

void Lexicon::drift(int regionId, uint64_t tick) {
    Language& L = langFor(regionId);
    uint64_t s = splitmix64(L.rngState ^ tick);
    // occasionally add a fresh phoneme or drop one -> slow sound change
    if (!L.onsets.empty() && (s & 1)) {
        const std::string& add = pickFrom(ALL_ONSETS, s);
        if (std::find(L.onsets.begin(), L.onsets.end(), add) == L.onsets.end())
            L.onsets.push_back(add);
    }
    s = splitmix64(s);
    if (L.nuclei.size() > 3 && (s & 1)) {
        L.nuclei.erase(L.nuclei.begin() + (s % L.nuclei.size()));
    }
    L.rngState = s;
}

// ── IV-P3: mutual intelligibility ────────────────────────────────────────────
// Two tongues understand each other in proportion to the sound-stuff they
// share. Jaccard over the three inventories: |A ∩ B| / |A ∪ B|, averaged across
// onsets, nuclei and codas. It is deliberately a *lexical* measure — this world
// models languages as syllable inventories, so overlap of those inventories is
// the honest thing to compute, and it moves exactly as speakers' contact moves
// it (blend raises it, drift lowers it).
float Lexicon::intelligibility(int regionA, int regionB) const {
    if (regionA == regionB) return 1.0f;
    if (regionA < 0 || regionB < 0) return 0.5f;          // unknown tongue: assume a lingua franca
    if (regionA >= (int)langs.size() || regionB >= (int)langs.size()) return 0.5f;
    const Language& A = langs[regionA];
    const Language& B = langs[regionB];

    auto jaccard = [](const std::vector<std::string>& x,
                      const std::vector<std::string>& y) -> float {
        if (x.empty() && y.empty()) return 1.0f;
        int shared = 0;
        for (const auto& a : x)
            if (std::find(y.begin(), y.end(), a) != y.end()) ++shared;
        int uni = (int)x.size() + (int)y.size() - shared;
        return uni > 0 ? (float)shared / (float)uni : 0.0f;
    };
    // Vowels carry less distinguishing weight than consonant clusters: two
    // languages sharing only their vowels are still mutually opaque.
    return 0.40f * jaccard(A.onsets, B.onsets)
         + 0.20f * jaccard(A.nuclei, B.nuclei)
         + 0.40f * jaccard(A.codas,  B.codas);
}

int Lexicon::cloneLanguage(int srcLangId, uint64_t salt) {
    Language child = langFor(srcLangId);          // copy the parents' speech
    uint64_t s = splitmix64(child.rngState ^ salt ^ 0x9E3779B97F4A7C15ull);

    // Sound change after a split is both loss and innovation: some of the
    // parents' distinctions are levelled away and new ones appear. `drift()`
    // alone only ever ADDS onsets, so a fork built from it would stay almost
    // perfectly intelligible with its parent for ever — a language family that
    // never actually branches. Dropping roughly a third of the inherited
    // inventory and minting replacements is what makes the daughter tongue
    // audibly its own within a generation, while still recognisably related.
    auto diverge = [&](std::vector<std::string>& inv,
                       const std::vector<std::string>& pool, size_t keepMin) {
        for (size_t i = inv.size(); i-- > keepMin; ) {
            s = splitmix64(s);
            if (s % 100 < 35) inv.erase(inv.begin() + i);
        }
        int add = 2 + (int)(splitmix64(s++) % 3);
        for (int k = 0; k < add; ++k) {
            const std::string& item = pickFrom(pool, s);
            if (std::find(inv.begin(), inv.end(), item) == inv.end()) inv.push_back(item);
        }
    };
    diverge(child.onsets, ALL_ONSETS, 4);
    diverge(child.codas,  ALL_CODAS,  3);
    diverge(child.nuclei, ALL_NUCLEI, 3);

    child.rngState = s;
    langs.push_back(child);
    return (int)langs.size() - 1;
}

void Lexicon::blend(int dstRegion, int srcRegion, float strength) {
    if (dstRegion == srcRegion) return;
    Language& D = langFor(dstRegion);
    Language& S = langFor(srcRegion);
    uint64_t s = splitmix64(D.rngState ^ S.rngState);
    auto mix = [&](std::vector<std::string>& dst, const std::vector<std::string>& src) {
        for (const auto& item : src) {
            s = splitmix64(s);
            if ((s % 1000) / 1000.0f < strength &&
                std::find(dst.begin(), dst.end(), item) == dst.end())
                dst.push_back(item);
        }
    };
    mix(D.onsets, S.onsets);
    mix(D.nuclei, S.nuclei);
    mix(D.codas,  S.codas);
    D.rngState = s;
}
