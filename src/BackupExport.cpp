#include "./header/BackupExport.h"
#include "./header/SaveLoad.h"
#include "./header/CivilizationEngine.h"
#include "./header/Entity.h"
#include "core/SimClock.h"      // g_clock — the in-world day, see below

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>       // snprintf, rename, remove
#include <ctime>
#include <sys/stat.h>
#ifdef _WIN32
  #include <windows.h>  // MoveFileExA — replace-in-place is atomic, unlike remove+rename
  #include <process.h>  // _getpid
  #include <direct.h>   // _getcwd
  #define ASHB_GETPID() _getpid()
  #define ASHB_GETCWD(b, n) _getcwd(b, n)
#else
  #include <unistd.h>   // getpid, gethostname, getcwd
  #define ASHB_GETPID() getpid()
  #define ASHB_GETCWD(b, n) getcwd(b, n)
#endif

namespace bkexport {
namespace {

bool        g_enabled  = false;
int         g_world    = 1;
long long   g_interval = 300;      // seconds between checkpoints
int         g_counter  = 0;

std::string g_run;                 // "<epoch>_<pid>", fixed for the process
std::string g_seed, g_label, g_host, g_cwd, g_argv;   // g_argv is a JSON array
std::string g_lastBackup;          // filename of the newest published save

long long   g_lastSave = 0;        // epoch of the last checkpoint
long long   g_lastBeat = 0;        // epoch of the last heartbeat write
int         g_day = 0, g_ticksDone = -1, g_target = -1;
int         g_pid = 0;

// ── paths ───────────────────────────────────────────────────────────────────
// SAVES_DIR is where saveGame puts everything and is not ours to change, so the
// backup channel lives in it rather than beside it: one directory for the
// indexer to watch, and no cross-device rename (which is not atomic) between
// where a save is written and where it is published.
std::string joinDir(const std::string& name) {
    std::string d = SAVES_DIR;
    if (!d.empty() && d.back() != '/' && d.back() != '\\') d += '/';
    return d + name;
}

// ── JSON helpers ────────────────────────────────────────────────────────────
// Same hand-rolled approach as DbExport.cpp: the engine carries no JSON
// dependency, and these objects are a dozen fields wide.

void esc(std::string& o, const std::string& s) {
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;   // Windows paths are full of these
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
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

void kStr(std::string& o, const char* k, const std::string& v) {
    o += '"'; o += k; o += "\":";
    if (v.empty()) { o += "null,"; return; }
    o += '"'; esc(o, v); o += "\",";
}

void kInt(std::string& o, const char* k, long long v) {
    char b[24]; std::snprintf(b, sizeof b, "%lld", v);
    o += '"'; o += k; o += "\":"; o += b; o += ',';
}

// Already-formed JSON (the argv array), inserted without quoting.
void kRaw(std::string& o, const char* k, const std::string& v) {
    o += '"'; o += k; o += "\":"; o += (v.empty() ? "null" : v); o += ',';
}

void close(std::string& o) {
    if (!o.empty() && o.back() == ',') o.pop_back();
    o += "}\n";
}

// ── atomic publish ──────────────────────────────────────────────────────────
// Write beside the target, then replace it in one step. The heartbeat is
// rewritten every second and read by another process at any moment, so the
// replace has to be atomic: plain std::rename fails on Windows when the
// destination exists, and remove-then-rename leaves a window where the file
// does not exist at all.
bool publish(const std::string& from, const std::string& to) {
#ifdef _WIN32
    if (MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING)) return true;
#else
    if (std::rename(from.c_str(), to.c_str()) == 0) return true;
#endif
    std::cerr << "BACKUP: publish " << from << " -> " << to << " failed\n";
    return false;
}

bool writeAtomic(const std::string& path, const std::string& text) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) {
            std::cerr << "BACKUP: cannot write " << tmp << "\n";
            return false;
        }
        f << text;
    }                                   // closed — bytes are on their way to disk
    return publish(tmp, path);
}

std::string hostName() {
#ifdef _WIN32
    if (const char* h = std::getenv("COMPUTERNAME")) return h;
    return "windows";
#else
    char b[256] = {0};
    if (gethostname(b, sizeof b - 1) == 0) return b;
    return "unknown";
#endif
}

std::string workingDir() {
    char b[4096] = {0};
    if (ASHB_GETCWD(b, sizeof b - 1)) return b;
    return "";
}

// Everything a supervisor needs to know about a live run, including how to
// start it again. Rewritten in place ~once a second.
void writeHeartbeat(const char* status) {
    std::string o = "{";
    kStr(o, "run_id",       g_run);
    kStr(o, "status",       status);
    kInt(o, "pid",          g_pid);
    kStr(o, "host",         g_host);
    kInt(o, "world_id",     g_world);
    kStr(o, "seed",         g_seed);
    kStr(o, "label",        g_label);
    kInt(o, "day",          g_day);
    kInt(o, "civ_day",      static_cast<long long>(g_clock.day()));
    kInt(o, "ticks_done",   g_ticksDone);
    kInt(o, "target_ticks", g_target);
    kInt(o, "interval_sec", g_interval);
    kInt(o, "updated_at",   static_cast<long long>(std::time(nullptr)));
    kStr(o, "last_backup",  g_lastBackup);
    kStr(o, "cwd",          g_cwd);
    kRaw(o, "argv",         g_argv);
    close(o);
    writeAtomic(joinDir("run_" + g_run + ".alive"), o);
}

} // namespace

// ─── public API ─────────────────────────────────────────────────────────────

bool enabled() { return g_enabled; }

void configure(int worldId, const std::string& seed, const std::string& label,
               int intervalSeconds, int argc, char** argv) {
    if (intervalSeconds <= 0) return;
    if (!ensureSavesDir()) {
        std::cerr << "BACKUP: no saves directory — backups disabled\n";
        return;
    }

    g_world    = worldId;
    g_seed     = seed;
    g_label    = label;
    g_interval = intervalSeconds;
    g_pid      = static_cast<int>(ASHB_GETPID());
    g_host     = hostName();
    g_cwd      = workingDir();
    g_counter  = 0;

    char b[32];
    std::snprintf(b, sizeof b, "%010lld_%05d",
                  static_cast<long long>(std::time(nullptr)), g_pid);
    g_run = b;

    g_argv = "[";
    for (int i = 0; i < argc; ++i) {
        if (i) g_argv += ',';
        g_argv += '"';
        esc(g_argv, argv[i] ? argv[i] : "");
        g_argv += '"';
    }
    g_argv += ']';

    // The first checkpoint lands one interval in, not now: at tick zero there
    // is nothing to lose, and a fresh start is what the supervisor would fall
    // back to anyway.
    g_lastSave = static_cast<long long>(std::time(nullptr));
    g_enabled  = true;

    writeHeartbeat("running");
    std::cout << "BACKUP: run " << g_run << " checkpointing every "
              << g_interval << "s into " << SAVES_DIR << "\n";
}

void checkpoint(const std::vector<Entity>& entities, int day, int frameCounter,
                const CivilizationEngine* civ, int ticksDone, int targetTicks) {
    if (!g_enabled) return;

    char stem[128];
    std::snprintf(stem, sizeof stem, "bk_%s_%06d_d%d", g_run.c_str(), g_counter, day);

    // saveGame owns the naming — it prepends the civilisation's history
    // signature and drops the file in SAVES_DIR — so the only way to control
    // the published name is to hand it a name we can predict and then rename.
    // The signature is recomputed here from the same engine state saveGame is
    // about to read, with no simulation step in between, so the two agree.
    const std::string sig      = civ ? std::to_string(civ->historySignature()) + "_" : "";
    const std::string partName = std::string(stem) + ".part";

    saveGame(partName, entities, day, frameCounter, civ);

    const std::string part  = joinDir(sig + partName);
    const std::string save  = joinDir(sig + stem + ".txt");
    struct stat st;
    if (stat(part.c_str(), &st) != 0) {
        std::cerr << "BACKUP: saveGame wrote nothing at " << part << "\n";
        return;
    }
    if (!publish(part, save)) return;

    // The sidecar is published second and on purpose: the indexer keys off the
    // .txt, and a save with no sidecar is still restorable (the filename and
    // the save's own DAY: line carry enough). A sidecar with no save would not
    // be.
    std::string o = "{";
    kStr(o, "run_id",       g_run);
    kStr(o, "save",         sig + stem + ".txt");
    kInt(o, "world_id",     g_world);
    kStr(o, "seed",         g_seed);
    kStr(o, "label",        g_label);
    kStr(o, "signature",    civ ? std::to_string(civ->historySignature()) : std::string());
    // Two different days, both wanted. `day` is what saveGame wrote into the
    // file and what a resumed run continues from, so it is the one to order
    // checkpoints by. It counts render frames (60 per tick), which makes it
    // meaningless to a person reading the index — so the in-world day the civ
    // log and the database speak in goes alongside it.
    kInt(o, "day",          day);
    kInt(o, "civ_day",      static_cast<long long>(g_clock.day()));
    kInt(o, "frame",        frameCounter);
    kInt(o, "ticks_done",   ticksDone);
    kInt(o, "target_ticks", targetTicks);
    kInt(o, "entities",     static_cast<long long>(entities.size()));
    kInt(o, "bytes",        static_cast<long long>(st.st_size));
    kInt(o, "created_at",   static_cast<long long>(std::time(nullptr)));
    kInt(o, "pid",          g_pid);
    kStr(o, "host",         g_host);
    kStr(o, "cwd",          g_cwd);
    kRaw(o, "argv",         g_argv);
    close(o);
    writeAtomic(joinDir(sig + stem + ".meta.json"), o);

    g_lastBackup = sig + stem + ".txt";
    g_lastSave   = static_cast<long long>(std::time(nullptr));
    ++g_counter;

    std::cout << "BACKUP: day " << day << " -> " << g_lastBackup
              << " (" << entities.size() << " entities)\n";
    writeHeartbeat("running");
}

void tick(const std::vector<Entity>& entities, int day, int frameCounter,
          const CivilizationEngine* civ, int ticksDone, int targetTicks) {
    if (!g_enabled) return;

    g_day       = day;
    g_ticksDone = ticksDone;
    g_target    = targetTicks;

    const long long now = static_cast<long long>(std::time(nullptr));
    if (now - g_lastSave >= g_interval)
        checkpoint(entities, day, frameCounter, civ, ticksDone, targetTicks);

    // A heartbeat a second is enough to spot a dead run in seconds and cheap
    // enough to ignore; without the throttle a fast headless run would rewrite
    // this file thousands of times a second.
    if (now != g_lastBeat) {
        g_lastBeat = now;
        writeHeartbeat("running");
    }
}

void shutdown(const char* status) {
    if (!g_enabled) return;
    // The last write wins: this is the difference between "the run ended" and
    // "the run stopped answering", which is the whole crash signal.
    writeHeartbeat(status && *status ? status : "finished");
    g_enabled = false;               // the atexit hook after an explicit call is a no-op
}

void atExit() { shutdown("finished"); }

} // namespace bkexport
