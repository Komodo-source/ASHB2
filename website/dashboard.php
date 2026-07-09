<?php
/**
 * ASHB2: Dashboard
 * 
 * Protected page. Shows the user's characters, their current status,
 * recent actions, and a feedback form. Authenticated access only.
 */

require_once __DIR__ . '/auth.php';
$user = require_auth();

// Get all characters for this user
$characters = get_user_characters($user['id']);

// Handle feedback submission
$feedbackMessage = '';
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['submit_feedback'])) {
    $characterId = (int)($_POST['character_id'] ?? 0);
    $simDay = (int)($_POST['simulation_day'] ?? 0);
    $alignment = (int)($_POST['alignment_score'] ?? 3);

    // Validate alignment score
    $alignment = max(1, min(5, $alignment));

    try {
        Database::insert(
            'INSERT INTO user_feedback (user_id, character_id, simulation_day, alignment_score,
                                        narrative_feedback, would_change_action, would_change_personality)
             VALUES (?, ?, ?, ?, ?, ?, ?)
             ON DUPLICATE KEY UPDATE alignment_score = VALUES(alignment_score),
                                      narrative_feedback = VALUES(narrative_feedback),
                                      would_change_action = VALUES(would_change_action),
                                      would_change_personality = VALUES(would_change_personality)',
            [
                $user['id'],
                $characterId,
                $simDay,
                $alignment,
                $_POST['narrative_feedback'] ?? '',
                $_POST['would_change_action'] ?? '',
                $_POST['would_change_personality'] ?? '',
            ]
        );
        $feedbackMessage = 'Feedback recorded. Thank you.';
    } catch (Exception $e) {
        $feedbackMessage = 'Error saving feedback.';
        error_log('Feedback error: ' . $e->getMessage());
    }
}

// Get latest state for the active character (first alive one)
$activeChar = null;
$latestState = null;
$recentActions = [];

foreach ($characters as $char) {
    if ($char['is_alive'] && $char['is_active']) {
        $activeChar = $char;
        $latestState = get_latest_character_state($char['id']);
        $recentActions = get_recent_actions($char['id'], 15);
        break;
    }
}

// If no active character, pick the most recent
if (!$activeChar && !empty($characters)) {
    $activeChar = $characters[0];
    $latestState = get_latest_character_state($activeChar['id']);
    $recentActions = get_recent_actions($activeChar['id'], 10);
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
      <div class="card text-center" style="padding:3rem;">
        <p class="section-label">No Digital Twins Found</p>
        <h3>You haven't created a character yet.</h3>
        <p class="lede" style="margin:1rem auto;">The simulation is waiting for a seed. Complete the intake questionnaire to create your first digital twin.</p>
        <a href="register.php" class="btn amber">Create Your First Character</a>
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
        <div class="stat-box">
          <div class="stat-value"><?= number_format($activeChar['total_offspring']) ?></div>
          <div class="stat-label">Offspring</div>
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
          <div class="data-row">
            <span class="key">attachment</span>
            <span class="val"><?= htmlspecialchars($activeChar['attachment_style'] ?? 'secure') ?></span>
          </div>
        </div>
      </div>
      <?php endif; ?>

      <!-- ── Personality correction ──────────────────────── -->
      <div class="card" style="margin-bottom:1.5rem;">
        <div class="card-label">Behavioral Alignment</div>
        <h3>How is your character doing?</h3>
        <p>Each day, you can give feedback. This trains the simulation to better match your expectations.</p>

        <form method="POST" action="dashboard.php" style="margin-top:1rem;">
          <input type="hidden" name="character_id" value="<?= $activeChar['id'] ?>">
          <input type="hidden" name="simulation_day" value="<?= $activeChar['current_day'] ?>">
          <input type="hidden" name="submit_feedback" value="1">

          <div class="form-group">
            <label>How aligned was your character's behavior with your expectations?</label>
            <div style="display:flex; gap:0.5rem; flex-wrap:wrap; margin-top:0.5rem;">
              <?php for ($i = 1; $i <= 5; $i++): ?>
                <label style="display:flex; align-items:center; gap:0.3rem; font-family:var(--mono); font-size:0.8rem; cursor:pointer;">
                  <input type="radio" name="alignment_score" value="<?= $i ?>" <?= $i === 3 ? 'checked' : '' ?>>
                  <?= $i ?>
                </label>
              <?php endfor; ?>
              <span class="hint" style="margin-left:0.5rem;">1=completely wrong &middot; 5=perfect</span>
            </div>
          </div>

          <div class="form-group">
            <label for="would_change_action">What would you have done differently?</label>
            <textarea id="would_change_action" name="would_change_action" rows="2"
                      placeholder="e.g., I would have migrated instead of staying in the depleted area..."></textarea>
          </div>

          <div class="form-group">
            <label for="narrative_feedback">Notable events / Free response</label>
            <textarea id="narrative_feedback" name="narrative_feedback" rows="2"
                      placeholder="What stood out to you this day?"></textarea>
          </div>

          <button type="submit" class="btn amber small">Submit Feedback</button>
        </form>
      </div>

      <!-- ── Recent Actions ──────────────────────────────── -->
      <?php if (!empty($recentActions)): ?>
      <div class="card">
        <div class="card-label">Recent Actions</div>
        <h3>Decision Log</h3>
        <p class="hint">The last <?= count($recentActions) ?> actions your character took.</p>
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
                <span class="muted mono" style="font-size:0.7rem; white-space:nowrap;">
                  util: <?= number_format($action['utility_score'], 3) ?>
                </span>
              </span>
            </div>
          <?php endforeach; ?>
        </div>
      </div>
      <?php endif; ?>

      <!-- ── Death notice ───────────────────────────────── -->
      <?php if (!$activeChar['is_alive'] && $activeChar['death_cause']): ?>
      <div class="card" style="border-color:var(--crimson); margin-top:1.5rem;">
        <div class="card-label crimson">Character Deceased</div>
        <h3><?= htmlspecialchars($activeChar['name']) ?> died at Day <?= number_format($activeChar['age_at_death'] ?: $activeChar['current_day']) ?></h3>
        <p>Cause: <?= htmlspecialchars($activeChar['death_cause']) ?></p>
        <p class="hint">A full chronicle of their life will be available soon.</p>
      </div>
      <?php endif; ?>

    <?php else: ?>
      <!-- ── All characters deceased ─────────────────────── -->
      <div class="card text-center" style="padding:3rem;">
        <p class="section-label">All Characters Deceased</p>
        <h3>Your lineage has ended.</h3>
        <p class="lede" style="margin:1rem auto;">Every simulation ends. But you can always create a new character with a different profile and see how the outcome changes.</p>
        <a href="register.php" class="btn amber">Create New Character</a>
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
