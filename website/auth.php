<?php
/**
 * ASHB2: Authentication System
 * 
 * Handles user registration, login, session management, and password resets.
 * All passwords hashed with bcrypt (cost=12). All SQL uses prepared statements.
 */

require_once __DIR__ . '/db.php';

/**
 * Attachment style → dimensional anxiety/avoidance mapping.
 * Based on the two-dimensional model of attachment (Brennan, Clark, Shaver 1998).
 * Maps each categorical style to its typical (anxiety, avoidance) coordinates.
 * Used by create_character() to populate the dimensional columns for ML training.
 */
function get_attachment_dimensions(string $style): array
{
    $map = [
        'secure'       => [0.300, 0.300],
        'anxious'      => [0.700, 0.200],
        'avoidant'     => [0.200, 0.700],
        'disorganized' => [0.600, 0.600],
        'fearful'      => [0.600, 0.500],
    ];
    return $map[$style] ?? [0.500, 0.500];
}

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
 * Check if a user is currently logged in.
 * Returns the user array if authenticated, null otherwise.
 */
function session_user(): ?array
{
    session_init();

    if (!isset($_SESSION['user_id'])) {
        return null;
    }

    // Validate session hasn't expired
    if (isset($_SESSION['last_activity']) && (time() - $_SESSION['last_activity']) > SESSION_LIFETIME) {
        session_destroy();
        return null;
    }

    $_SESSION['last_activity'] = time();

    return Database::fetchOne(
        'SELECT id, email, username, display_name, is_admin, is_verified, last_login_at, login_count, created_at
         FROM users WHERE id = ? AND is_active = 1',
        [$_SESSION['user_id']]
    );
}

/**
 * Require authentication. Redirects to login page if not authenticated.
 */
function require_auth(): array
{
    $user = session_user();
    if ($user === null) {
        $_SESSION['redirect_after_login'] = $_SERVER['REQUEST_URI'];
        header('Location: login.php');
        exit;
    }
    return $user;
}

/**
 * Register a new user account.
 */
function register_user(string $email, string $username, string $password, string $displayName = ''): array
{
    $email = trim(strtolower($email));
    $username = trim($username);
    $displayName = trim($displayName) ?: $username;

    if (!filter_var($email, FILTER_VALIDATE_EMAIL)) {
        return ['success' => false, 'error' => 'Invalid email address.'];
    }

    if (strlen($username) < 3 || strlen($username) > 60) {
        return ['success' => false, 'error' => 'Username must be 3–60 characters.'];
    }

    if (!preg_match('/^[a-zA-Z0-9_-]+$/', $username)) {
        return ['success' => false, 'error' => 'Username can only contain letters, numbers, hyphens, and underscores.'];
    }

    if (strlen($password) < 8) {
        return ['success' => false, 'error' => 'Password must be at least 8 characters.'];
    }

    $existing = Database::fetchOne(
        'SELECT id FROM users WHERE email = ? OR username = ?',
        [$email, $username]
    );

    if ($existing) {
        return ['success' => false, 'error' => 'Email or username is already registered.'];
    }

    $passwordHash = password_hash($password, PASSWORD_ALGO, ['cost' => PASSWORD_COST]);

    $userId = Database::insert(
        'INSERT INTO users (email, username, password_hash, display_name)
         VALUES (?, ?, ?, ?)',
        [$email, $username, $passwordHash, $displayName]
    );

    return ['success' => true, 'user_id' => (int)$userId];
}

/**
 * Authenticate a user login.
 */
function login_user(string $email, string $password): array
{
    $email = trim(strtolower($email));

    $user = Database::fetchOne(
        'SELECT id, email, username, display_name, password_hash, is_active, is_verified, is_admin
         FROM users WHERE email = ?',
        [$email]
    );

    if (!$user) {
        return ['success' => false, 'error' => 'Invalid email or password.'];
    }

    if (!$user['is_active']) {
        return ['success' => false, 'error' => 'This account has been deactivated.'];
    }

    if (!password_verify($password, $user['password_hash'])) {
        return ['success' => false, 'error' => 'Invalid email or password.'];
    }

    if (password_needs_rehash($user['password_hash'], PASSWORD_ALGO, ['cost' => PASSWORD_COST])) {
        $newHash = password_hash($password, PASSWORD_ALGO, ['cost' => PASSWORD_COST]);
        Database::query(
            'UPDATE users SET password_hash = ? WHERE id = ?',
            [$newHash, $user['id']]
        );
    }

    Database::query(
        'UPDATE users SET last_login_at = NOW(), login_count = login_count + 1 WHERE id = ?',
        [$user['id']]
    );

    session_init();
    session_regenerate_id(true);

    $_SESSION['user_id'] = $user['id'];
    $_SESSION['last_activity'] = time();

    return [
        'success' => true,
        'user' => [
            'id'           => (int)$user['id'],
            'email'        => $user['email'],
            'username'     => $user['username'],
            'display_name' => $user['display_name'],
            'is_admin'     => (bool)$user['is_admin'],
            'is_verified'  => (bool)$user['is_verified'],
        ]
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
 * Initiate a password reset.
 */
function create_password_reset(string $email): array
{
    $email = trim(strtolower($email));

    $user = Database::fetchOne(
        'SELECT id FROM users WHERE email = ? AND is_active = 1',
        [$email]
    );

    if (!$user) {
        return ['success' => true, 'message' => 'If the email exists, a reset link has been sent.'];
    }

    Database::query(
        'UPDATE password_resets SET is_used = 1 WHERE user_id = ? AND is_used = 0',
        [$user['id']]
    );

    $token = bin2hex(random_bytes(32));
    $tokenHash = hash('sha256', $token);
    $expiresAt = date('Y-m-d H:i:s', time() + 3600);

    Database::insert(
        'INSERT INTO password_resets (user_id, token_hash, expires_at) VALUES (?, ?, ?)',
        [$user['id'], $tokenHash, $expiresAt]
    );

    return [
        'success' => true,
        'token'   => $token,
        'message' => 'If the email exists, a reset link has been sent.',
    ];
}

/**
 * Complete a password reset.
 */
function complete_password_reset(string $token, string $newPassword): array
{
    if (strlen($newPassword) < 8) {
        return ['success' => false, 'error' => 'Password must be at least 8 characters.'];
    }

    $tokenHash = hash('sha256', $token);

    $reset = Database::fetchOne(
        'SELECT id, user_id, expires_at FROM password_resets
         WHERE token_hash = ? AND is_used = 0 AND expires_at > NOW()',
        [$tokenHash]
    );

    if (!$reset) {
        return ['success' => false, 'error' => 'Invalid or expired reset token.'];
    }

    $passwordHash = password_hash($newPassword, PASSWORD_ALGO, ['cost' => PASSWORD_COST]);

    Database::query('UPDATE users SET password_hash = ? WHERE id = ?', [
        $passwordHash, $reset['user_id']
    ]);

    Database::query('UPDATE password_resets SET is_used = 1 WHERE id = ?', [
        $reset['id']
    ]);

    return ['success' => true, 'message' => 'Password has been reset successfully.'];
}

/**
 * Get all characters belonging to the current user.
 */
function get_user_characters(int $userId): array
{
    return Database::fetchAll(
        'SELECT id, name, is_active, is_alive, current_day, age_at_death, death_cause,
                total_offspring, created_at, released_at, died_at,
                personality_openness, personality_conscientiousness,
                personality_extraversion, personality_agreeableness,
                personality_neuroticism, attachment_style,
                attachment_anxiety, attachment_avoidance,
                drive_exploration, drive_social, drive_safety, drive_dominance, drive_achievement,
                memory_decay_rate, memory_trauma_retention, memory_capacity
         FROM characters
         WHERE user_id = ?
         ORDER BY created_at DESC',
        [$userId]
    );
}

/**
 * Create a new character for a user.
 * 
 * Inserts the full psychological profile, derives attachment dimensional
 * scores from the categorical style, and creates a genome record.
 */
function create_character(int $userId, array $data): array
{
    $required = ['name', 'personality_openness', 'personality_conscientiousness',
                  'personality_extraversion', 'personality_agreeableness', 'personality_neuroticism'];

    foreach ($required as $field) {
        if (!isset($data[$field]) || $data[$field] === '') {
            return ['success' => false, 'error' => "Missing required field: $field"];
        }
    }

    // Derive attachment dimensional scores from categorical style
    $attachmentStyle = $data['attachment_style'] ?? 'secure';
    $attachmentDims = get_attachment_dimensions($attachmentStyle);

    $db = Database::raw();
    try {
        $db->beginTransaction();

        // Insert character
        Database::insert(
            'INSERT INTO characters (
                user_id, name,
                personality_openness, personality_conscientiousness,
                personality_extraversion, personality_agreeableness, personality_neuroticism,
                attachment_style, attachment_anxiety, attachment_avoidance,
                drive_exploration, drive_social, drive_safety, drive_dominance, drive_achievement,
                memory_decay_rate, memory_trauma_retention, memory_capacity
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
            [
                $userId,
                $data['name'],
                (float)$data['personality_openness'],
                (float)$data['personality_conscientiousness'],
                (float)$data['personality_extraversion'],
                (float)$data['personality_agreeableness'],
                (float)$data['personality_neuroticism'],
                $attachmentStyle,
                $attachmentDims[0],
                $attachmentDims[1],
                (float)($data['drive_exploration'] ?? 0.500),
                (float)($data['drive_social'] ?? 0.500),
                (float)($data['drive_safety'] ?? 0.500),
                (float)($data['drive_dominance'] ?? 0.500),
                (float)($data['drive_achievement'] ?? 0.500),
                (float)($data['memory_decay_rate'] ?? 0.050),
                (float)($data['memory_trauma_retention'] ?? 0.500),
                (int)($data['memory_capacity'] ?? 100),
            ]
        );

        $charId = (int)$db->lastInsertId();

        // Create genome record (default 1.0 traits, no NEAT brain yet)
        Database::insert(
            'INSERT INTO character_genomes (character_id, speed, sight_range, metabolism, fertility, resilience)
             VALUES (?, 1.000, 1.000, 1.000, 1.000, 1.000)',
            [$charId]
        );

        $db->commit();
        return ['success' => true, 'character_id' => $charId];

    } catch (Exception $e) {
        $db->rollBack();
        error_log('Character creation failed: ' . $e->getMessage());
        return ['success' => false, 'error' => 'Failed to create character. Please try again.'];
    }
}

/**
 * Get the latest state snapshot for a character.
 */
function get_latest_character_state(int $characterId): ?array
{
    return Database::fetchOne(
        'SELECT css.* FROM character_state_snapshots css
         WHERE css.character_id = ?
         ORDER BY css.simulation_day DESC
         LIMIT 1',
        [$characterId]
    );
}

/**
 * Get recent actions for a character.
 */
function get_recent_actions(int $characterId, int $limit = 20): array
{
    return Database::fetchAll(
        'SELECT ca.*, css.health, css.hunger, css.energy, css.stress
         FROM character_actions ca
         LEFT JOIN character_state_snapshots css ON ca.snapshot_id = css.id
         WHERE ca.character_id = ?
         ORDER BY ca.executed_at DESC
         LIMIT ?',
        [$characterId, $limit]
    );
}
