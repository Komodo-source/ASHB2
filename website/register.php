<?php
/**
 * ASHB2: Register
 * 
 * User registration combined with initial character creation.
 * Collects account info + full personality profile in one flow.
 */

require_once __DIR__ . '/auth.php';
session_init();

// If already logged in, redirect to dashboard
$user = session_user();
if ($user) {
    header('Location: dashboard.php');
    exit;
}

$errors = [];
$formData = [
    'email' => '',
    'username' => '',
    'display_name' => '',
    'name' => '',
    // Personality defaults (center of scale)
    'personality_openness' => '0.500',
    'personality_conscientiousness' => '0.500',
    'personality_extraversion' => '0.500',
    'personality_agreeableness' => '0.500',
    'personality_neuroticism' => '0.500',
    'attachment_style' => 'secure',
    'drive_exploration' => '0.500',
    'drive_social' => '0.500',
    'drive_safety' => '0.500',
    'drive_dominance' => '0.500',
    'drive_achievement' => '0.500',
    'memory_decay_rate' => '0.050',
    'memory_trauma_retention' => '0.500',
];

// Process registration
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    // Collect form data
    foreach ($formData as $key => &$val) {
        $val = $_POST[$key] ?? $val;
    }
    unset($val);

    $password = $_POST['password'] ?? '';
    $password_confirm = $_POST['password_confirm'] ?? '';

    // ── Validate passwords match ─────────────────────────────
    if ($password !== $password_confirm) {
        $errors[] = 'Passwords do not match.';
    }

    // ── Register user ────────────────────────────────────────
    if (empty($errors)) {
        $regResult = register_user(
            $formData['email'],
            $formData['username'],
            $password,
            $formData['display_name']
        );

        if ($regResult['success']) {
            // ── Create the initial character ──────────────────
            $charResult = create_character($regResult['user_id'], $formData);

            if ($charResult['success']) {
                // Log the user in automatically
                $loginResult = login_user($formData['email'], $password);
                if ($loginResult['success']) {
                    header('Location: dashboard.php');
                    exit;
                }
            } else {
                $errors[] = $charResult['error'];
            }
        } else {
            $errors[] = $regResult['error'];
        }
    }
}

?><!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ASHB2 — Create Your Twin</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>

  <nav>
    <div class="logo">ASHB2<span>.exe</span></div>
    <ul>
      <li><a href="index.php">Home</a></li>
      <li><a href="login.php">Login</a></li>
      <li><a href="register.php" class="active">Register</a></li>
    </ul>
  </nav>

  <main>

    <div class="auth-container" style="max-width:600px;">
      <h1>Create Your Digital Twin.</h1>
      <p class="mono muted text-center" style="margin-bottom:2rem;">
        Step 01: Account. Step 02: Psyche profile. One injection.
      </p>

      <?php if (!empty($errors)): ?>
        <?php foreach ($errors as $err): ?>
          <div class="alert error"><?= htmlspecialchars($err) ?></div>
        <?php endforeach; ?>
      <?php endif; ?>

      <form method="POST" action="register.php">

        <!-- ── Account Section ───────────────────────────── -->
        <h4>Account Credentials</h4>

        <div class="form-row">
          <div class="form-group">
            <label for="email">Email</label>
            <input type="email" id="email" name="email"
                   value="<?= htmlspecialchars($formData['email']) ?>"
                   required autocomplete="email" placeholder="you@email.com">
          </div>

          <div class="form-group">
            <label for="username">Username</label>
            <input type="text" id="username" name="username"
                   value="<?= htmlspecialchars($formData['username']) ?>"
                   required autocomplete="username" placeholder="3–60 chars"
                   pattern="[a-zA-Z0-9_-]{3,60}">
          </div>
        </div>

        <div class="form-row">
          <div class="form-group">
            <label for="password">Password</label>
            <input type="password" id="password" name="password"
                   required autocomplete="new-password" minlength="8"
                   placeholder="Min. 8 characters">
          </div>

          <div class="form-group">
            <label for="password_confirm">Confirm Password</label>
            <input type="password" id="password_confirm" name="password_confirm"
                   required autocomplete="new-password" minlength="8"
                   placeholder="Repeat password">
          </div>
        </div>

        <div class="form-group">
          <label for="display_name">Display Name <span class="muted">(optional)</span></label>
          <input type="text" id="display_name" name="display_name"
                 value="<?= htmlspecialchars($formData['display_name']) ?>"
                 placeholder="How you appear to others">
        </div>

        <!-- ── Character Section ─────────────────────────── -->
        <hr style="border:none; border-top:1px solid var(--border); margin:2rem 0;">

        <h4>Your Character</h4>

        <div class="form-group">
          <label for="character_name">Character Name</label>
          <input type="text" id="character_name" name="name"
                 value="<?= htmlspecialchars($formData['name']) ?>"
                 required placeholder="What will your twin be called?">
          <p class="hint">This name will appear in the simulation logs.</p>
        </div>

        <!-- ── Personality Section ───────────────────────── -->
        <hr style="border:none; border-top:1px solid var(--border); margin:2rem 0;">

        <h4>Big Five Personality</h4>
        <p class="hint" style="margin-bottom:1rem;">
          Rate each trait from 0.000 (very low) to 1.000 (very high). 0.500 is average.
        </p>

        <div class="form-group">
          <label for="personality_openness">Openness <span class="muted">(curiosity, creativity)</span></label>
          <input type="number" id="personality_openness" name="personality_openness"
                 value="<?= htmlspecialchars($formData['personality_openness']) ?>"
                 min="0" max="1" step="0.001" required>
        </div>

        <div class="form-group">
          <label for="personality_conscientiousness">Conscientiousness <span class="muted">(discipline, organization)</span></label>
          <input type="number" id="personality_conscientiousness" name="personality_conscientiousness"
                 value="<?= htmlspecialchars($formData['personality_conscientiousness']) ?>"
                 min="0" max="1" step="0.001" required>
        </div>

        <div class="form-group">
          <label for="personality_extraversion">Extraversion <span class="muted">(sociability, energy)</span></label>
          <input type="number" id="personality_extraversion" name="personality_extraversion"
                 value="<?= htmlspecialchars($formData['personality_extraversion']) ?>"
                 min="0" max="1" step="0.001" required>
        </div>

        <div class="form-group">
          <label for="personality_agreeableness">Agreeableness <span class="muted">(cooperation, trust)</span></label>
          <input type="number" id="personality_agreeableness" name="personality_agreeableness"
                 value="<?= htmlspecialchars($formData['personality_agreeableness']) ?>"
                 min="0" max="1" step="0.001" required>
        </div>

        <div class="form-group">
          <label for="personality_neuroticism">Neuroticism <span class="muted">(emotional volatility, anxiety)</span></label>
          <input type="number" id="personality_neuroticism" name="personality_neuroticism"
                 value="<?= htmlspecialchars($formData['personality_neuroticism']) ?>"
                 min="0" max="1" step="0.001" required>
        </div>

        <!-- ── Attachment Style ──────────────────────────── -->
        <hr style="border:none; border-top:1px solid var(--border); margin:2rem 0;">

        <h4>Attachment Style</h4>

        <div class="form-group">
          <label for="attachment_style">How do you form bonds?</label>
          <select id="attachment_style" name="attachment_style" required>
            <option value="secure"      <?= $formData['attachment_style'] === 'secure'      ? 'selected' : '' ?>>Secure — Comfortable with intimacy and independence</option>
            <option value="anxious"     <?= $formData['attachment_style'] === 'anxious'     ? 'selected' : '' ?>>Anxious — Fear of abandonment, need for reassurance</option>
            <option value="avoidant"    <?= $formData['attachment_style'] === 'avoidant'    ? 'selected' : '' ?>>Avoidant — Prefers distance, uncomfortable with closeness</option>
            <option value="disorganized"<?= $formData['attachment_style'] === 'disorganized' ? 'selected' : '' ?>>Disorganized — Mixed patterns, unpredictable</option>
            <option value="fearful"     <?= $formData['attachment_style'] === 'fearful'     ? 'selected' : '' ?>>Fearful — Desires closeness but fears rejection</option>
          </select>
        </div>

        <!-- ── Core Drives ───────────────────────────────── -->
        <hr style="border:none; border-top:1px solid var(--border); margin:2rem 0;">

        <h4>Core Drives</h4>
        <p class="hint" style="margin-bottom:1rem;">
          What motivates your character? (0.000 = irrelevant, 1.000 = primary driver)
        </p>

        <div class="form-row">
          <div class="form-group">
            <label for="drive_exploration">Exploration</label>
            <input type="number" id="drive_exploration" name="drive_exploration"
                   value="<?= htmlspecialchars($formData['drive_exploration']) ?>"
                   min="0" max="1" step="0.001">
          </div>
          <div class="form-group">
            <label for="drive_social">Social</label>
            <input type="number" id="drive_social" name="drive_social"
                   value="<?= htmlspecialchars($formData['drive_social']) ?>"
                   min="0" max="1" step="0.001">
          </div>
        </div>

        <div class="form-row">
          <div class="form-group">
            <label for="drive_safety">Safety</label>
            <input type="number" id="drive_safety" name="drive_safety"
                   value="<?= htmlspecialchars($formData['drive_safety']) ?>"
                   min="0" max="1" step="0.001">
          </div>
          <div class="form-group">
            <label for="drive_dominance">Dominance</label>
            <input type="number" id="drive_dominance" name="drive_dominance"
                   value="<?= htmlspecialchars($formData['drive_dominance']) ?>"
                   min="0" max="1" step="0.001">
          </div>
        </div>

        <div class="form-row">
          <div class="form-group">
            <label for="drive_achievement">Achievement</label>
            <input type="number" id="drive_achievement" name="drive_achievement"
                   value="<?= htmlspecialchars($formData['drive_achievement']) ?>"
                   min="0" max="1" step="0.001">
          </div>
          <div></div>
        </div>

        <!-- ── Memory Parameters ─────────────────────────── -->
        <hr style="border:none; border-top:1px solid var(--border); margin:2rem 0;">

        <h4>Memory Parameters</h4>

        <div class="form-row">
          <div class="form-group">
            <label for="memory_decay_rate">Memory Decay Rate <span class="muted">(0.01 = slow, 0.20 = fast)</span></label>
            <input type="number" id="memory_decay_rate" name="memory_decay_rate"
                   value="<?= htmlspecialchars($formData['memory_decay_rate']) ?>"
                   min="0.01" max="0.20" step="0.001">
          </div>
          <div class="form-group">
            <label for="memory_trauma_retention">Trauma Retention <span class="muted">(0.0 = forget, 1.0 = remember vividly)</span></label>
            <input type="number" id="memory_trauma_retention" name="memory_trauma_retention"
                   value="<?= htmlspecialchars($formData['memory_trauma_retention']) ?>"
                   min="0" max="1" step="0.001">
          </div>
        </div>

        <!-- ── Submit ────────────────────────────────────── -->
        <hr style="border:none; border-top:1px solid var(--border); margin:2rem 0;">

        <button type="submit" class="btn amber" style="width:100%;">
          Inject Twin Into Simulation
        </button>

        <p class="hint text-center" style="margin-top:0.75rem;">
          By registering, you agree that your personality profile will be used to generate autonomous behavior in a simulation environment.
        </p>
      </form>

      <div class="auth-links">
        Already have a twin? <a href="login.php">Sign in.</a>
      </div>
    </div>

  </main>

  <footer>
    ASHB2 <span class="dot">◆</span> Human-in-the-Loop Simulation &nbsp;|&nbsp; Scientific Terminal
  </footer>

</body>
</html>
