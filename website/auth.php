<?php
/**
 * ASHB2: Authentication System
 *
 * Registration, sign-in, session management — against PostgreSQL, on the schema
 * in sql/schema_pg.sql. Passwords are bcrypt (cost=12); every statement is
 * prepared.
 *
 * ── What changed, and why the shape of this file is different ────────────────
 *
 * This used to run on MySQL (sql/schema.sql), where `users` carried email,
 * display_name, is_active/is_verified/is_admin, login_count and timestamps, and
 * where a player's character was a row in `characters` that the web app created
 * from a questionnaire.
 *
 * The provided PostgreSQL schema models it differently, and this file follows
 * the schema rather than fighting it:
 *
 *     sim.users(user_id, world_id, user_entity_id, login, password)
 *          FOREIGN KEY (world_id, user_entity_id) -> sim.entity(world_id, sim_id)
 *
 * Three consequences run through everything below.
 *
 * 1. An account is identified by `login`, not email. There is no email column,
 *    so there is nothing to send to and nothing to verify.
 *
 * 2. A character is not created by the web app. The FK requires user_entity_id
 *    to already name a row in sim.entity, and sim.entity is written only by the
 *    C++ engine through scripts/db_spool_loader.py. So registration CLAIMS a
 *    person the simulation has already produced. This is why register_user()
 *    takes no personality scores: you do not author your entity, you are
 *    assigned one, and the engine had already decided who they are.
 *
 * 3. The key is composite. (user_id, world_id) — entity ids restart per world,
 *    so an account belongs to one world and $_SESSION carries both halves.
 *
 * Password reset is absent from the schema (no password_resets table, and no
 * email to send a token to). The two functions remain so the pages that call
 * them keep parsing, and both refuse in a way the UI can display.
 */

require_once __DIR__ . '/sim_db.php';

/**
 * The columns of sim.entity that stand in for the old `characters` row.
 *
 * Aliased to the names the dashboard already reads, so the presentation layer
 * did not have to be rewritten alongside the storage layer. The mapping is not
 * always one-to-one and the gaps are marked below — an honest NULL beats a
 * plausible zero on a page whose whole purpose is showing what the simulation
 * actually did.
 */
const ENTITY_AS_CHARACTER = "
    e.sim_id                    AS id,
    e.name                      AS name,
    e.alive                     AS is_alive,
    e.alive                     AS is_active,
    e.last_seen_day             AS current_day,
    e.age                       AS age,
    e.sex                       AS sex,
    e.life_stage                AS life_stage,
    e.openness                  AS personality_openness,
    e.conscientiousness         AS personality_conscientiousness,
    e.extraversion              AS personality_extraversion,
    e.agreeableness             AS personality_agreeableness,
    e.neuroticism               AS personality_neuroticism,
    e.attachment_style          AS attachment_style,
    -- QI: realized ability, the inherited ceiling, and the schooling between
    -- them. A character's mind is as much a part of who they are as their
    -- temperament, and unlike temperament it is something their world DID to
    -- them — so it belongs beside the Big Five, not buried in a stat dump.
    e.qi                        AS qi,
    e.qi_potential              AS qi_potential,
    e.school_years              AS school_years,
    e.tribe_id                  AS tribe_id,
    e.current_goal              AS current_goal,
    e.last_action               AS last_action,
    e.inner_monologue           AS inner_monologue,
    -- Only meaningful once alive = false; the loader flips that flag rather
    -- than deleting the row, so the last age it saw is the age at death.
    CASE WHEN e.alive THEN NULL ELSE e.age END AS age_at_death,
    -- No column for either. The engine exports neither a cause of death nor an
    -- offspring count, and there is no lineage table here to count from.
    NULL::text                  AS death_cause,
    NULL::integer               AS total_offspring
";

/**
 * Start a session if one isn't already active.
 */
function session_init(): void
{
    if (session_status() === PHP_SESSION_NONE) {
        session_start();
    }
}

/**
 * The signed-in user, or null.
 *
 * Joined to sim.entity so a caller gets the person's name without a second
 * query — and so that an account whose entity has been reaped along with its
 * world reads as signed-out rather than as a user with no character. The FK is
 * ON DELETE CASCADE, so that row genuinely no longer exists.
 */
function session_user(): ?array
{
    session_init();

    if (!isset($_SESSION['user_id'], $_SESSION['world_id'])) {
        return null;
    }

    if (isset($_SESSION['last_activity']) && (time() - $_SESSION['last_activity']) > SESSION_LIFETIME) {
        session_destroy();
        return null;
    }

    $_SESSION['last_activity'] = time();

    $row = SimDb::fetchOne(
        'SELECT u.user_id, u.world_id, u.user_entity_id, u.login,
                e.name AS entity_name, e.alive AS entity_alive
           FROM sim.users u
           JOIN sim.entity e
             ON e.world_id = u.world_id AND e.sim_id = u.user_entity_id
          WHERE u.user_id = ? AND u.world_id = ?',
        [$_SESSION['user_id'], $_SESSION['world_id']]
    );

    if (!$row) {
        return null;
    }

    return [
        'id'           => (int)$row['user_id'],
        'world_id'     => (int)$row['world_id'],
        'entity_id'    => (int)$row['user_entity_id'],
        'username'     => $row['login'],
        // The dashboard prints display_name and falls back to username. There
        // is no display_name column, so the entity's own name is the better
        // answer: it is what the person is called inside the world.
        'display_name' => $row['entity_name'],
        'entity_alive' => (bool)$row['entity_alive'],
    ];
}

/**
 * Require authentication. Redirects to the login page if not signed in.
 */
function require_auth(): array
{
    $user = session_user();
    if ($user === null) {
        session_init();
        $_SESSION['redirect_after_login'] = $_SERVER['REQUEST_URI'] ?? 'dashboard.php';
        header('Location: login.php');
        exit;
    }
    return $user;
}

/**
 * An unclaimed, living entity in the given world — the person a new account
 * will be assigned.
 *
 * NOT EXISTS rather than a LEFT JOIN: sim.users has no unique constraint on
 * user_entity_id, so a join could multiply rows if the same entity were ever
 * claimed twice, and the anti-join says what is meant.
 *
 * ORDER BY random() is deliberate. Taking the lowest free sim_id would hand
 * every early account one of the founding generation, which are the entities
 * with the longest histories and the most relationships — a systematically
 * unrepresentative sample of the world.
 */
function find_unclaimed_entity(int $worldId): ?int
{
    $row = SimDb::fetchOne(
        'SELECT e.sim_id
           FROM sim.entity e
          WHERE e.world_id = ?
            AND e.alive
            AND NOT EXISTS (
                SELECT 1 FROM sim.users u
                 WHERE u.world_id = e.world_id
                   AND u.user_entity_id = e.sim_id
            )
          ORDER BY random()
          LIMIT 1',
        [$worldId]
    );

    return $row ? (int)$row['sim_id'] : null;
}

/**
 * Register a new account and bind it to an entity.
 *
 * @param int|null $entityId Claim this specific entity; null picks a free one.
 */
function register_user(string $login, string $password, ?int $entityId = null): array
{
    $login = trim($login);

    if (strlen($login) < 3 || strlen($login) > 60) {
        return ['success' => false, 'error' => 'Login must be 3–60 characters.'];
    }

    if (!preg_match('/^[a-zA-Z0-9_-]+$/', $login)) {
        return ['success' => false, 'error' => 'Login can only contain letters, numbers, hyphens, and underscores.'];
    }

    if (strlen($password) < 8) {
        return ['success' => false, 'error' => 'Password must be at least 8 characters.'];
    }

    $worldId = SIM_WORLD_ID;

    // Nothing to claim before the engine has run. Said plainly, because the
    // alternative — a foreign key violation surfacing as "registration failed"
    // — sends people looking for a bug in this function.
    $worldExists = SimDb::fetchValue(
        'SELECT 1 FROM sim.world WHERE world_id = ?',
        [$worldId]
    );
    if (!$worldExists) {
        return [
            'success' => false,
            'error'   => "World $worldId has no data yet. Run the engine with --db-export "
                       . 'and load the spool before creating an account.',
        ];
    }

    // The schema declares no UNIQUE on login, so uniqueness is this check plus
    // the insert below losing a race it cannot detect. Two people claiming the
    // same name in the same millisecond is not a threat model worth a schema
    // change; two people claiming the same ENTITY is, and that one the
    // primary key does catch.
    $taken = SimDb::fetchValue(
        'SELECT 1 FROM sim.users WHERE world_id = ? AND lower(login) = lower(?)',
        [$worldId, $login]
    );
    if ($taken) {
        return ['success' => false, 'error' => 'That login is already taken.'];
    }

    if ($entityId === null) {
        $entityId = find_unclaimed_entity($worldId);
    }
    if ($entityId === null) {
        return [
            'success' => false,
            'error'   => 'Every living entity in this world is already claimed. '
                       . 'Let the simulation run further and try again.',
        ];
    }

    $hash = password_hash($password, PASSWORD_ALGO, ['cost' => PASSWORD_COST]);

    // user_id by hand: the schema types it `integer NOT NULL` with no sequence
    // behind it, so there is no DEFAULT to lean on and no lastInsertId to read.
    //
    // MAX+1 races, and the retry is the whole point of the loop — the primary
    // key (user_id, world_id) is what actually decides the winner, so a loser
    // sees 23505 and recomputes instead of overwriting. Bounded rather than
    // while(true): if something other than contention is producing 23505 —
    // most likely the entity being claimed a moment ago, which trips the same
    // code — an unbounded loop would spin on it forever.
    for ($attempt = 0; $attempt < 5; $attempt++) {
        $nextId = (int)(SimDb::fetchValue(
            'SELECT COALESCE(MAX(user_id), 0) + 1 FROM sim.users WHERE world_id = ?',
            [$worldId]
        ) ?? 1);

        try {
            SimDb::execute(
                'INSERT INTO sim.users (user_id, world_id, user_entity_id, login, password)
                 VALUES (?, ?, ?, ?, ?)',
                [$nextId, $worldId, $entityId, $login, $hash]
            );

            return [
                'success'   => true,
                'user_id'   => $nextId,
                'world_id'  => $worldId,
                'entity_id' => $entityId,
            ];
        } catch (PDOException $e) {
            if (($e->getCode() ?? '') !== '23505') {
                error_log('Registration failed: ' . $e->getMessage());
                return ['success' => false, 'error' => 'Could not create the account. Please try again.'];
            }
            // Lost the race for this user_id. If the collision was on the
            // entity instead, pick a different one before trying again.
            if ($entityId !== null && !SimDb::fetchValue(
                'SELECT 1 FROM sim.entity e WHERE e.world_id = ? AND e.sim_id = ?
                   AND NOT EXISTS (SELECT 1 FROM sim.users u
                                    WHERE u.world_id = e.world_id AND u.user_entity_id = e.sim_id)',
                [$worldId, $entityId]
            )) {
                $entityId = find_unclaimed_entity($worldId);
                if ($entityId === null) {
                    return ['success' => false, 'error' => 'Every living entity in this world is already claimed.'];
                }
            }
        }
    }

    return ['success' => false, 'error' => 'Could not create the account. Please try again.'];
}

/**
 * Authenticate a sign-in.
 *
 * By login, not email — see the header. The caller-facing error never says
 * which half was wrong, or the form becomes an account-enumeration oracle.
 */
function login_user(string $login, string $password): array
{
    $login = trim($login);

    $user = SimDb::fetchOne(
        'SELECT u.user_id, u.world_id, u.user_entity_id, u.login, u.password,
                e.name AS entity_name, e.alive AS entity_alive
           FROM sim.users u
           JOIN sim.entity e
             ON e.world_id = u.world_id AND e.sim_id = u.user_entity_id
          WHERE u.world_id = ? AND lower(u.login) = lower(?)',
        [SIM_WORLD_ID, $login]
    );

    // password_verify against a known-bad hash on the miss path, so that "no
    // such login" and "wrong password" take comparable time. Without it the
    // miss returns immediately and the difference is measurable — bcrypt at
    // cost 12 is ~250ms, which is not a subtle timing signal.
    if (!$user) {
        password_verify($password, '$2y$12$' . str_repeat('.', 53));
        return ['success' => false, 'error' => 'Invalid login or password.'];
    }

    if (!password_verify($password, $user['password'])) {
        return ['success' => false, 'error' => 'Invalid login or password.'];
    }

    if (password_needs_rehash($user['password'], PASSWORD_ALGO, ['cost' => PASSWORD_COST])) {
        SimDb::execute(
            'UPDATE sim.users SET password = ? WHERE user_id = ? AND world_id = ?',
            [
                password_hash($password, PASSWORD_ALGO, ['cost' => PASSWORD_COST]),
                $user['user_id'],
                $user['world_id'],
            ]
        );
    }

    // No last_login_at or login_count in this schema, so nothing is stamped
    // here. Dropped rather than emulated: a column that does not exist should
    // not be faked into the session.

    session_init();
    session_regenerate_id(true);

    $_SESSION['user_id']       = (int)$user['user_id'];
    $_SESSION['world_id']      = (int)$user['world_id'];
    $_SESSION['last_activity'] = time();

    return [
        'success' => true,
        'user' => [
            'id'           => (int)$user['user_id'],
            'world_id'     => (int)$user['world_id'],
            'entity_id'    => (int)$user['user_entity_id'],
            'username'     => $user['login'],
            'display_name' => $user['entity_name'],
            'entity_alive' => (bool)$user['entity_alive'],
        ],
    ];
}

/**
 * Log out the current user.
 */
function logout_user(): void
{
    session_init();
    $_SESSION = [];

    if (ini_get('session.use_cookies')) {
        $params = session_get_cookie_params();
        setcookie(
            session_name(),
            '',
            time() - 42000,
            $params['path'],
            $params['domain'],
            $params['secure'],
            $params['httponly']
        );
    }

    session_destroy();
}

/**
 * Password reset — not available on this schema.
 *
 * It needs two things sql/schema_pg.sql does not provide: somewhere to store a
 * token with an expiry (the old `password_resets` table), and an email address
 * to send it to (sim.users has `login`, and nothing else identifying).
 *
 * Kept as a function, and failing loudly, so reset-password.php still parses
 * and can tell the visitor the truth instead of pretending a mail was sent.
 */
function password_reset_available(): bool
{
    return false;
}

function create_password_reset(string $login): array
{
    return [
        'success' => false,
        'error'   => 'Passphrase recovery is unavailable: this deployment stores no email address. '
                   . 'Ask an administrator to set a new passphrase.',
    ];
}

function complete_password_reset(string $token, string $newPassword): array
{
    return [
        'success' => false,
        'error'   => 'Passphrase recovery is unavailable on this deployment.',
    ];
}

/**
 * The user's character, as a one-element list.
 *
 * A list rather than a single row because the dashboard loops over it and shows
 * a roster. On this schema the roster is always exactly one long — sim.users
 * binds one account to one entity — but returning the shape the caller expects
 * keeps the "no character yet" branch working for an account whose entity has
 * gone, and leaves room if a world ever allows more.
 */
function get_user_characters(int $userId, ?int $worldId = null): array
{
    $worldId ??= ($_SESSION['world_id'] ?? SIM_WORLD_ID);

    return SimDb::fetchAll(
        'SELECT ' . ENTITY_AS_CHARACTER . '
           FROM sim.users u
           JOIN sim.entity e
             ON e.world_id = u.world_id AND e.sim_id = u.user_entity_id
          WHERE u.user_id = ? AND u.world_id = ?',
        [$userId, $worldId]
    );
}

/**
 * The character's current state.
 *
 * "Latest snapshot" in the old schema meant the newest row in a history table.
 * There is no history here: the loader upserts each entity in place, so one
 * entity is always exactly one row and the current row IS the latest state.
 * last_seen_day stands in for simulation_day — it is the last civ-day on which
 * the engine pushed this person.
 */
function get_latest_character_state(int $entityId, ?int $worldId = null): ?array
{
    $worldId ??= ($_SESSION['world_id'] ?? SIM_WORLD_ID);

    return SimDb::fetchOne(
        'SELECT last_seen_day AS simulation_day,
                health, hunger, stress, happiness, mental_health,
                hygiene, loneliness, boredom, general_anger,
                energy_level AS energy,
                fatigue_level, sleep_pressure, sleep_quality,
                food_store, antibody, disease_type,
                pos_x, pos_y
           FROM sim.entity
          WHERE world_id = ? AND sim_id = ?',
        [$worldId, $entityId]
    );
}

/**
 * Recent actions.
 *
 * sim.entity keeps only the most recent one — there is no character_actions
 * table and the engine exports no action log — so this returns at most a single
 * row, in the list shape the dashboard iterates. utility_score has no column
 * and stays null rather than being invented.
 */
function get_recent_actions(int $entityId, int $limit = 20, ?int $worldId = null): array
{
    $worldId ??= ($_SESSION['world_id'] ?? SIM_WORLD_ID);

    if ($limit < 1) {
        return [];
    }

    $row = SimDb::fetchOne(
        'SELECT last_seen_day AS simulation_day,
                last_action   AS action_type,
                current_goal  AS action_target,
                inner_monologue,
                NULL::real    AS utility_score
           FROM sim.entity
          WHERE world_id = ? AND sim_id = ? AND last_action IS NOT NULL',
        [$worldId, $entityId]
    );

    return $row ? [$row] : [];
}

/**
 * life_stage and attachment_style are smallints, not words.
 *
 * The engine exports them as the raw enum ordinal (src/header/Entity.h), so a
 * page that prints the column prints "3". These two functions are the only
 * place that knows the ordering, and they must track those enums: inserting a
 * value into the middle of either C++ enum silently relabels every stored row,
 * because the number in the database does not change.
 *
 * Unknown ordinals come back as "unknown (n)" rather than a default like
 * "secure" — a wrong label that looks right is worse than an obviously odd one.
 */
function life_stage_label(?int $ordinal): string
{
    // enum LifeStage { INFANT, CHILD, ADOLESCENT, ADULT, ELDER };
    $labels = ['infant', 'child', 'adolescent', 'adult', 'elder'];
    if ($ordinal === null) {
        return 'unknown';
    }
    return $labels[$ordinal] ?? "unknown ($ordinal)";
}

function attachment_style_label(?int $ordinal): string
{
    // enum AttachmentStyle { SECURE, ANXIOUS, AVOIDANT, DISORGANIZED };
    $labels = ['secure', 'anxious', 'avoidant', 'disorganized'];
    if ($ordinal === null) {
        return 'unknown';
    }
    return $labels[$ordinal] ?? "unknown ($ordinal)";
}

/**
 * The world's headline numbers, for the dashboard banner.
 */
function get_world_summary(?int $worldId = null): ?array
{
    $worldId ??= ($_SESSION['world_id'] ?? SIM_WORLD_ID);

    return SimDb::fetchOne(
        'SELECT w.world_id, w.label, w.last_day, w.started_at, w.last_push_at,
                (SELECT count(*) FROM sim.entity e
                  WHERE e.world_id = w.world_id AND e.alive) AS population
           FROM sim.world w
          WHERE w.world_id = ?',
        [$worldId]
    );
}
