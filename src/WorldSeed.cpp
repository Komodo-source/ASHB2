#include "WorldSeed.h"

// The one global seed for the whole simulation. Default is arbitrary but fixed
// so an un-initialised run is still deterministic; main() overwrites it.
WorldSeed g_worldSeed{ 0xA5A5A5A5A5A5A5A5ull, {} };

uint64_t WorldSeed::hashString(const std::string& s) {
    // FNV-1a 64-bit
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h ? h : 0x1ull;
}

WorldSeed WorldSeed::fromString(const std::string& s) {
    WorldSeed ws;
    // Pure-digit strings are used as the literal numeric seed; otherwise hash.
    bool allDigits = !s.empty();
    for (char c : s) if (c < '0' || c > '9') { allDigits = false; break; }
    if (allDigits) {
        ws.master = std::strtoull(s.c_str(), nullptr, 10);
        if (ws.master == 0) ws.master = 0x1ull;
    } else {
        ws.master = hashString(s);
    }
    return ws;
}
