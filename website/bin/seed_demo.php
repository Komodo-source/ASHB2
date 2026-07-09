<?php
/**
 * ASHB2: Demo seeder (CLI only)
 *
 * Creates a demo user (demo / demo1234), one character, and ~8 fake
 * simulation days of state snapshots + actions so the dashboard renders
 * fully without the C++ engine. Idempotent: exits if 'demo' already exists.
 *
 * Usage:  php bin/seed_demo.php
 */

if (php_sapi_name() !== 'cli') exit(1);

require_once __DIR__ . '/../db.php';

echo 'Target database: ' . DB_USER . '@' . DB_HOST . ':' . DB_PORT . '/' . DB_NAME . "\n";

// ── Idempotency guard ───────────────────────────────────────────
$existing = Database::fetchOne('SELECT id FROM users WHERE username = ?', ['demo']);
if ($existing) {
    echo "Demo user already exists (id {$existing['id']}). Nothing to do.\n";
    exit(0);
}

// ── 1. Demo user ────────────────────────────────────────────────
$passwordHash = password_hash('demo1234', PASSWORD_ALGO, ['cost' => PASSWORD_COST]);

$userId = Database::insert(
    'INSERT INTO users (email, username, password_hash, display_name)
     VALUES (?, ?, ?, ?)',
    ['demo@example.com', 'demo', $passwordHash, 'Demo User']
);
echo "Created user 'demo' (id $userId) — password: demo1234\n";

// ── 2. Character (same column set as auth.php create_character) ─
$charId = Database::insert(
    'INSERT INTO characters (
        user_id, name,
        personality_openness, personality_conscientiousness,
        personality_extraversion, personality_agreeableness, personality_neuroticism,
        attachment_style, attachment_anxiety, attachment_avoidance,
        drive_exploration, drive_social, drive_safety, drive_dominance, drive_achievement,
        memory_decay_rate, memory_trauma_retention, memory_capacity
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
    [
        (int)$userId, 'Naelle Demo',
        0.720, 0.550, 0.610, 0.680, 0.340,
        'secure', 0.300, 0.300,
        0.650, 0.700, 0.450, 0.350, 0.550,
        0.050, 0.500, 100,
    ]
);
echo "Created character 'Naelle Demo' (id $charId)\n";

// Genome record, mirroring create_character()
Database::insert(
    'INSERT INTO character_genomes (character_id, speed, sight_range, metabolism, fertility, resilience)
     VALUES (?, 1.000, 1.000, 1.000, 1.000, 1.000)',
    [(int)$charId]
);

// ── 3. Eight fake simulation days ───────────────────────────────
$actionPool = [
    'forage'  => ['berry thicket', 'riverbank', 'oak grove'],
    'bond'    => ['Kera', 'Tomn', 'Ashka'],
    'explore' => ['northern ridge', 'salt flats', 'old caves'],
    'craft'   => ['stone knife', 'reed basket', 'fire drill'],
    'rest'    => [null],
];
$actionTypes = array_keys($actionPool);

// Plausible drifting vitals
$health = 88.0; $hunger = 22.0; $energy = 76.0; $stress = 28.0;
$happiness = 0.62; $fear = 0.15; $curiosity = 0.70;
$posX = 248; $posY = 310; $wealth = 4.0; $bonds = 0;

$clamp = fn(float $v, float $lo, float $hi): float => max($lo, min($hi, $v));

$totalActions = 0;
for ($day = 1; $day <= 8; $day++) {
    // Drift the state a little each day
    $health    = $clamp($health + mt_rand(-40, 25) / 10, 40.0, 100.0);
    $hunger    = $clamp($hunger + mt_rand(-30, 45) / 10, 0.0, 80.0);
    $energy    = $clamp($energy + mt_rand(-35, 30) / 10, 30.0, 100.0);
    $stress    = $clamp($stress + mt_rand(-25, 35) / 10, 5.0, 75.0);
    $happiness = $clamp($happiness + mt_rand(-8, 8) / 100, 0.20, 0.95);
    $fear      = $clamp($fear + mt_rand(-5, 6) / 100, 0.02, 0.60);
    $curiosity = $clamp($curiosity + mt_rand(-6, 6) / 100, 0.30, 0.95);
    $posX     += mt_rand(-12, 12);
    $posY     += mt_rand(-12, 12);
    $wealth    = $clamp($wealth + mt_rand(-10, 25) / 10, 0.0, 999.0);
    if ($day % 3 === 0 && $bonds < 4) $bonds++;

    $goalType = $actionTypes[array_rand($actionTypes)];

    $snapshotId = Database::insert(
        'INSERT INTO character_state_snapshots (
            character_id, simulation_day,
            health, hunger, energy, stress,
            pos_x, pos_y,
            num_bonds, num_enemies, wealth,
            mood_happiness, mood_fear, mood_curiosity,
            current_goal, goal_utility
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
        [
            (int)$charId, $day,
            round($health, 2), round($hunger, 2), round($energy, 2), round($stress, 2),
            $posX, $posY,
            $bonds, 0, round($wealth, 2),
            round($happiness, 3), round($fear, 3), round($curiosity, 3),
            $goalType, round(mt_rand(4500, 9500) / 10000, 4),
        ]
    );

    // 2-3 actions per day
    $numActions = mt_rand(2, 3);
    $picked = (array)array_rand($actionPool, $numActions);
    foreach ($picked as $type) {
        $targets = $actionPool[$type];
        $target = $targets[array_rand($targets)];
        Database::insert(
            'INSERT INTO character_actions (
                character_id, simulation_day, snapshot_id,
                action_type, action_target,
                utility_score, success, outcome_detail
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            [
                (int)$charId, $day, (int)$snapshotId,
                $type, $target,
                round(mt_rand(3000, 9800) / 10000, 4),
                mt_rand(0, 9) < 8 ? 1 : 0,
                ucfirst($type) . ($target !== null ? " — $target" : '') . " on day $day.",
            ]
        );
        $totalActions++;
    }
    echo "  Day $day: snapshot #$snapshotId + $numActions actions\n";
}

// Reflect the seeded timeline on the character row
Database::query(
    'UPDATE characters SET current_day = ?, released_at = NOW() WHERE id = ?',
    [8, (int)$charId]
);

echo "\nDone. 8 snapshots, $totalActions actions. Log in as demo / demo1234.\n";
