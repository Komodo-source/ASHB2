<?php
/**
 * ASHB2: Schema importer (CLI only)
 *
 * Reads ../sql/schema.sql, splits it into statements and executes each one
 * against the configured database. Usage:  php bin/import_schema.php
 */

if (php_sapi_name() !== 'cli') exit(1);

require_once __DIR__ . '/../db.php';

$schemaFile = __DIR__ . '/../sql/schema.sql';
if (!is_file($schemaFile)) {
    fwrite(STDERR, "ERROR: schema file not found: $schemaFile\n");
    exit(1);
}

echo 'Target database: ' . DB_USER . '@' . DB_HOST . ':' . DB_PORT . '/' . DB_NAME . "\n";

try {
    $pdo = Database::raw();
} catch (Throwable $e) {
    // Never echo the password. db.php may also die() on its own with the PDO
    // message; the target line above gives host/db context either way.
    fwrite(STDERR, 'ERROR: could not connect to ' . DB_HOST . ':' . DB_PORT
        . ' database "' . DB_NAME . '" — check your .env credentials.' . "\n");
    exit(1);
}

$sql = file_get_contents($schemaFile);
$sql = str_replace("\r\n", "\n", $sql);

// Split at end-of-statement semicolons (the file has one statement per block,
// each terminated by ";" at end of line).
$statements = preg_split('/;\s*\n/', $sql);

$executed = 0;
foreach ($statements as $statement) {
    $statement = trim($statement);
    if ($statement === '') {
        continue;
    }

    // Skip chunks that contain only comments / blank lines.
    $body = preg_replace('/^\s*--.*$/m', '', $statement);
    if (trim($body) === '') {
        continue;
    }

    // Human-readable label for progress output.
    if (preg_match('/CREATE\s+TABLE\s+`?(\w+)`?/i', $statement, $m)) {
        $label = 'CREATE TABLE ' . $m[1];
    } elseif (preg_match('/INSERT\s+INTO\s+`?(\w+)`?/i', $statement, $m)) {
        $label = 'INSERT INTO ' . $m[1];
    } else {
        $label = strtok($statement, " \n") . ' ...';
    }

    try {
        Database::raw()->exec($statement);
        $executed++;
        echo "  OK   $label\n";
    } catch (PDOException $e) {
        fwrite(STDERR, "  FAIL $label\n       " . $e->getMessage() . "\n");
        exit(1);
    }
}

$tables = Database::fetchAll('SHOW TABLES');
echo "\nDone. Executed $executed statements. Database now has " . count($tables) . " tables.\n";
