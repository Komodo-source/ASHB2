<?php
/**
 * ASHB2: Human-in-the-Loop Simulation — Configuration
 * 
 * Loads settings from environment variables with .env file fallback.
 * Sensible defaults for local development.
 */

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

// ── Database ────────────────────────────────────────────────────
define('DB_HOST', getenv('DB_HOST') ?: '127.0.0.1');
define('DB_PORT', getenv('DB_PORT') ?: '3306');
define('DB_NAME', getenv('DB_NAME') ?: 'ashb2');
define('DB_USER', getenv('DB_USER') ?: 'root');
define('DB_PASS', getenv('DB_PASS') ?: '');
define('DB_CHARSET', 'utf8mb4');

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
