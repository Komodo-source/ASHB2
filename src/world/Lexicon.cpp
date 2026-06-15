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
