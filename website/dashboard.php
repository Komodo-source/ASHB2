<?php
/**
 * ASHB2: Dashboard
 * 
 * Protected page. Shows the user's characters, their current status,
 * recent actions, and a feedback form. Authenticated access only.
 */

require_once __DIR__ . '/auth.php';
$user = require_auth();

// The user's character — their assigned entity. Always one row on this schema
// (sim.users binds one account to one entity), but the page's roster loop is
// left intact; see get_user_characters().
$characters = get_user_characters($user['id'], $user['world_id']);

// The world's own numbers, for the banner. Nothing here depends on it, so a
// null just means the world row has gone.
$world = get_world_summary($user['world_id']);

// Feedback capture is not wired up on this schema.
//
// It needs a `user_feedback` table, and sql/schema_pg.sql defines none — the
// only table here the web app may write is sim.users, because every other one
// is replaced wholesale by the loader on the next push (see SimDb::execute).
// The form has been removed rather than left accepting input it would drop:
// a page that thanks you for feedback it discarded is worse than one that says
// it cannot take any.
$feedbackMessage = '';

// Get latest state for the active character (first alive one)
$activeChar = null;
$latestState = null;
$recentActions = [];

foreach ($characters as $char) {
    if ($char['is_alive'] && $char['is_active']) {
        $activeChar = $char;
        $latestState = get_latest_character_state($char['id'], $user['world_id']);
        $recentActions = get_recent_actions($char['id'], 15, $user['world_id']);
        break;
    }
}

// If no active character, pick the most recent
if (!$activeChar && !empty($characters)) {
    $activeChar = $characters[0];
    $latestState = get_latest_character_state($activeChar['id'], $user['world_id']);
    $recentActions = get_recent_actions($activeChar['id'], 10, $user['world_id']);
}
?><!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ASHB2 — Dashboard</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>

  <nav>
    <div class="logo">ASHB2<span>.exe</span></div>
    <ul>
      <li><a href="index.php">Home</a></li>
      <li><a href="dashboard.php" class="active">Dashboard</a></li>
      <li><a href="logout.php">Logout</a></li>
    </ul>
    <div class="user-info"><?= htmlspecialchars($user['display_name'] ?: $user['username']) ?></div>
  </nav>

  <main>

    <!-- ── Header ────────────────────────────────────────── -->
    <section class="hero" style="padding-bottom:1.5rem; margin-bottom:1.5rem;">
      <div class="tag-group">
        <span class="tag amber">DASHBOARD</span>
        <span class="tag mint">SIMULATION ACTIVE</span>
      </div>
      <h1>Your Simulation.</h1>
    </section>

    <?php if ($feedbackMessage): ?>
      <div class="alert success"><?= htmlspecialchars($feedbackMessage) ?></div>
    <?php endif; ?>

    <?php if (empty($characters)): ?>
      <!-- ── No characters ───────────────────────────────── -->
      <?php /* Reached only when the account's entity is gone from sim.entity.
               The FK is ON DELETE CASCADE, so this means the world itself was
               retired — not that the visitor has yet to make a character. There
               is nothing for them to do about it, and register.php would only
               refuse them for already holding a login. */ ?>
      <div class="card text-center" style="padding:3rem;">
        <p class="section-label">Subject Not Found</p>
        <h3>Your subject is no longer in the world.</h3>
        <p class="lede" style="margin:1rem auto;">
          World <?= (int)$user['world_id'] ?> no longer holds the entity this account was
          assigned to &mdash; the world has been retired or reloaded. An
          administrator will need to reassign you.
        </p>
        <a href="index.php" class="btn amber">Return to index</a>
      </div>

    <?php elseif ($activeChar): ?>
      <!-- ── Character stats grid ────────────────────────── -->
      <div class="stats-grid">
        <div class="stat-box">
          <div class="stat-value"><?= htmlspecialchars($activeChar['name']) ?></div>
          <div class="stat-label">Character Name</div>
        </div>
        <div class="stat-box">
          <div class="stat-value" style="color:<?= $activeChar['is_alive'] ? 'var(--mint)' : 'var(--crimson)' ?>">
            <?= $activeChar['is_alive'] ? 'ALIVE' : 'DECEASED' ?>
          </div>
          <div class="stat-label">Status</div>
        </div>
        <div class="stat-box">
          <div class="stat-value"><?= number_format($activeChar['current_day']) ?></div>
          <div class="stat-label">Simulation Days Lived</div>
        </div>
        <?php /* Offspring was a `characters` column; sim.entity has no
                 equivalent and there is no lineage table here to count from.
                 Age is real data on this schema, so the tile shows that
                 instead of formatting a NULL into a confident "0". */ ?>
        <div class="stat-box">
          <div class="stat-value"><?= number_format((float)$activeChar['age'], 1) ?></div>
          <div class="stat-label">Age &middot; <?= htmlspecialchars(life_stage_label(
            $activeChar['life_stage'] === null ? null : (int)$activeChar['life_stage']
          )) ?></div>
        </div>
      </div>

      <!-- ── Latest State ────────────────────────────────── -->
      <?php if ($latestState): ?>
      <div class="grid-2" style="margin-bottom:1.5rem;">
        <div class="card">
          <div class="card-label">Vital Signs (Day <?= number_format($latestState['simulation_day']) ?>)</div>
          <div class="data-row">
            <span class="key">health</span>
            <span class="val <?= $latestState['health'] < 30 ? 'crimson' : ($latestState['health'] < 60 ? 'amber' : 'mint') ?>">
              <?= number_format($latestState['health'], 1) ?>
            </span>
          </div>
          <div class="data-row">
            <span class="key">hunger</span>
            <span class="val <?= $latestState['hunger'] > 70 ? 'crimson' : ($latestState['hunger'] > 40 ? 'amber' : 'mint') ?>">
              <?= number_format($latestState['hunger'], 1) ?>
            </span>
          </div>
          <div class="data-row">
            <span class="key">energy</span>
            <span class="val"><?= number_format($latestState['energy'], 1) ?></span>
          </div>
          <div class="data-row">
            <span class="key">stress</span>
            <span class="val <?= $latestState['stress'] > 60 ? 'crimson' : 'amber' ?>">
              <?= number_format($latestState['stress'], 1) ?>
            </span>
          </div>
          <?php if ($latestState['pos_x'] !== null): ?>
          <div class="data-row">
            <span class="key">position</span>
            <span class="val">(<?= $latestState['pos_x'] ?>, <?= $latestState['pos_y'] ?>)</span>
          </div>
          <?php endif; ?>
        </div>

        <div class="card">
          <div class="card-label">Personality Profile</div>
          <div class="data-row">
            <span class="key">openness</span>
            <span class="val amber"><?= number_format($activeChar['personality_openness'], 3) ?></span>
          </div>
          <div class="data-row">
            <span class="key">conscientiousness</span>
            <span class="val"><?= number_format($activeChar['personality_conscientiousness'] ?? 0.500, 3) ?></span>
          </div>
          <div class="data-row">
            <span class="key">extraversion</span>
            <span class="val"><?= number_format($activeChar['personality_extraversion'] ?? 0.500, 3) ?></span>
          </div>
          <div class="data-row">
            <span class="key">agreeableness</span>
            <span class="val mint"><?= number_format($activeChar['personality_agreeableness'] ?? 0.500, 3) ?></span>
          </div>
          <div class="data-row">
            <span class="key">neuroticism</span>
            <span class="val crimson"><?= number_format($activeChar['personality_neuroticism'], 3) ?></span>
          </div>
          <?php /* QI is nullable: a world exported before the QI columns
                   existed has no value to show, and a dash is honest where a
                   zero would read as "this person is an idiot". */ ?>
          <?php if (($activeChar['qi'] ?? null) !== null): ?>
          <div class="data-row">
            <span class="key">QI</span>
            <span class="val amber"><?= number_format((float)$activeChar['qi'], 0) ?>
              <span style="opacity:.6">/ <?= number_format((float)($activeChar['qi_potential'] ?? 0), 0) ?></span></span>
          </div>
          <div class="data-row">
            <span class="key">school-years</span>
            <span class="val"><?= number_format((float)($activeChar['school_years'] ?? 0), 1) ?></span>
          </div>
          <?php endif; ?>
          <div class="data-row">
            <span class="key">attachment</span>
            <?php /* A smallint enum ordinal, not a word — see
                     attachment_style_label(). Printing the column directly
                     would show "0". */ ?>
            <span class="val"><?= htmlspecialchars(attachment_style_label(
              $activeChar['attachment_style'] === null ? null : (int)$activeChar['attachment_style']
            )) ?></span>
          </div>
        </div>
      </div>
      <?php endif; ?>

      <!-- ── Inner state ─────────────────────────────────── -->
      <?php /* The behavioural-alignment form stood here. It wrote to
               `user_feedback`, which sql/schema_pg.sql does not define, and the
               loader owns every table it does define — so there is nowhere to
               put an answer. Restoring it needs a schema change, not a code
               change. What the engine DOES export is the entity's own account
               of itself, which is worth more than a 1–5 slider anyway. */ ?>
      <?php if (!empty($activeChar['inner_monologue']) || !empty($activeChar['current_goal'])): ?>
      <div class="card" style="margin-bottom:1.5rem;">
        <div class="card-label">Inner State</div>
        <?php if (!empty($activeChar['current_goal'])): ?>
          <div class="data-row">
            <span class="key">current goal</span>
            <span class="val amber"><?= htmlspecialchars($activeChar['current_goal']) ?></span>
          </div>
        <?php endif; ?>
        <?php if (!empty($activeChar['last_action'])): ?>
          <div class="data-row">
            <span class="key">last action</span>
            <span class="val"><?= htmlspecialchars($activeChar['last_action']) ?></span>
          </div>
        <?php endif; ?>
        <?php if (!empty($activeChar['inner_monologue'])): ?>
          <p style="margin-top:1rem; font-style:italic;">
            &ldquo;<?= htmlspecialchars($activeChar['inner_monologue']) ?>&rdquo;
          </p>
        <?php endif; ?>
      </div>
      <?php endif; ?>

      <!-- ── Recent Actions ──────────────────────────────── -->
      <?php if (!empty($recentActions)): ?>
      <div class="card">
        <div class="card-label">Recent Actions</div>
        <h3>Decision Log</h3>
        <?php /* At most one row: sim.entity keeps only the most recent action
                 and there is no character_actions history table here. */ ?>
        <p class="hint">The most recent action on record.</p>
        <div style="margin-top:1rem;">
          <?php foreach ($recentActions as $action): ?>
            <div class="data-row">
              <span style="display:flex; gap:0.5rem; width:100%;">
                <span class="key mono" style="white-space:nowrap;">
                  [Day <?= number_format($action['simulation_day']) ?>]
                </span>
                <span class="val mono" style="flex:1;">
                  <?= htmlspecialchars($action['action_type']) ?>
                  <?php if ($action['action_target']): ?>
                    → <?= htmlspecialchars($action['action_target']) ?>
                  <?php endif; ?>
                </span>
                <?php /* No utility_score column on sim.entity — the engine
                         exports the chosen action, not the planner's scoring.
                         Rendered only when there is something to render. */ ?>
                <?php if ($action['utility_score'] !== null): ?>
                <span class="muted mono" style="font-size:0.7rem; white-space:nowrap;">
                  util: <?= number_format((float)$action['utility_score'], 3) ?>
                </span>
                <?php endif; ?>
              </span>
            </div>
          <?php endforeach; ?>
        </div>
      </div>
      <?php endif; ?>

      <!-- ── Death notice ───────────────────────────────── -->
      <?php /* Gated on is_alive alone. It used to also require death_cause,
               which sim.entity does not record — keeping that condition would
               have meant the notice never appeared at all. Death arrives here
               as absence: the engine erases the dead before exporting, and the
               loader flips alive to false for anyone it stops seeing, so the
               last age it recorded is the age at death. */ ?>
      <?php if (!$activeChar['is_alive']): ?>
      <div class="card" style="border-color:var(--crimson); margin-top:1.5rem;">
        <div class="card-label crimson">Subject Deceased</div>
        <h3>
          <?= htmlspecialchars($activeChar['name']) ?>
          <?php if ($activeChar['age_at_death'] !== null): ?>
            died at age <?= number_format((float)$activeChar['age_at_death'], 1) ?>
          <?php else: ?>
            is no longer living
          <?php endif; ?>
        </h3>
        <p>Last seen on civ day <?= number_format((int)$activeChar['current_day']) ?>.</p>
        <p class="hint">The engine exports no cause of death; their record remains, and their relationships outlive them.</p>
      </div>
      <?php endif; ?>

    <?php else: ?>
      <!-- ── All characters deceased ─────────────────────── -->
      <div class="card text-center" style="padding:3rem;">
        <p class="section-label">No Subject</p>
        <h3>This account holds no assignment.</h3>
        <p class="lede" style="margin:1rem auto;">
          Subjects are assigned from the living population when an account is
          created; this one has none on record.
        </p>
        <a href="world.php" class="btn amber">View the world</a>
      </div>
    <?php endif; ?>

    <!-- ── All characters list ───────────────────────────── -->
    <?php if (count($characters) > 1): ?>
    <section class="section" style="margin-top:2rem;">
      <p class="section-label">All Characters</p>
      <h3>Your lineage history</h3>
      <div style="margin-top:1rem;">
        <?php foreach ($characters as $char): ?>
          <div class="data-row">
            <span class="key"><?= htmlspecialchars($char['name']) ?></span>
            <span class="val">
              <span class="<?= $char['is_alive'] ? 'mint' : 'crimson' ?>">
                <?= $char['is_alive'] ? 'ALIVE' : 'DECEASED' ?>
              </span>
              &middot; Day <?= number_format($char['current_day']) ?>
              <?php if ($char['total_offspring'] > 0): ?>
                &middot; <?= number_format($char['total_offspring']) ?> offspring
              <?php endif; ?>
            </span>
          </div>
        <?php endforeach; ?>
      </div>
    </section>
    <?php endif; ?>

  </main>

  <footer>
    ASHB2 <span class="dot">◆</span> Human-in-the-Loop Simulation &nbsp;|&nbsp; Scientific Terminal
  </footer>

</body>
</html>
