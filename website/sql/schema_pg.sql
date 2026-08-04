-- ============================================================================
-- ASHB2 — PostgreSQL schema (entity-centric)
--
-- Scope: one table for an entity's own attributes, and one table per "pointed"
-- association it carries (desire / anger / social / couple / mental model).
-- Nothing else. Tribes, religions, markets and the civ log are deliberately
-- out of scope here — add them as separate files when they're needed.
--
-- Loaded by  scripts/db_spool_loader.py  from the NDJSON chunks the engine
-- drops in bridge/spool/ (see src/DbExport.cpp).
--
-- Apply with:  psql -d ashb2 -f website/sql/schema_pg.sql
-- ============================================================================

CREATE SCHEMA IF NOT EXISTS sim;

-- ─── world ──────────────────────────────────────────────────────────────────
-- One row per simulated world. Everything below hangs off this, so retiring a
-- world is a single DELETE.

CREATE TABLE IF NOT EXISTS sim.world (
    world_id      integer     PRIMARY KEY,
    label         text,
    seed          text,
    last_day      integer     NOT NULL DEFAULT 0,   -- highest civ-day ingested
    started_at    timestamptz NOT NULL DEFAULT now(),
    last_push_at  timestamptz
);

COMMENT ON TABLE  sim.world IS 'One simulated world; parent of every other sim table.';
COMMENT ON COLUMN sim.world.last_day IS 'Highest civ-day the loader has ingested for this world.';


-- ─── entity ─────────────────────────────────────────────────────────────────
-- An entity's PURE attributes: scalars it owns outright, plus the flat structs
-- it embeds (Personality, ValueSystem, SelfConcept, DevelopmentalHistory,
-- EmotionalState, NarrativeIdentity, Creed, BiologicalState).
--
-- This is CURRENT state, not history: the loader upserts each row in place, so
-- one entity is always exactly one row.
--
-- Death arrives as absence. The engine's per-tick batch erase drops everything
-- with health <= 0 before the export runs, so a person who stops being pushed
-- has died; the loader flips them to alive = false rather than deleting the
-- row, and lineage, kinship and their last relationships all outlive them.
-- `last_seen_day` is when the row was last refreshed, and for the dead it is
-- therefore the day they died.
--
-- Sentinels are normalised to NULL by the exporter: -1 ids, -2 / -99999 "never"
-- markers and empty strings all arrive as NULL.



CREATE TABLE IF NOT EXISTS sim.entity (
    world_id                     integer NOT NULL REFERENCES sim.world(world_id) ON DELETE CASCADE,
    sim_id                       integer NOT NULL,   -- Entity::entityId
    last_seen_day                integer NOT NULL,
    alive                        boolean NOT NULL DEFAULT true,

    -- identity
    name                         text,
    sex                          char(1),
    birth_day                    smallint,           -- day of year 0-364
    birth_year                   integer,            -- negative = BC
    web_char_id                  integer,            -- web bridge char, NULL = pure AI

    -- body / life course
    age                          real,
    life_stage                   smallint,           -- 0 infant 1 child 2 adolescent 3 adult 4 elder
    health                       real,
    hunger                       real,
    food_store                   real,
    hygiene                      real,
    fatigue_level                real,
    injury_level                 real,
    sleep_pressure               real,
    sleep_quality                real,
    antibody                     integer,
    disease_type                 integer,            -- NULL = healthy
    base_immunity                real,

    -- affect / mind
    happiness                    real,
    stress                       real,
    stress_baseline              real,
    mental_health                real,
    loneliness                   real,
    boredom                      real,
    general_anger                real,
    esteem                       real,
    sense_of_purpose             real,
    grief_intensity              real,
    social_deficit               real,
    days_without_social_action   integer,
    meeting_count                integer,

    -- QI: realized ability, the inherited ceiling, and the schooling in between
    qi                           real,
    qi_potential                 real,
    school_years                 real,

    -- Personality (Big Five)
    openness                     real,
    conscientiousness            real,
    extraversion                 real,
    agreeableness                real,
    neuroticism                  real,

    -- ValueSystem
    value_family                 real,
    value_achievement            real,
    value_spiritual              real,
    value_hedonism               real,
    value_collectivism           real,

    -- SelfConcept
    self_perceived_extraversion  real,
    self_perceived_agreeableness real,
    self_perceived_neuroticism   real,
    self_primary_identity        text,
    self_esteem                  real,
    self_efficacy                real,
    self_calibration             real,

    -- DevelopmentalHistory
    had_secure_attachment        boolean,
    childhood_trauma_score       real,
    childhood_nurturing_score    real,
    attachment_style             smallint,           -- 0 secure 1 anxious 2 avoidant 3 disorganized

    -- EmotionalState
    raw_anger                    real,
    expressed_anger              real,
    suppression_debt             real,

    -- NarrativeIdentity
    self_story                   text,
    dominant_value               text,
    narrative_coherence          real,

    -- Creed (cached doctrine of the faith actually held)
    creed_held                   boolean,
    creed_militarism             real,
    creed_tolerance              real,
    creed_asceticism             real,
    creed_authority              real,
    creed_afterlife_focus        real,
    creed_devotion               real,

    -- BiologicalState
    nutritional_status           real,
    hydration_level              real,
    energy_level                 real,

    -- kinship (ID-based; the durable source of truth for lineage)
    parent1_id                   integer,
    parent2_id                   integer,
    family_id                    integer,
    lineage_depth                integer,

    -- society / geography
    tribe_id                     integer,
    religion_id                  integer,
    origin_region_id             integer,
    dominance_rank               real,
    social_class                 smallint,           -- 0 slave 1 plebeian 2 patrician
    auctoritas                   real,
    integrity                    real,
    specialization               text,
    is_specialist                boolean,
    role_since_day               integer,
    cultural_capital             real,
    culture_traits               bit(64),            -- unsigned long long, verbatim (PG has no uint64)
    committed_traits             bit(64),
    home_attachment              real,

    -- economy / space
    wealth                       real,               -- Economic::token
    pos_x                        real,
    pos_y                        real,

    -- surface state (what the dashboard shows)
    current_goal                 text,
    last_action                  text,
    inner_monologue              text,

    PRIMARY KEY (world_id, sim_id)
);



CREATE TABLE IF NOT EXISTS sim.users (
    user_id                     integer NOT NULL,
    world_id                    integer NOT NULL DEFAULT 1,
    user_entity_id              integer NOT NULL,
    login                       text NOT NULL,
    password                    text NOT NULL,

    PRIMARY KEY (user_id, world_id),
    FOREIGN KEY (world_id, user_entity_id)
        REFERENCES sim.entity(world_id, sim_id) ON DELETE CASCADE
);

CREATE TYPE BuildingType AS ENUM ('MEDECINE', 'ECONOMY_AND_PRODUCTION', 'HOUSING_AND_CIVILIAN', 'MILITARY', 'POLITICAL');

CREATE TABLE IF NOT EXISTS sim.building (
    building_id                 integer NOT NULL,
    building_category           BuildingType NOT NULL,
    building_name               text NOT NULL,
    base_health                 integer NOT NULL,
    base_price                  integer NOT NULL,
    base_maintenance            integer NOT NULL,
    trust_level                 integer NOT NULL,
);


CREATE TABLE IF NOT EXISTS Tribe(
    current_tribe_id            integer NOT NULL,
    tribe_name                  text NOT NULL,
    leader_id                   integer NOT NULL,

);

CREATE INDEX IF NOT EXISTS entity_alive_idx    ON sim.entity (world_id) WHERE alive;
CREATE INDEX IF NOT EXISTS entity_tribe_idx    ON sim.entity (world_id, tribe_id);
CREATE INDEX IF NOT EXISTS entity_web_char_idx ON sim.entity (web_char_id) WHERE web_char_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS entity_parent1_idx  ON sim.entity (world_id, parent1_id);
CREATE INDEX IF NOT EXISTS entity_parent2_idx  ON sim.entity (world_id, parent2_id);

COMMENT ON TABLE  sim.entity IS 'Current state of one entity: its own scalar attributes and embedded flat structs.';
COMMENT ON COLUMN sim.entity.alive IS 'Set false by the loader when the engine stops emitting this entity, which is how death arrives. Rows are never deleted.';
COMMENT ON COLUMN sim.entity.culture_traits IS 'One bit per trait in the world catalogue; stored as bit(64) because PostgreSQL has no unsigned bigint.';


-- ─── pointed associations ───────────────────────────────────────────────────
-- The directed edges an entity carries toward other entities. All five share a
-- shape: (owner, target) is the key, `last_seen_day` marks the push that
-- refreshed it, and the loader deletes anything left behind — which is how a
-- broken couple or a forgotten mental model disappears without the engine
-- having to emit a tombstone.
--
-- NOTE on the FKs: only the OWNER side is enforced. The target is indexed but
-- not constrained, because a runtime pointer can name an entity that predates
-- this database (association lists are rebuilt from pointers on load, and the
-- older ones may never have been pushed). Enforcing it would let one dangling
-- edge abort an otherwise-good chunk.

-- desire ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS sim.entity_desire (
    world_id      integer NOT NULL,
    from_id       integer NOT NULL,
    to_id         integer NOT NULL,
    desire        real    NOT NULL,
    last_seen_day integer NOT NULL,
    PRIMARY KEY (world_id, from_id, to_id),
    FOREIGN KEY (world_id, from_id) REFERENCES sim.entity(world_id, sim_id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS entity_desire_to_idx ON sim.entity_desire (world_id, to_id);

-- anger ----------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS sim.entity_anger (
    world_id      integer NOT NULL,
    from_id       integer NOT NULL,
    to_id         integer NOT NULL,
    anger         real    NOT NULL,
    last_seen_day integer NOT NULL,
    PRIMARY KEY (world_id, from_id, to_id),
    FOREIGN KEY (world_id, from_id) REFERENCES sim.entity(world_id, sim_id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS entity_anger_to_idx ON sim.entity_anger (world_id, to_id);

-- social ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS sim.entity_social (
    world_id      integer NOT NULL,
    from_id       integer NOT NULL,
    to_id         integer NOT NULL,
    social        real    NOT NULL,
    last_seen_day integer NOT NULL,
    PRIMARY KEY (world_id, from_id, to_id),
    FOREIGN KEY (world_id, from_id) REFERENCES sim.entity(world_id, sim_id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS entity_social_to_idx ON sim.entity_social (world_id, to_id);

-- couple ---------------------------------------------------------------------
-- Bond state is runtime-only in the engine (not serialized), so a world resumed
-- from a save re-enters with these at their defaults. That's engine behaviour,
-- mirrored faithfully here rather than papered over.
CREATE TABLE IF NOT EXISTS sim.entity_couple (
    world_id      integer NOT NULL,
    from_id       integer NOT NULL,
    to_id         integer NOT NULL,
    commitment    real,
    satisfaction  real,
    trust         real,
    suspicion     real,
    days_together integer,
    last_seen_day integer NOT NULL,
    PRIMARY KEY (world_id, from_id, to_id),
    FOREIGN KEY (world_id, from_id) REFERENCES sim.entity(world_id, sim_id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS entity_couple_to_idx ON sim.entity_couple (world_id, to_id);



-- mental model ---------------------------------------------------------------
-- Theory of mind: how the owner believes the target is, which is routinely
-- WRONG and is supposed to be. Runtime-only in the engine, same caveat as above.
CREATE TABLE IF NOT EXISTS sim.entity_mental_model (
    world_id                 integer NOT NULL,
    from_id                  integer NOT NULL,
    to_id                    integer NOT NULL,
    perceived_extraversion   real,
    perceived_agreeableness  real,
    perceived_neuroticism    real,
    estimated_happiness      real,
    estimated_anger          real,
    estimated_stress         real,
    trust_level              real,
    predictability           real,
    perceived_intentionality real,
    confidence               real,               -- 0..1, decays with time since last look
    last_observed_day        integer,
    last_interaction_day     real,
    last_interaction_outcome text,
    faked_by_them            boolean,            -- trust inflated by the target faking warmth
    fake_trust_gain          real,
    last_seen_day            integer NOT NULL,
    PRIMARY KEY (world_id, from_id, to_id),
    FOREIGN KEY (world_id, from_id) REFERENCES sim.entity(world_id, sim_id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS entity_mm_to_idx ON sim.entity_mental_model (world_id, to_id);


-- ─── convenience view ───────────────────────────────────────────────────────
-- Every edge an entity points at, with the target's name resolved. One row per
-- (entity, other) pair; FULL OUTER between the four edge kinds so a bond that
-- exists as anger-only still shows up.



CREATE OR REPLACE VIEW sim.v_entity_edges AS
WITH edges AS (
    SELECT world_id, from_id, to_id FROM sim.entity_desire
    UNION
    SELECT world_id, from_id, to_id FROM sim.entity_anger
    UNION
    SELECT world_id, from_id, to_id FROM sim.entity_social
    UNION
    SELECT world_id, from_id, to_id FROM sim.entity_couple
    UNION
    SELECT world_id, from_id, to_id FROM sim.entity_mental_model
)
SELECT e.world_id,
       e.from_id,
       me.name                 AS from_name,
       e.to_id,
       other.name              AS to_name,
       other.alive             AS to_alive,
       d.desire,
       a.anger,
       s.social,
       (c.to_id IS NOT NULL)   AS is_partner,
       c.commitment,
       c.trust                 AS couple_trust,
       c.suspicion,
       c.days_together,
       m.trust_level           AS model_trust,
       m.confidence            AS model_confidence
FROM       edges                   e
JOIN       sim.entity              me    ON (me.world_id, me.sim_id)    = (e.world_id, e.from_id)
LEFT JOIN  sim.entity              other ON (other.world_id, other.sim_id) = (e.world_id, e.to_id)
LEFT JOIN  sim.entity_desire       d ON (d.world_id, d.from_id, d.to_id) = (e.world_id, e.from_id, e.to_id)
LEFT JOIN  sim.entity_anger        a ON (a.world_id, a.from_id, a.to_id) = (e.world_id, e.from_id, e.to_id)
LEFT JOIN  sim.entity_social       s ON (s.world_id, s.from_id, s.to_id) = (e.world_id, e.from_id, e.to_id)
LEFT JOIN  sim.entity_couple       c ON (c.world_id, c.from_id, c.to_id) = (e.world_id, e.from_id, e.to_id)
LEFT JOIN  sim.entity_mental_model m ON (m.world_id, m.from_id, m.to_id) = (e.world_id, e.from_id, e.to_id);
