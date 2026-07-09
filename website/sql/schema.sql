-- ═══════════════════════════════════════════════════════════════════
-- ASHB2: Human-in-the-Loop Simulation — Database Schema
-- Version: 1.0
-- Engine: MySQL / MariaDB
-- 
-- Design philosophy:
-- - Append-only state tables for complete trajectory capture
-- - Psychometric depth for personality → behavior ML modeling
-- - Structured user feedback for RLHF training signals
-- - Temporal granularity for sequence modeling
-- ═══════════════════════════════════════════════════════════════════

-- ── 1. USERS ────────────────────────────────────────────────────
-- Identity and authentication. Tracks engagement for churn prediction.

CREATE TABLE users (
    id              INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    email           VARCHAR(255) NOT NULL UNIQUE,
    username        VARCHAR(60)  NOT NULL UNIQUE,
    password_hash   VARCHAR(255) NOT NULL,              -- bcrypt
    display_name    VARCHAR(120) DEFAULT NULL,
    
    -- Demographics (optional, for research)
    birth_year      YEAR         DEFAULT NULL,
    country_code    CHAR(2)      DEFAULT NULL,          -- ISO 3166-1 alpha-2
    
    -- Account state
    is_verified     BOOLEAN      NOT NULL DEFAULT FALSE,
    is_active       BOOLEAN      NOT NULL DEFAULT TRUE,
    is_admin        BOOLEAN      NOT NULL DEFAULT FALSE,
    
    -- Engagement tracking
    last_login_at   DATETIME     DEFAULT NULL,
    login_count     INT UNSIGNED NOT NULL DEFAULT 0,
    
    -- Timestamps
    created_at      DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at      DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    
    INDEX idx_users_email (email),
    INDEX idx_users_username (username),
    INDEX idx_users_active (is_active),
    INDEX idx_users_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 2. PASSWORD RESETS ──────────────────────────────────────────
-- Secure token-based password reset flow.

CREATE TABLE password_resets (
    id              INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    user_id         INT UNSIGNED NOT NULL,
    token_hash      VARCHAR(64)  NOT NULL,              -- SHA-256 of token
    expires_at      DATETIME     NOT NULL,
    is_used         BOOLEAN      NOT NULL DEFAULT FALSE,
    created_at      DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_resets_token (token_hash),
    INDEX idx_resets_user (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 3. CHARACTERS (The Core Entity) ─────────────────────────────
-- Each user can have multiple characters over time.
-- A character is a digital twin of the user's psyche.
-- This table captures the FULL psychological profile for ML modeling.

CREATE TABLE characters (
    id              INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    user_id         INT UNSIGNED NOT NULL,
    name            VARCHAR(120) NOT NULL,
    is_active       BOOLEAN      NOT NULL DEFAULT TRUE, -- currently in simulation
    is_alive        BOOLEAN      NOT NULL DEFAULT TRUE,
    
    ── Big Five Personality (0.0 – 1.0 scale) ─────────────────
    -- OCEAN model: Openness, Conscientiousness, Extraversion,
    -- Agreeableness, Neuroticism
    personality_openness        DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    personality_conscientiousness DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    personality_extraversion    DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    personality_agreeableness   DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    personality_neuroticism     DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    
    ── Attachment Style ────────────────────────────────────────
    -- secure, anxious, avoidant, disorganized, fearful
    attachment_style            ENUM('secure','anxious','avoidant','disorganized','fearful')
                                NOT NULL DEFAULT 'secure',
    attachment_anxiety          DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    attachment_avoidance        DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    
    ── Core Drives (0.0 – 1.0) ─────────────────────────────────
    drive_exploration           DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    drive_social                DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    drive_safety                DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    drive_dominance             DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    drive_achievement           DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    
    ── Memory Parameters (for episodic memory simulation) ─────
    -- Decay rate: how fast memories fade (0.01 = slow, 0.20 = fast)
    -- Trauma retention: how strongly negative events are remembered
    memory_decay_rate           DECIMAL(4,3) NOT NULL DEFAULT 0.050,
    memory_trauma_retention     DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    memory_capacity             SMALLINT UNSIGNED NOT NULL DEFAULT 100,
    
    ── Initial Simulation State (set when released into world) ─
    spawn_region_x              INT NOT NULL DEFAULT 0,
    spawn_region_y              INT NOT NULL DEFAULT 0,
    initial_wealth              DECIMAL(8,2) NOT NULL DEFAULT 0.00,
    initial_knowledge           JSON DEFAULT NULL,      -- starting recipes/tools
    
    ── Lifecycle ───────────────────────────────────────────────
    current_day                 INT UNSIGNED NOT NULL DEFAULT 0,  -- simulation day
    age_at_death                INT UNSIGNED DEFAULT NULL,
    death_cause                 VARCHAR(255) DEFAULT NULL,
    total_offspring             INT UNSIGNED NOT NULL DEFAULT 0,
    
    ── Timestamps ──────────────────────────────────────────────
    created_at                  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    released_at                 DATETIME DEFAULT NULL,   -- when injected into simulation
    died_at                     DATETIME DEFAULT NULL,
    updated_at                  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_characters_user (user_id),
    INDEX idx_characters_active (is_active, is_alive),
    INDEX idx_characters_personality (personality_openness, personality_neuroticism),
    INDEX idx_characters_attachment (attachment_style),
    INDEX idx_characters_released (released_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 4. CHARACTER STATE SNAPSHOTS ───────────────────────────────
-- Append-only time-series of character state at each simulation day.
-- CRITICAL for ML training: provides complete state→action trajectories.

CREATE TABLE character_state_snapshots (
    id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id    INT UNSIGNED NOT NULL,
    simulation_day  INT UNSIGNED NOT NULL,              -- which day of simulation
    
    ── Vital stats ────────────────────────────────────────────
    health          DECIMAL(5,2) NOT NULL,              -- 0.00 – 100.00
    hunger          DECIMAL(5,2) NOT NULL,              -- 0.00 – 100.00
    energy          DECIMAL(5,2) NOT NULL,              -- 0.00 – 100.00
    stress          DECIMAL(5,2) NOT NULL,              -- 0.00 – 100.00
    
    ── Position ───────────────────────────────────────────────
    pos_x           INT NOT NULL,
    pos_y           INT NOT NULL,
    region_id       INT UNSIGNED DEFAULT NULL,          -- FK to regions (future)
    
    ── Social state ──────────────────────────────────────────
    current_tribe_id    INT UNSIGNED DEFAULT NULL,      -- FK to tribes (future)
    social_standing     DECIMAL(4,3) DEFAULT NULL,      -- 0.0 – 1.0 within group
    num_bonds           SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    num_enemies         SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    
    ── Economic state ────────────────────────────────────────
    wealth              DECIMAL(8,2) NOT NULL DEFAULT 0.00,
    inventory_json      JSON DEFAULT NULL,              -- current items
    known_recipes       JSON DEFAULT NULL,              -- learned recipes
    
    ── Emotional state (for affective computing models) ──────
    mood_happiness      DECIMAL(4,3) DEFAULT NULL,      -- 0.0 – 1.0
    mood_fear           DECIMAL(4,3) DEFAULT NULL,      -- 0.0 – 1.0
    mood_anger          DECIMAL(4,3) DEFAULT NULL,      -- 0.0 – 1.0
    mood_sadness        DECIMAL(4,3) DEFAULT NULL,      -- 0.0 – 1.0
    mood_curiosity      DECIMAL(4,3) DEFAULT NULL,      -- 0.0 – 1.0
    
    ── Cognitive state ───────────────────────────────────────
    current_goal        VARCHAR(255) DEFAULT NULL,      -- what the GOAP planner chose
    goal_utility        DECIMAL(6,4) DEFAULT NULL,      -- utility score of chosen goal
    deliberation_depth  SMALLINT UNSIGNED DEFAULT NULL, -- how many steps the planner searched
    hesitations         SMALLINT UNSIGNED DEFAULT NULL, -- number of times decision was re-weighed
    
    ── Timestamp ─────────────────────────────────────────────
    recorded_at         DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    INDEX idx_snapshots_char (character_id),
    INDEX idx_snapshots_day (simulation_day),
    INDEX idx_snapshots_char_day (character_id, simulation_day),
    INDEX idx_snapshots_health (health),
    INDEX idx_snapshots_mood (mood_happiness, mood_fear, mood_anger),
    INDEX idx_snapshots_recorded (recorded_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 5. CHARACTER ACTIONS ───────────────────────────────────────
-- Every decision a character makes, logged with full context.
-- PRIMARY table for behavioral cloning / imitation learning.

CREATE TABLE character_actions (
    id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id    INT UNSIGNED NOT NULL,
    simulation_day  INT UNSIGNED NOT NULL,
    snapshot_id     BIGINT UNSIGNED DEFAULT NULL,       -- FK to state BEFORE action
    
    ── Action details ────────────────────────────────────────
    action_type     VARCHAR(60) NOT NULL,               -- e.g. 'forage', 'bond', 'attack', 'explore'
    action_target   VARCHAR(255) DEFAULT NULL,           -- target entity/object ID
    action_location_x INT DEFAULT NULL,
    action_location_y INT DEFAULT NULL,
    
    ── Decision context ──────────────────────────────────────
    -- The utility scores that led to this action being chosen
    utility_score   DECIMAL(6,4) NOT NULL,              -- final utility of chosen action
    alternative_actions JSON DEFAULT NULL,              -- other actions considered with their scores
    
    ── Outcome ───────────────────────────────────────────────
    success         BOOLEAN NOT NULL DEFAULT TRUE,
    outcome_detail  TEXT DEFAULT NULL,                   -- narrative description
    outcome_effects JSON DEFAULT NULL,                   -- structured effect deltas
    
    ── Timestamp ─────────────────────────────────────────────
    executed_at     DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (snapshot_id) REFERENCES character_state_snapshots(id) ON DELETE SET NULL,
    INDEX idx_actions_char (character_id),
    INDEX idx_actions_day (simulation_day),
    INDEX idx_actions_type (action_type),
    INDEX idx_actions_char_day (character_id, simulation_day),
    INDEX idx_actions_utility (utility_score),
    INDEX idx_actions_success (success),
    INDEX idx_actions_executed (executed_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 6. USER FEEDBACK ───────────────────────────────────────────
-- Structured human feedback on character behavior.
-- CRITICAL for RLHF: provides the human reward signal.

CREATE TABLE user_feedback (
    id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    user_id         INT UNSIGNED NOT NULL,
    character_id    INT UNSIGNED NOT NULL,
    simulation_day  INT UNSIGNED NOT NULL,              -- which day this feedback refers to
    
    ── Alignment rating (how well the character matched user expectation) ──
    alignment_score TINYINT UNSIGNED NOT NULL,          -- 1-5: 1=completely wrong, 5=perfect
    
    ── Character state ratings (user's perception) ────────────
    perceived_happiness     TINYINT UNSIGNED DEFAULT NULL,  -- 1-10
    perceived_health        TINYINT UNSIGNED DEFAULT NULL,  -- 1-10
    perceived_stress        TINYINT UNSIGNED DEFAULT NULL,  -- 1-10
    
    ── Corrections (structured training signals) ────────────
    -- User says "my character wouldn't do that" — that's a correction signal
    would_change_action     TEXT DEFAULT NULL,           -- what would you have done differently?
    would_change_personality TEXT DEFAULT NULL,          -- would you adjust any personality scores?
    personality_corrections JSON DEFAULT NULL,          -- structured: {"neuroticism": -0.1}
    
    ── Free response ──────────────────────────────────────────
    narrative_feedback      TEXT DEFAULT NULL,           -- free-form story feedback
    notable_events          TEXT DEFAULT NULL,           -- what stood out to the user
    
    ── Engagement signal ──────────────────────────────────────
    time_spent_seconds      INT UNSIGNED DEFAULT NULL,  -- how long user spent reviewing
    actions_reviewed        SMALLINT UNSIGNED DEFAULT NULL, -- how many actions they looked at
    click_hesitation_count  SMALLINT UNSIGNED DEFAULT NULL, -- clicks before submitting
    
    ── Timestamps ─────────────────────────────────────────────
    created_at              DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    INDEX idx_feedback_user (user_id),
    INDEX idx_feedback_char (character_id),
    INDEX idx_feedback_day (simulation_day),
    INDEX idx_feedback_alignment (alignment_score),
    INDEX idx_feedback_char_day (character_id, simulation_day),
    UNIQUE KEY uk_feedback_char_day (character_id, simulation_day),
    INDEX idx_feedback_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 7. SOCIAL INTERACTIONS ─────────────────────────────────────
-- Track every interaction between two characters.
-- Enables social network analysis and relationship dynamics modeling.

CREATE TABLE social_interactions (
    id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    simulation_day  INT UNSIGNED NOT NULL,
    
    actor_char_id   INT UNSIGNED NOT NULL,              -- who initiated
    target_char_id  INT UNSIGNED NOT NULL,              -- who received
    
    interaction_type VARCHAR(60) NOT NULL,              -- 'share_food', 'bond', 'conflict', 'trade', 'teach'
    intensity       DECIMAL(4,3) NOT NULL,              -- 0.0 – 1.0 how strong the interaction was
    
    ── Context ───────────────────────────────────────────────
    actor_mood_before   JSON DEFAULT NULL,              -- actor's mood state before
    target_mood_before  JSON DEFAULT NULL,              -- target's mood state before
    
    ── Outcome ───────────────────────────────────────────────
    trust_delta     DECIMAL(4,3) DEFAULT NULL,          -- change in trust (actor→target)
    hostility_delta DECIMAL(4,3) DEFAULT NULL,          -- change in hostility
    knowledge_transferred JSON DEFAULT NULL,            -- what knowledge was shared
    
    ── Timestamp ─────────────────────────────────────────────
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (actor_char_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (target_char_id) REFERENCES characters(id) ON DELETE CASCADE,
    INDEX idx_interactions_actor (actor_char_id),
    INDEX idx_interactions_target (target_char_id),
    INDEX idx_interactions_day (simulation_day),
    INDEX idx_interactions_type (interaction_type),
    INDEX idx_interactions_both (actor_char_id, target_char_id),
    INDEX idx_interactions_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 8. RELATIONSHIPS ───────────────────────────────────────────
-- Persistent dyadic relationships between characters.
-- Updated as trust/hostility evolves through interactions.

CREATE TABLE relationships (
    id                  BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_a_id      INT UNSIGNED NOT NULL,
    character_b_id      INT UNSIGNED NOT NULL,
    
    ── Relationship state ─────────────────────────────────────
    trust_a_to_b        DECIMAL(4,3) NOT NULL DEFAULT 0.000,  -- what A feels about B
    trust_b_to_a        DECIMAL(4,3) NOT NULL DEFAULT 0.000,  -- what B feels about A
    hostility_a_to_b    DECIMAL(4,3) NOT NULL DEFAULT 0.000,
    hostility_b_to_a    DECIMAL(4,3) NOT NULL DEFAULT 0.000,
    
    ── Bond type ──────────────────────────────────────────────
    bond_type           ENUM('none','acquaintance','friend','close_friend','pair_bond','rival','enemy')
                        NOT NULL DEFAULT 'none',
    bond_strength       DECIMAL(4,3) NOT NULL DEFAULT 0.000,
    
    ── Interaction count ─────────────────────────────────────
    total_interactions  INT UNSIGNED NOT NULL DEFAULT 0,
    last_interaction_day INT UNSIGNED DEFAULT NULL,
    last_interaction_type VARCHAR(60) DEFAULT NULL,
    
    ── Timestamps ─────────────────────────────────────────────
    created_at          DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at          DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    
    FOREIGN KEY (character_a_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (character_b_id) REFERENCES characters(id) ON DELETE CASCADE,
    INDEX idx_relationships_a (character_a_id),
    INDEX idx_relationships_b (character_b_id),
    INDEX idx_relationships_bond (bond_type),
    INDEX idx_relationships_trust (trust_a_to_b),
    UNIQUE KEY uk_relationship_pair (character_a_id, character_b_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 9. CHARACTER INVENTORY / ITEMS ─────────────────────────────
-- What each character carries and has crafted.

CREATE TABLE character_items (
    id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id    INT UNSIGNED NOT NULL,
    simulation_day  INT UNSIGNED NOT NULL,              -- when acquired
    
    item_name       VARCHAR(120) NOT NULL,
    item_type       VARCHAR(60) NOT NULL,               -- 'tool', 'food', 'weapon', 'material'
    item_tags       JSON DEFAULT NULL,                  -- ['cutting', 'sharp', 'stone']
    
    quantity        INT UNSIGNED NOT NULL DEFAULT 1,
    durability      DECIMAL(5,2) DEFAULT NULL,          -- 0.00 – 100.00
    is_equipped     BOOLEAN NOT NULL DEFAULT FALSE,
    
    ── Provenance ─────────────────────────────────────────────
    was_invented    BOOLEAN NOT NULL DEFAULT FALSE,     -- true if character invented this
    crafted_from    JSON DEFAULT NULL,                  -- what items were combined
    
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    INDEX idx_items_char (character_id),
    INDEX idx_items_type (item_type),
    INDEX idx_items_invented (was_invented)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 10. INVENTIONS / DISCOVERIES ──────────────────────────────
-- Track every new invention or discovery made by any character.
-- Enables technology progression modeling.

CREATE TABLE inventions (
    id              INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    inventor_char_id INT UNSIGNED NOT NULL,
    simulation_day  INT UNSIGNED NOT NULL,
    
    invention_name  VARCHAR(120) NOT NULL,
    invention_type  VARCHAR(60) NOT NULL,
    tags            JSON DEFAULT NULL,
    description     TEXT DEFAULT NULL,
    
    ── Spread tracking ────────────────────────────────────────
    spread_count    INT UNSIGNED NOT NULL DEFAULT 1,    -- how many characters know it
    spread_mechanism ENUM('independent','taught','diffused','inherited')
                    NOT NULL DEFAULT 'independent',
    
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (inventor_char_id) REFERENCES characters(id) ON DELETE CASCADE,
    INDEX idx_inventions_inventor (inventor_char_id),
    INDEX idx_inventions_type (invention_type),
    INDEX idx_inventions_spread (spread_count)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 11. KNOWLEDGE / RECIPE DIFFUSION ──────────────────────────
-- Track how knowledge spreads between characters.
-- Enables cultural transmission modeling.

CREATE TABLE knowledge_diffusion (
    id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    invention_id    INT UNSIGNED NOT NULL,
    learner_char_id INT UNSIGNED NOT NULL,
    teacher_char_id INT UNSIGNED DEFAULT NULL,          -- NULL = self-discovered
    simulation_day  INT UNSIGNED NOT NULL,
    
    diffusion_method ENUM('taught','observed','traded','inherited','self_discovered')
                    NOT NULL,
    proficiency     DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (invention_id) REFERENCES inventions(id) ON DELETE CASCADE,
    FOREIGN KEY (learner_char_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (teacher_char_id) REFERENCES characters(id) ON DELETE SET NULL,
    INDEX idx_diffusion_invention (invention_id),
    INDEX idx_diffusion_learner (learner_char_id),
    INDEX idx_diffusion_method (diffusion_method)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 12. TRIBES / GROUPS ───────────────────────────────────────
-- Emergent social groups.

CREATE TABLE tribes (
    id              INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name            VARCHAR(120) NOT NULL,
    formation_day   INT UNSIGNED NOT NULL,
    dissolution_day INT UNSIGNED DEFAULT NULL,
    
    ── Territory ──────────────────────────────────────────────
    territory_center_x  INT DEFAULT NULL,
    territory_center_y  INT DEFAULT NULL,
    territory_radius    INT DEFAULT NULL,
    
    ── Belief system (emergent) ──────────────────────────────
    belief_system       VARCHAR(255) DEFAULT NULL,
    cultural_values     JSON DEFAULT NULL,              -- emerged norms/taboos
    
    ── Demographics ──────────────────────────────────────────
    max_members         INT UNSIGNED DEFAULT NULL,
    total_members_ever  INT UNSIGNED NOT NULL DEFAULT 0,
    
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    dissolved_at    DATETIME DEFAULT NULL,
    
    INDEX idx_tribes_formed (formation_day),
    INDEX idx_tribes_active (dissolution_day)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 13. TRIBE MEMBERSHIP ───────────────────────────────────────
-- Character ↔ Tribe affiliation timeline.

CREATE TABLE tribe_memberships (
    id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id    INT UNSIGNED NOT NULL,
    tribe_id        INT UNSIGNED NOT NULL,
    joined_day      INT UNSIGNED NOT NULL,
    left_day        INT UNSIGNED DEFAULT NULL,
    left_reason     VARCHAR(120) DEFAULT NULL,          -- 'migration', 'expelled', 'dissolved'
    
    role            VARCHAR(60) DEFAULT 'member',       -- 'leader', 'elder', 'member'
    status          DECIMAL(4,3) DEFAULT NULL,          -- standing within tribe (0-1)
    
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (tribe_id) REFERENCES tribes(id) ON DELETE CASCADE,
    INDEX idx_membership_char (character_id),
    INDEX idx_membership_tribe (tribe_id),
    INDEX idx_membership_active (left_day)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 14. WORLD EVENTS ───────────────────────────────────────────
-- Macro-level simulation events.
-- Enables narrative generation and causality analysis.

CREATE TABLE world_events (
    id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    simulation_day  INT UNSIGNED NOT NULL,
    
    event_type      VARCHAR(60) NOT NULL,               -- 'war', 'famine', 'discovery', 'migration', 'extinction'
    severity        ENUM('minor','notable','critical','catastrophic') NOT NULL DEFAULT 'notable',
    description     TEXT NOT NULL,
    
    ── Affected entities ──────────────────────────────────────
    primary_tribe_id    INT UNSIGNED DEFAULT NULL,
    secondary_tribe_id  INT UNSIGNED DEFAULT NULL,
    primary_char_id     INT UNSIGNED DEFAULT NULL,
    secondary_char_id   INT UNSIGNED DEFAULT NULL,
    
    ── Impact data (structured) ───────────────────────────────
    casualties      INT UNSIGNED DEFAULT NULL,
    inventions_spawned INT UNSIGNED DEFAULT NULL,
    territory_shift JSON DEFAULT NULL,
    economic_impact DECIMAL(10,2) DEFAULT NULL,
    
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    INDEX idx_events_day (simulation_day),
    INDEX idx_events_type (event_type),
    INDEX idx_events_severity (severity),
    INDEX idx_events_tribe (primary_tribe_id),
    INDEX idx_events_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 15. SIMULATION CONFIGURATIONS ──────────────────────────────
-- Each simulation run has a configuration for reproducibility.

CREATE TABLE simulation_configs (
    id              INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    seed            INT UNSIGNED NOT NULL,              -- deterministic seed
    name            VARCHAR(120) DEFAULT NULL,
    description     TEXT DEFAULT NULL,
    
    ── Parameters ─────────────────────────────────────────────
    world_width     INT NOT NULL DEFAULT 512,
    world_height    INT NOT NULL DEFAULT 512,
    resource_density DECIMAL(4,3) NOT NULL DEFAULT 0.500,
    aggression_multiplier DECIMAL(4,3) NOT NULL DEFAULT 1.000,
    mutation_rate   DECIMAL(4,3) NOT NULL DEFAULT 0.050,
    max_characters  INT UNSIGNED NOT NULL DEFAULT 100,
    
    ── Timestamps ─────────────────────────────────────────────
    started_at      DATETIME DEFAULT NULL,
    ended_at        DATETIME DEFAULT NULL,
    tick_count      BIGINT UNSIGNED DEFAULT 0,
    is_running      BOOLEAN NOT NULL DEFAULT FALSE,
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    INDEX idx_configs_seed (seed),
    INDEX idx_configs_running (is_running)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 16. CHARACTER GENOME (Future: NEAT / Genetic Algorithm) ───
-- Heritable traits for evolutionary simulation.

CREATE TABLE character_genomes (
    id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    character_id    INT UNSIGNED NOT NULL UNIQUE,
    parent_genome_id BIGINT UNSIGNED DEFAULT NULL,      -- lineage tracking
    
    ── Traits (0.0 – 2.0 normalized) ─────────────────────────
    speed           DECIMAL(4,3) NOT NULL DEFAULT 1.000,
    sight_range     DECIMAL(4,3) NOT NULL DEFAULT 1.000,
    metabolism      DECIMAL(4,3) NOT NULL DEFAULT 1.000,
    fertility       DECIMAL(4,3) NOT NULL DEFAULT 1.000,
    resilience      DECIMAL(4,3) NOT NULL DEFAULT 1.000,
    
    ── NEAT neural network (JSON-encoded topology) ───────────
    neat_genome     JSON DEFAULT NULL,
    generation      INT UNSIGNED NOT NULL DEFAULT 0,    -- how many mutations from baseline
    
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (character_id) REFERENCES characters(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_genome_id) REFERENCES character_genomes(id) ON DELETE SET NULL,
    INDEX idx_genomes_char (character_id),
    INDEX idx_genomes_generation (generation),
    INDEX idx_genomes_traits (speed, sight_range, metabolism, fertility, resilience)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ── 17. USER SESSION LOGS ──────────────────────────────────────
-- Track user behavior on the platform for UX optimization.

CREATE TABLE user_session_logs (
    id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    user_id         INT UNSIGNED NOT NULL,
    
    ip_address      VARBINARY(16) DEFAULT NULL,         -- IPv4 or IPv6
    user_agent      VARCHAR(500) DEFAULT NULL,
    
    session_start   DATETIME NOT NULL,
    session_end     DATETIME DEFAULT NULL,
    session_duration_seconds INT UNSIGNED DEFAULT NULL,
    
    pages_viewed    JSON DEFAULT NULL,                  -- which pages and timestamps
    actions_taken   JSON DEFAULT NULL,                  -- what the user did
    
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_logs_user (user_id),
    INDEX idx_logs_start (session_start),
    INDEX idx_logs_duration (session_duration_seconds)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ═══════════════════════════════════════════════════════════════════
-- INSERT STATEMENTS: Default data
-- ═══════════════════════════════════════════════════════════════════

-- Default simulation config for first run
INSERT INTO simulation_configs (seed, name, description, world_width, world_height, resource_density)
VALUES (0x4F1A, 'Genesis', 'First simulation run - baseline parameters', 512, 512, 0.550);
