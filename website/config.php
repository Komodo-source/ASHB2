<?php
/**
 * ASHB2: Human-in-the-Loop Simulation — Configuration
 * 
 * Loads settings from environment variables with .env file fallback.
 * Sensible defaults for local development.
 */

// ── .env loader (KEY=value lines; real env vars always win) ────
$__envFile = __DIR__ . '/.env';
if (is_file($__envFile)) {
    foreach (file($__envFile, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) as $__line) {
        $__line = trim($__line);
        if ($__line === '' || $__line[0] === '#' || strpos($__line, '=') === false) {
            continue;
        }
        [$__key, $__value] = explode('=', $__line, 2);
        $__key = trim($__key);
        $__value = trim(trim($__value), "\"'");
        if ($__key !== '' && getenv($__key) === false) {
            putenv($__key . '=' . $__value);
        }
    }
}
unset($__envFile, $__line, $__key, $__value);

// ── Error reporting (disable in production) ─────────────────────
error_reporting(E_ALL);
ini_set('display_errors', '1');

// ── Session configuration (must be set before session_start) ──
ini_set('session.use_strict_mode', '1');
ini_set('session.use_only_cookies', '1');
ini_set('session.cookie_httponly', '1');
ini_set('session.cookie_samesite', 'Lax');
ini_set('session.cookie_secure', '0'); // Set to '1' if using HTTPS
ini_set('session.gc_maxlifetime', '86400'); // 24 hours

// ── Application database: MySQL ─────────────────────────────────
// Accounts, characters, password resets, feedback — sql/schema.sql.
// Read through Database:: (db.php).
define('DB_HOST', getenv('DB_HOST') ?: '127.0.0.1');
define('DB_PORT', getenv('DB_PORT') ?: '3306');
define('DB_NAME', getenv('DB_NAME') ?: 'ashb2');
define('DB_USER', getenv('DB_USER') ?: 'root');
define('DB_PASS', getenv('DB_PASS') ?: '');
define('DB_CHARSET', 'utf8mb4');

// ── Simulation database: PostgreSQL ─────────────────────────────
// The world itself — sim.* in sql/schema_pg.sql, written by
// scripts/db_spool_loader.py from what the C++ engine spools.
// Read through SimDb:: (sim_db.php). A separate server, so separate
// settings: sharing DB_* would point one of the two at the wrong host.
//
// PG_DSN is the whole connection as a URL and wins when set — it is what
// hosted providers give you, sslmode and all:
//   PG_DSN=postgresql://user:password@host.neon.tech/ashb2?sslmode=require
// Otherwise the discrete settings below are used.
//
// PG_POOLER_URL is the fallback, and on Supabase it is the one that works from
// here. A direct connection (db.<ref>.supabase.co) publishes an AAAA record and
// no A record, so a machine without IPv6 cannot even resolve the name — the
// failure is "could not translate host name", not a refused connection, which
// sends people looking for a firewall that isn't the problem. The session
// pooler (aws-0-<region>.pooler.supabase.com) answers over IPv4. Its username
// carries the project ref: postgres.<ref>, not postgres.
define('PG_DSN',     getenv('PG_DSN') ?: (getenv('PG_POOLER_URL') ?: ''));
define('PG_HOST',    getenv('PG_HOST') ?: '127.0.0.1');
define('PG_PORT',    getenv('PG_PORT') ?: '5432');
define('PG_NAME',    getenv('PG_NAME') ?: 'ashb2');
define('PG_USER',    getenv('PG_USER') ?: 'postgres');
define('PG_PASS',    getenv('PG_PASS') ?: '');
// Hosted PostgreSQL almost always requires TLS; a local one usually has none.
// Empty leaves the choice to libpq's own default ('prefer').
define('PG_SSLMODE', getenv('PG_SSLMODE') ?: '');
// Seconds to wait for the simulation database before giving up. Short on
// purpose: a page must not hang because the world's server is down.
define('PG_TIMEOUT', (int)(getenv('PG_TIMEOUT') ?: 5));
// Which world the site displays. The engine writes one row per --world-id.
define('SIM_WORLD_ID', (int)(getenv('SIM_WORLD_ID') ?: 1));

// ── Application ─────────────────────────────────────────────────
define('APP_NAME', 'ASHB2');
define('APP_URL', getenv('APP_URL') ?: 'http://localhost:8080');
define('APP_ENV', getenv('APP_ENV') ?: 'development');

// ── Security ────────────────────────────────────────────────────
define('PASSWORD_ALGO', PASSWORD_BCRYPT);
define('PASSWORD_COST', 12);
define('SESSION_LIFETIME', 86400); // 24 hours

// ── Simulation ──────────────────────────────────────────────────
define('SIM_DAYS_PER_HUMAN_DAY', 4);
define('SIM_TICK_INTERVAL_SECONDS', 21600); // 6 hours
