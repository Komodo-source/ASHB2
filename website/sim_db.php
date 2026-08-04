<?php
/**
 * ASHB2: Simulation Database Connection (PostgreSQL)
 *
 * The read side of the engine → database → web pipeline. The C++ engine spools
 * NDJSON chunks (src/header/DbExport.h), scripts/db_spool_loader.py COPYs them
 * into `sim.*` (sql/schema_pg.sql), and this class is how PHP reads them.
 *
 * Separate from Database (db.php) on purpose: that one is MySQL and owns the
 * accounts — users, characters, password resets. This one is PostgreSQL and owns
 * the world. Different engines, different credentials, different lifetimes; one
 * class each keeps a query from silently going to the wrong store.
 *
 * NOTHING HERE WRITES. The loader is the only writer to `sim.*`, and it upserts
 * whole chunks in one transaction against a high-water mark. A write from the
 * web app would be reverted by the next push at best and would corrupt the
 * reaping logic at worst — so the API below is read-only by construction.
 *
 * Requires the pdo_pgsql extension:  extension=pdo_pgsql in php.ini.
 */

require_once __DIR__ . '/config.php';

class SimDb
{
    private static ?PDO $instance = null;

    /**
     * Get the PDO connection to the simulation database.
     * Created on first call, reused thereafter.
     */
    public static function connect(): PDO
    {
        if (self::$instance !== null) {
            return self::$instance;
        }
        try {
            self::$instance = self::open();
        } catch (Throwable $e) {
            // CLI writes error_log to stderr, where fail() is about to write
            // the same text — log only for the web, where it goes to the
            // server log and the visitor sees nothing.
            if (php_sapi_name() !== 'cli') {
                error_log('Simulation database connection failed: ' . $e->getMessage());
            }
            self::fail('Simulation database connection failed: ' . $e->getMessage());
        }
        return self::$instance;
    }

    /**
     * Open a connection, or throw. Kept separate from connect() so that
     * isAvailable() has a path that reports failure instead of ending the
     * request: fail() calls die() in a web context, which no try/catch can
     * intercept, and a page that wants to degrade gracefully needs an answer
     * rather than an exit.
     */
    private static function open(): PDO
    {
        // Say this plainly rather than letting PDO throw "could not find
        // driver", which sends people looking for a bad DSN. The DLL usually
        // ships with PHP and is simply not enabled.
        if (!in_array('pgsql', PDO::getAvailableDrivers(), true)) {
            throw new RuntimeException(
                'The pdo_pgsql extension is not loaded. Enable it in php.ini '
                . '(extension=pdo_pgsql) and restart PHP. Loaded drivers: '
                . implode(', ', PDO::getAvailableDrivers())
            );
        }

        [$dsn, $user, $pass] = self::dsn();

        $pdo = new PDO($dsn, $user, $pass, [
            PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
            PDO::ATTR_EMULATE_PREPARES   => false,
            // A hosted database is a network hop away and the dashboard is not
            // worth hanging a page load on. Without this a dead host blocks
            // until the OS gives up, which on Windows is over a minute.
            PDO::ATTR_TIMEOUT            => PG_TIMEOUT,
        ]);
        // Every query in the app names its schema (sim.entity, sim.world), so
        // this only decides where an UNqualified name would resolve. Pinned
        // anyway: a search_path inherited from the role is one more thing that
        // can differ between your machine and the server.
        $pdo->exec("SET search_path TO sim, public");
        return $pdo;
    }

    /**
     * Strip the brackets from an IPv6 literal.
     *
     * A URL has to bracket an IPv6 address — `postgresql://u:p@[2001:db8::1]/db`
     * — otherwise its colons are indistinguishable from the port separator, and
     * parse_url hands the host back with those brackets still attached.
     * libpq's `host=` keyword wants the bare address: `host=[2001:db8::1]` is
     * read as a hostname containing bracket characters, which resolves to
     * nothing. Hostnames and IPv4 literals pass through untouched.
     *
     * This matters for Supabase in particular, whose direct connections
     * (db.<ref>.supabase.co) are IPv6-only.
     */
    private static function unbracket(string $host): string
    {
        if (strlen($host) > 1 && $host[0] === '[' && substr($host, -1) === ']') {
            return substr($host, 1, -1);
        }
        return $host;
    }

    /**
     * Build (dsn, user, password) from configuration.
     *
     * PG_DSN wins when set, because that is the form hosted providers hand you
     * (`postgresql://user:pass@host/db?sslmode=require`). It is translated into
     * PDO's own DSN syntax, which does not accept a URL and — importantly —
     * does not accept credentials inside the DSN string.
     */
    private static function dsn(): array
    {
        $url = PG_DSN;
        if ($url !== '') {
            $p = parse_url($url);
            if ($p === false || !isset($p['host'])) {
                throw new RuntimeException('PG_DSN is not a valid PostgreSQL URL: ' . $url);
            }

            $parts = ['host=' . self::unbracket($p['host'])];
            $parts[] = 'port=' . ($p['port'] ?? 5432);
            $parts[] = 'dbname=' . ltrim($p['path'] ?? '', '/');

            // Query parameters (sslmode, options, ...) pass straight through:
            // a hosted database usually REQUIRES sslmode=require, and dropping
            // it turns a working URL into a refused connection.
            $q = [];
            if (isset($p['query'])) {
                parse_str($p['query'], $q);
                foreach ($q as $k => $v) {
                    $parts[] = $k . '=' . $v;
                }
            }

            // A URL copied from a provider's dashboard often carries no query
            // string at all — Supabase's pooler URL does not. libpq would then
            // fall back to sslmode=prefer, which silently accepts an
            // unencrypted connection if the server ever offers one. PG_SSLMODE
            // is the deliberate setting, so honour it when the URL is silent;
            // an sslmode already in the URL still wins.
            if (!isset($q['sslmode']) && PG_SSLMODE !== '') {
                $parts[] = 'sslmode=' . PG_SSLMODE;
            }

            return [
                'pgsql:' . implode(';', $parts),
                isset($p['user']) ? urldecode($p['user']) : PG_USER,
                isset($p['pass']) ? urldecode($p['pass']) : PG_PASS,
            ];
        }

        $parts = [
            'host='   . self::unbracket(PG_HOST),   // PG_HOST may be an IPv6 literal
            'port='   . PG_PORT,
            'dbname=' . PG_NAME,
        ];
        if (PG_SSLMODE !== '') {
            $parts[] = 'sslmode=' . PG_SSLMODE;
        }

        return ['pgsql:' . implode(';', $parts), PG_USER, PG_PASS];
    }

    /**
     * Prepare + execute + return the statement.
     */
    public static function query(string $sql, array $params = []): PDOStatement
    {
        $stmt = self::connect()->prepare($sql);
        $stmt->execute($params);
        return $stmt;
    }

    /**
     * Fetch a single row, or null when there is none.
     */
    public static function fetchOne(string $sql, array $params = []): ?array
    {
        $result = self::query($sql, $params)->fetch();
        return $result ?: null;
    }

    /**
     * Fetch all rows.
     */
    public static function fetchAll(string $sql, array $params = []): array
    {
        return self::query($sql, $params)->fetchAll();
    }

    /**
     * Fetch a single column of the first row (counts, maxima, one field).
     */
    public static function fetchValue(string $sql, array $params = [])
    {
        $value = self::query($sql, $params)->fetchColumn();
        return $value === false ? null : $value;
    }

    /**
     * Run a statement that writes.
     *
     * The read-only rule at the top of this file is about the tables the loader
     * owns — sim.world, sim.entity and the entity_* associations. Those are
     * replaced wholesale on every push against a high-water mark, so a write
     * from here is reverted at best and desynchronises the reaping logic at
     * worst.
     *
     * sim.users is the exception, and the only one: the loader never touches it
     * (grep it — the engine exports no accounts) and the web app is its sole
     * writer. Registration and sign-in have to persist somewhere, and this is
     * where the provided schema puts them.
     *
     * Deliberately not a general write API. Anything that is not sim.users is
     * refused here rather than in review, because the failure it prevents is
     * silent: a stray UPDATE on sim.entity looks like it worked right up until
     * the next push erases it.
     */
    public static function execute(string $sql, array $params = []): int
    {
        if (!preg_match('/^\s*(INSERT\s+INTO|UPDATE|DELETE\s+FROM)\s+(?:sim\.)?users\b/i', $sql)) {
            throw new RuntimeException(
                'SimDb::execute is limited to sim.users; the loader owns every '
                . 'other table in sim.* and overwrites it on the next push. '
                . 'Refused: ' . $sql
            );
        }
        $stmt = self::connect()->prepare($sql);
        $stmt->execute($params);
        return $stmt->rowCount();
    }

    /**
     * Is the simulation database reachable?
     *
     * For pages that should degrade to "the world is offline" rather than die:
     * the loader and the engine are separate processes that can be down while
     * the site is perfectly able to serve accounts.
     */
    public static function isAvailable(): bool
    {
        try {
            if (self::$instance === null) {
                self::$instance = self::open();
                return true;
            }
            self::$instance->query('SELECT 1');
            return true;
        } catch (Throwable $e) {
            return false;
        }
    }

    /**
     * The underlying PDO instance.
     */
    public static function raw(): PDO
    {
        return self::connect();
    }

    /**
     * One place that decides how a connection failure surfaces: the detail in
     * development, nothing in production. CLI tools get the message either way
     * or they are impossible to debug.
     */
    private static function fail(string $message): void
    {
        if (php_sapi_name() === 'cli') {
            // Thrown, not exited: a tool can catch this and say something more
            // useful than a raw driver message, and isAvailable() below depends
            // on the failure being catchable at all — which exit() is not.
            throw new RuntimeException($message);
        }
        if (defined('APP_ENV') && APP_ENV === 'development') {
            die($message);
        }
        http_response_code(500);
        die('The simulation database is unavailable. Please try again later.');
    }

    private function __construct() {}
    private function __clone() {}
}
