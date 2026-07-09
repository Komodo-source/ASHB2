<?php
/**
 * ASHB2: Landing Page
 * 
 * Public-facing landing page with Scientific Terminal aesthetic.
 * Shows a pitch and login/register CTAs.
 */

require_once __DIR__ . '/auth.php';
session_init();

$user = session_user();
?><!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ASHB2 — Human-in-the-Loop Simulation</title>
  <meta name="description" content="You are not the player. You are the seed. Create your digital twin and release it into a living simulation.">
  <link rel="stylesheet" href="style.css">
</head>
<body>

  <nav>
    <div class="logo">ASHB2<span>.exe</span></div>
    <ul>
      <li><a href="index.php" class="active">Home</a></li>
      <?php if ($user): ?>
        <li><a href="dashboard.php">Dashboard</a></li>
        <li><a href="logout.php">Logout</a></li>
      <?php else: ?>
        <li><a href="login.php">Login</a></li>
        <li><a href="register.php">Register</a></li>
      <?php endif; ?>
    </ul>
  </nav>

  <main>

    <section class="hero">
      <div class="tag-group">
        <span class="tag amber">PERSONALITY-DRIVEN</span>
        <span class="tag mint">HUMAN-IN-THE-LOOP</span>
      </div>
      <h1>You are not the player.<br>You are the seed.</h1>
      <p class="sub">Create your digital twin. Release it into a living simulation. Watch it live, love, suffer, and die — based entirely on your real psychological profile.</p>
      <?php if ($user): ?>
        <div class="btn-group" style="display:flex; gap:1rem; flex-wrap:wrap; margin-top:1.5rem;">
          <a href="dashboard.php" class="btn amber">Enter Dashboard</a>
        </div>
      <?php else: ?>
        <div class="btn-group" style="display:flex; gap:1rem; flex-wrap:wrap; margin-top:1.5rem;">
          <a href="register.php" class="btn amber">Create Your Twin</a>
          <a href="login.php" class="btn">Sign In</a>
        </div>
      <?php endif; ?>
    </section>

    <section class="section">
      <p class="section-label">The Concept</p>
      <h2>Your psyche as a simulation seed.</h2>
      <p class="lede">
        Most simulations script behaviors. ASHB2 does the opposite — it starts with your
        psychological profile and lets behavior emerge. Your Big Five personality scores,
        your attachment style, your core drives — these become the parameters of an entity
        that lives entirely autonomously.
      </p>
      <div class="grid-2 mt-2">
        <div class="card">
          <div class="card-label">Input</div>
          <h3>Your Profile</h3>
          <p>A clinical questionnaire maps your psychology onto the simulation's parameter space. No gamification — raw personality data.</p>
          <div class="data-row"><span class="key">Openness</span><span class="val amber">0.73</span></div>
          <div class="data-row"><span class="key">Conscientiousness</span><span class="val">0.58</span></div>
          <div class="data-row"><span class="key">Extraversion</span><span class="val">0.41</span></div>
          <div class="data-row"><span class="key">Agreeableness</span><span class="val mint">0.82</span></div>
          <div class="data-row"><span class="key">Neuroticism</span><span class="val crimson">0.35</span></div>
          <div class="data-row"><span class="key">Attachment</span><span class="val">secure</span></div>
        </div>
        <div class="card">
          <div class="card-label">Output</div>
          <h3>Emergent History</h3>
          <p>Your character forges bonds, invents tools, joins tribes, and faces death — entirely autonomously. You observe and give feedback.</p>
          <div class="data-row"><span class="key">1 human day</span><span class="val amber">4 simulation days</span></div>
          <div class="data-row"><span class="key">Daily feedback</span><span class="val mint">yes</span></div>
          <div class="data-row"><span class="key">Full telemetry</span><span class="val">yes</span></div>
          <div class="data-row"><span class="key">Character timeline</span><span class="val">yes</span></div>
          <div class="data-row"><span class="key">Social network map</span><span class="val">future</span></div>
        </div>
      </div>
    </section>

    <section class="section">
      <p class="section-label">The Loop</p>
      <h2>Three steps from seed to legacy.</h2>

      <div style="border-left: 2px solid var(--amber); padding-left: 1.25rem; margin: 1.5rem 0;">
        <p class="mono amber" style="font-size:0.7rem; text-transform:uppercase; letter-spacing:0.1em; margin-bottom:0.25rem;">Step 01</p>
        <p style="font-family:var(--serif); font-size:1.2rem; font-weight:700; margin-bottom:0.5rem;">Complete the intake.</p>
        <p>A clinical questionnaire maps your psychology. Fifteen minutes. No progress bars. No gamification.</p>
      </div>

      <div style="border-left: 2px solid var(--amber); padding-left: 1.25rem; margin: 1.5rem 0;">
        <p class="mono amber" style="font-size:0.7rem; text-transform:uppercase; letter-spacing:0.1em; margin-bottom:0.25rem;">Step 02</p>
        <p style="font-family:var(--serif); font-size:1.2rem; font-weight:700; margin-bottom:0.5rem;">Release into the world.</p>
        <p>Your character is injected into the simulation. From this moment, you have zero control. The GOAP planner takes over.</p>
      </div>

      <div style="border-left: 2px solid var(--amber); padding-left: 1.25rem; margin: 1.5rem 0;">
        <p class="mono amber" style="font-size:0.7rem; text-transform:uppercase; letter-spacing:0.1em; margin-bottom:0.25rem;">Step 03</p>
        <p style="font-family:var(--serif); font-size:1.2rem; font-weight:700; margin-bottom:0.5rem;">Observe and give feedback.</p>
        <p>Each day, review your character's choices. Did they act like you? What would you have done differently? Your feedback trains the system.</p>
      </div>
    </section>

    <section class="section text-center">
      <p class="section-label">Determinism</p>
      <h2>Same seed. Same history.<br>Until you change one variable.</h2>
      <p class="lede" style="margin:1rem auto 0 auto; max-width:65ch;">
        The simulation is fully deterministic. Your psychological profile is a seed.
        Change one trait score and the entire life trajectory diverges.
      </p>
      <?php if (!$user): ?>
        <div class="btn-group" style="justify-content:center; margin-top:1.5rem;">
          <a href="register.php" class="btn amber">Create Your Digital Twin</a>
        </div>
      <?php endif; ?>
    </section>

  </main>

  <footer>
    ASHB2 <span class="dot">◆</span> Human-in-the-Loop Simulation &nbsp;|&nbsp; Scientific Terminal
  </footer>

</body>
</html>
