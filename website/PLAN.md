# ASHB2: Human-in-the-Loop Simulation — Implementation Plan

## I. Concept Overview

A web application where real humans create digital "characters" that represent themselves
and release them into an autonomous simulation world. Characters act based on their
owner's psychological profile. The owner can observe, give feedback, and watch their
character live out a simulated life.

**Time Scale:** 1 human day = 4 simulation days (characters age faster, allowing
meaningful life arcs to unfold in weeks, not years).

---

## II. System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   Web Browser (User)                      │
├─────────────────────────────────────────────────────────┤
│              PHP Backend (no framework)                   │
├─────────────────────────────────────────────────────────┤
│               MySQL / MariaDB Database                    │
├─────────────────────────────────────────────────────────┤
│           Future: Simulation Engine (C++/Python)          │
└─────────────────────────────────────────────────────────┘
```

### Layer 1: Database (MySQL/MariaDB)
- Relational schema designed for maximum data collection
- Every decision, state change, interaction, and user reaction is logged
- Time-series tables enable trajectory analysis for ML training

### Layer 2: PHP Backend
- Authentication system (register, login, password reset, session management)
- Character management (create, view, update feedback)
- Dashboard (view character state, timeline, telemetry)
- API endpoints (future: for simulation engine to push/pull data)

### Layer 3: Simulation Engine (Future)
- Headless process that reads character profiles from DB
- Runs GOAP planner, personality simulation, environmental model
- Writes tick-by-tick state changes back to DB
- 4 ticks per human day

### Layer 4: Frontend
- Static HTML/CSS with the Scientific Terminal aesthetic
- JS for live-updating dashboards
- Responsive: desktop-first with mobile support

---

## III. Database Design Philosophy (AI Training Maximization)

Every table is designed with three principles:

1. **Temporal granularity** — State snapshots are append-only. We never overwrite
   a character's previous state. This creates a complete life trajectory usable
   for sequence modeling.

2. **Context capture** — Every action log includes the character's full state vector
   at the moment of decision (health, hunger, social bonds, personality, position).
   This enables supervised learning: given state → predict action.

3. **Human signal** — User feedback is stored as a structured signal. When a user
   says "my character wouldn't do that" or rates their character's day, this is
   valuable RLHF (Reinforcement Learning from Human Feedback) training data.

### Data Categories for ML Training

| Category | ML Use Case |
|----------|------------|
| Personality × Decision | Predict behavior from psychometric profile |
| State → Action | Imitation learning / behavioral cloning |
| Social bond trajectories | Graph-based social dynamics modeling |
| User corrections | RLHF reward model training |
| Survival prediction | Fitness function learning |
| Cultural transmission | Knowledge diffusion modeling |
| Time-series states | Anomaly detection, trajectory clustering |

---

## IV. Page Structure (Phase 1)

| Route | Page | Auth Required | Purpose |
|-------|------|--------------|---------|
| `/` | Landing | No | Pitch + login/register CTA |
| `/login` | Login | No | Authenticate user |
| `/register` | Register | No | Create account + first character |
| `/dashboard` | Dashboard | Yes | Character overview, timeline, feedback |
| `/logout` | Logout | Yes | End session |

### Future Pages (Phase 2+)

| Route | Page | Purpose |
|-------|------|---------|
| `/character/{id}` | Character Profile | Detailed view of a specific character |
| `/world` | World Map | Live simulation map with all characters |
| `/chronicle` | Chronicle Browser | Browse all character histories |
| `/admin` | Admin Panel | System control, user management |
| `/api/*` | REST API | Simulation engine data exchange |

---

## V. Character System (The Core Loop)

### Character Creation
User fills out a clinical questionnaire:
- Big Five personality (openness, conscientiousness, extraversion, agreeableness, neuroticism)
- Attachment style (secure, anxious, avoidant, disorganized)
- Core drives (exploration, social, safety, dominance)
- Memory parameters (decay rate, trauma retention)
- Optional: initial knowledge (starting recipes/tools)

### Simulation Loop (Future Engine)
```
Every 6 hours (1 simulation day):
  1. Engine loads all active characters from DB
  2. For each character:
     a. Recalculate drives based on current state
     b. Score available actions via utility function
     c. Execute highest-scoring action
     d. Update character state
     e. Log action + new state
  3. Process social interactions between nearby characters
  4. Update world environmental state
  5. Check for births, deaths, discoveries
```

### User Feedback Loop
```
Every 24 hours (1 human day = 4 simulation days):
  1. User receives notification: "Day X is complete"
  2. User views dashboard: timeline, state changes, key events
  3. User provides feedback:
     a. How aligned was your character's behavior? (1-5)
     b. What would you have done differently? (free text)
     c. Rate your character's current emotional state (1-10)
     d. Any corrections to the personality profile?
```

---

## VI. Implementation Roadmap

### Phase 1 (Current) — Foundation
- [x] Database schema (maximal data collection)
- [x] User registration + login system
- [x] Session management
- [ ] Dashboard with character status
- [ ] Password reset flow

### Phase 2 — Character Management
- [ ] Extended character creation questionnaire
- [ ] Character profile page
- [ ] Feedback form submission
- [ ] Character timeline visualization

### Phase 3 — Simulation Engine
- [ ] Headless simulation engine (C++/Python)
- [ ] Engine ↔ DB synchronization
- [ ] Tick processing: 4 ticks/day
- [ ] Environmental simulation

### Phase 4 — User Experience
- [ ] Live-updating dashboard (WebSockets/SSE)
- [ ] Chronicle browser (search character histories)
- [ ] World map visualization
- [ ] Notifications (email, in-app)

### Phase 5 — AI Training
- [ ] Export pipeline for ML datasets
- [ ] Personality → behavior model training
- [ ] RLHF from user feedback
- [ ] Iterative model improvement

---

## VII. File Structure

```
human-sim/
├── PLAN.md                        # This document
├── sql/
│   └── schema.sql                 # Complete database schema
├── config.php                     # Database + app config
├── db.php                         # PDO connection singleton
├── index.php                      # Landing page (login/register CTA)
├── login.php                      # Login form + authentication
├── register.php                   # Registration form + character creation
├── dashboard.php                  # Protected user dashboard
├── logout.php                     # Session destruction
├── reset-password.php             # Password reset flow
└── style.css                      # Scientific Terminal theme
```

---

## VIII. Key Design Decisions

1. **No framework** — Vanilla PHP keeps the stack simple and avoids dependency
   bloat. Future migration to Laravel/Symfony is possible if needed.

2. **Prepared statements everywhere** — All SQL uses PDO prepared statements.
   No raw query concatenation.

3. **Password hashing** — PHP's `password_hash()` with `PASSWORD_BCRYPT`.
   No plaintext passwords ever.

4. **Session security** — HttpOnly + SameSite cookies, session regeneration
   on login, configurable session lifetime.

5. **Time-series over overwrites** — Character states are append-only.
   This creates a complete trajectory for ML training.

6. **Psychometric richness** — The character table captures full Big Five
   + attachment + drives + memory parameters, enabling rich personality
   → behavior modeling.

7. **Feedback as signal** — User ratings and corrections are stored as
   structured ML training data, not just free text.
