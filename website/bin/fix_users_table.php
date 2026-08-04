<?php
/**
 * Rebuild sim.users from sql/schema_pg.sql.
 *
 * ── Why this exists ─────────────────────────────────────────────────────────
 *
 * The deployed sim.users had drifted from the schema file. It was created by an
 * earlier definition and carried:
 *
 *   FOREIGN KEY (world_id, user_id) REFERENCES entity(world_id, sim_id)
 *
 * where the schema declares the reference on user_entity_id:
 *
 *   FOREIGN KEY (world_id, user_entity_id) REFERENCES sim.entity(world_id, sim_id)
 *
 * Those are different tables in every way that matters. The deployed one makes
 * user_id itself an entity id, which collapses the account key and the subject
 * key into one column — so accounts cannot be numbered independently, and
 * user_entity_id becomes an unconstrained integer pointing at nothing.
 *
 * It was also missing the PRIMARY KEY (user_id, world_id) entirely, so nothing
 * stopped two rows sharing an id, and the DEFAULT 1 on world_id.
 *
 * The drift is quiet: registration appears to work for as long as the allocated
 * user_id happens to match an existing sim_id, which it does while both start
 * at 1 and increment together. It fails the first time an entity dies or an
 * account is deleted and the two sequences separate.
 *
 * ── Why a rebuild rather than ALTER ─────────────────────────────────────────
 *
 * The CREATE statement is read out of sql/schema_pg.sql at run time rather than
 * copied here, so the result is the schema file by construction — a
 * hand-written sequence of ALTERs could drift from it again, which is the
 * problem being fixed.
 *
 * REFUSES TO RUN IF THE TABLE HAS ROWS. Rebuilding means dropping, and the
 * accounts in there are the only copy — the loader does not write sim.users and
 * cannot restore it. Migrate the rows by hand if you ever need this on a
 * populated table.
 *
 * Usage:  php website/bin/fix_users_table.php [--force]
 */

require_once __DIR__ . '/../sim_db.php';

$force = in_array('--force', $argv, true);
$schemaPath = __DIR__ . '/../sql/schema_pg.sql';

function say(string $status, string $msg): void
{
    printf("%-6s%s\n", $status, $msg);
}

$pdo = SimDb::raw();

// ── 1. Is there anything to lose? ───────────────────────────────────────────
$exists = SimDb::fetchValue(
    "SELECT to_regclass('sim.users') IS NOT NULL"
);

if ($exists) {
    $rows = (int)SimDb::fetchValue('SELECT count(*) FROM sim.users');
    if ($rows > 0 && !$force) {
        say('ABORT', "sim.users holds $rows row(s). Rebuilding drops them, and nothing else");
        say('', 'has a copy — the loader never writes this table. Export them first,');
        say('', 'or re-run with --force if they are disposable.');
        exit(1);
    }
    say('OK', "sim.users exists with $rows row(s)");
} else {
    say('OK', 'sim.users does not exist yet');
}

// ── 2. Report the drift, so the log says what changed ───────────────────────
if ($exists) {
    $before = SimDb::fetchAll(
        "SELECT conname, pg_get_constraintdef(oid) AS def
           FROM pg_constraint WHERE conrelid = 'sim.users'::regclass ORDER BY conname"
    );
    if (!$before) {
        say('WARN', 'no constraints at all — not even a primary key');
    }
    foreach ($before as $c) {
        say('WARN', "existing: {$c['conname']} — {$c['def']}");
    }
}

// ── 3. Lift the CREATE statement straight out of the schema file ────────────
$sql = @file_get_contents($schemaPath);
if ($sql === false) {
    say('FAIL', "cannot read $schemaPath");
    exit(1);
}

// From the CREATE through to the first line that closes it. The table's own
// body contains no ");" at the start of a line, so this is unambiguous.
if (!preg_match('/CREATE TABLE IF NOT EXISTS sim\.users\s*\(.*?^\);/ms', $sql, $m)) {
    say('FAIL', 'could not find the sim.users definition in sql/schema_pg.sql');
    exit(1);
}
$create = $m[0];

// ── 4. Rebuild ──────────────────────────────────────────────────────────────
try {
    $pdo->beginTransaction();
    $pdo->exec('DROP TABLE IF EXISTS sim.users');
    $pdo->exec($create);
    $pdo->commit();
} catch (Throwable $e) {
    if ($pdo->inTransaction()) {
        $pdo->rollBack();
    }
    say('FAIL', 'rebuild failed: ' . $e->getMessage());
    exit(1);
}

say('OK', 'sim.users rebuilt from sql/schema_pg.sql');

// ── 5. Verify against what the file asked for ───────────────────────────────
$after = SimDb::fetchAll(
    "SELECT conname, pg_get_constraintdef(oid) AS def
       FROM pg_constraint WHERE conrelid = 'sim.users'::regclass ORDER BY contype DESC, conname"
);
foreach ($after as $c) {
    say('OK', "{$c['conname']} — {$c['def']}");
}

$hasPk = false;
$hasFk = false;
foreach ($after as $c) {
    if (str_starts_with($c['def'], 'PRIMARY KEY (user_id, world_id)')) {
        $hasPk = true;
    }
    if (str_contains($c['def'], 'FOREIGN KEY (world_id, user_entity_id)')) {
        $hasFk = true;
    }
}

$ok = true;
if (!$hasPk) { say('FAIL', 'PRIMARY KEY (user_id, world_id) is missing'); $ok = false; }
if (!$hasFk) { say('FAIL', 'FOREIGN KEY (world_id, user_entity_id) is missing'); $ok = false; }

exit($ok ? 0 : 1);
