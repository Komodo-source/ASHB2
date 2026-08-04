<?php
/**
 * ASHB2: Simulation database check / import (CLI only)
 *
 * The one command to run after putting PG_DSN in .env. It answers, in order,
 * every question a failing dashboard raises:
 *
 *   php website/bin/pg_check.php            is the driver there, does it
 *                                           connect, is sim.* present, is
 *                                           anything actually in it?
 *   php website/bin/pg_check.php --import   apply sql/schema_pg.sql (safe to
 *                                           re-run: every statement is
 *                                           IF NOT EXISTS)
 *
 * Checked in this order on purpose — a missing extension looks exactly like a
 * bad password from the browser, and an empty database looks exactly like a
 * broken page.
 */

if (php_sapi_name() !== 'cli') exit(1);

require_once __DIR__ . '/../sim_db.php';

$import = in_array('--import', $argv, true);

function line(string $status, string $text): void
{
    echo str_pad($status, 6) . $text . "\n";
}

// ── 1. the driver ───────────────────────────────────────────────────────────
$drivers = PDO::getAvailableDrivers();
if (!in_array('pgsql', $drivers, true)) {
    line('FAIL', 'pdo_pgsql is not loaded (have: ' . implode(', ', $drivers) . ')');
    echo "\n  Add to your php.ini and restart PHP:\n"
       . "      extension=pdo_pgsql\n"
       . "      extension=pgsql\n"
       . "  php --ini  shows which php.ini is in use.\n";
    exit(1);
}
line('OK', 'pdo_pgsql loaded');

// ── 2. configuration ────────────────────────────────────────────────────────
// Never print the password: this output is the thing people paste into chats
// and issues when they ask for help.
if (PG_DSN !== '') {
    $p = parse_url(PG_DSN);
    $where = ($p['user'] ?? '?') . '@' . ($p['host'] ?? '?')
           . ':' . ($p['port'] ?? 5432) . '/' . ltrim($p['path'] ?? '', '/');
    line('OK', 'PG_DSN -> ' . $where);
} else {
    if (PG_HOST === '127.0.0.1' && PG_PASS === '') {
        line('WARN', 'no PG_DSN set — falling back to the local defaults');
    }
    $where = PG_USER . '@' . PG_HOST . ':' . PG_PORT . '/' . PG_NAME;
    line('OK', 'PG_* -> ' . $where);
}

// ── 3. the connection ───────────────────────────────────────────────────────
try {
    $pdo = SimDb::raw();
} catch (Throwable $e) {
    line('FAIL', 'cannot connect to ' . $where);
    echo '       ' . $e->getMessage() . "\n";
    exit(1);
}
$version = SimDb::fetchValue('SHOW server_version');
line('OK', 'connected — PostgreSQL ' . $version);

// ── 4. the schema ───────────────────────────────────────────────────────────
$expected = ['world', 'entity', 'entity_desire', 'entity_anger',
             'entity_social', 'entity_couple', 'entity_mental_model'];

$present = array_column(SimDb::fetchAll(
    "SELECT table_name FROM information_schema.tables WHERE table_schema = 'sim'"
), 'table_name');

$missing = array_values(array_diff($expected, $present));

if ($missing && $import) {
    $file = __DIR__ . '/../sql/schema_pg.sql';
    if (!is_file($file)) {
        line('FAIL', 'schema file not found: ' . $file);
        exit(1);
    }
    line('..', 'importing ' . basename($file));

    // The whole schema in one transaction: a half-applied schema is worse than
    // none, because the next run sees some tables and reports "present".
    $sql = str_replace("\r\n", "\n", file_get_contents($file));
    $pdo->beginTransaction();
    try {
        $pdo->exec($sql);
        $pdo->commit();
    } catch (PDOException $e) {
        $pdo->rollBack();
        line('FAIL', 'import failed — nothing was applied');
        echo '       ' . $e->getMessage() . "\n";
        exit(1);
    }

    $present = array_column(SimDb::fetchAll(
        "SELECT table_name FROM information_schema.tables WHERE table_schema = 'sim'"
    ), 'table_name');
    $missing = array_values(array_diff($expected, $present));
    line('OK', 'schema imported');
}

if ($missing) {
    line('FAIL', 'schema sim.* is incomplete, missing: ' . implode(', ', $missing));
    echo "\n  Create it with:  php website/bin/pg_check.php --import\n";
    exit(1);
}
line('OK', 'schema sim.* present (' . count($present) . ' tables)');

// ── 5. the data ─────────────────────────────────────────────────────────────
// An empty but valid schema is the normal state before the loader has ever
// run, and it is worth saying so — otherwise the dashboard is blank with no
// explanation and the database looks broken when it is merely new.
$worlds = SimDb::fetchAll(
    'SELECT world_id, label, last_day, last_push_at,
            (SELECT count(*) FROM sim.entity e
              WHERE e.world_id = w.world_id AND e.alive) AS alive
       FROM sim.world w ORDER BY world_id'
);

if (!$worlds) {
    line('WARN', 'no worlds yet — run the engine with --db-export and then the loader:');
    echo "         ./app --headless 500 --db-export bridge/spool --world-id " . SIM_WORLD_ID . "\n"
       . "         python scripts/db_spool_loader.py --dsn \"\$PG_DSN\"\n";
    exit(0);
}

foreach ($worlds as $w) {
    $mark = ((int)$w['world_id'] === SIM_WORLD_ID) ? ' <- SIM_WORLD_ID' : '';
    line('OK', sprintf(
        'world %d "%s": day %s, %s alive, last push %s%s',
        $w['world_id'],
        $w['label'] ?? '(unnamed)',
        $w['last_day'] ?? '?',
        $w['alive'],
        $w['last_push_at'] ?? 'never',
        $mark
    ));
}

if (!in_array(SIM_WORLD_ID, array_map('intval', array_column($worlds, 'world_id')), true)) {
    line('WARN', 'SIM_WORLD_ID=' . SIM_WORLD_ID . ' is not among them — the site will show nothing');
    exit(1);
}

echo "\nThe simulation database is connected and has data.\n";
