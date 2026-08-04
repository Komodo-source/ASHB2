<?php
/**
 * ASHB2 — Register
 *
 * Creates an account and binds it to a person the simulation has already made.
 *
 * ── Why this is no longer a questionnaire ────────────────────────────────────
 *
 * This page used to collect a full psychological profile — Big Five sliders,
 * attachment style, five drives, memory parameters — and write it to a
 * `characters` row, on the premise in the landing copy: you are the seed, your
 * traits become the parameters of a new entity.
 *
 * sql/schema_pg.sql cannot express that, and not by omission. sim.users'
 * foreign key requires user_entity_id to already name a row in sim.entity, and
 * sim.entity is written only by the C++ engine through the spool loader. There
 * is no path by which the web app authors a person. Every trait the old form
 * collected — openness, attachment_style, the lot — is a column the engine
 * fills when it creates them.
 *
 * So registration claims instead of creates. You are assigned someone already
 * living in the world, with a history you did not choose. Collecting the
 * sliders anyway and discarding them would be worse than dropping them: a form
 * that implies your answers shape the simulation, when they reach nothing.
 *
 * Presentation follows login.php — the specimen plate, embedded rather than
 * from style.css, for the reason given there.
 */

require_once __DIR__ . '/auth.php';
session_init();

if (session_user()) {
    header('Location: dashboard.php');
    exit;
}

// Same reasoning as login.php: an attacker submitting their own credentials
// from the victim's browser lands the victim in an account the attacker reads.
if (empty($_SESSION['csrf_register'])) {
    $_SESSION['csrf_register'] = bin2hex(random_bytes(32));
}
$csrf = $_SESSION['csrf_register'];

$error = '';
$login = '';

// How much there is to claim. Shown on the form because "no entities left" is
// otherwise only discoverable by submitting, and because a world the engine has
// not populated yet is the likeliest reason registration fails on a fresh
// checkout. isAvailable() first: the database being down should read as the
// world being offline, not as a page-ending error.
$worldReady = false;
$available  = 0;
$world      = null;

if (SimDb::isAvailable()) {
    $world = get_world_summary(SIM_WORLD_ID);
    if ($world) {
        $worldReady = true;
        $available = (int)SimDb::fetchValue(
            'SELECT count(*) FROM sim.entity e
              WHERE e.world_id = ? AND e.alive
                AND NOT EXISTS (SELECT 1 FROM sim.users u
                                 WHERE u.world_id = e.world_id AND u.user_entity_id = e.sim_id)',
            [SIM_WORLD_ID]
        );
    }
}

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $login    = trim((string)($_POST['login'] ?? ''));
    $password = (string)($_POST['password'] ?? '');
    $confirm  = (string)($_POST['password_confirm'] ?? '');
    $sent     = (string)($_POST['csrf'] ?? '');

    if (!hash_equals($csrf, $sent)) {
        $error = 'This form expired. Please try again.';
    } elseif ($login === '' || $password === '') {
        $error = 'Login and passphrase are required.';
    } elseif ($password !== $confirm) {
        $error = 'The passphrases do not match.';
    } else {
        $result = register_user($login, $password);

        if ($result['success']) {
            // Sign in immediately. login_user() regenerates the session id, so
            // retire this form's token with the pre-login session.
            unset($_SESSION['csrf_register']);

            $loginResult = login_user($login, $password);
            if ($loginResult['success']) {
                header('Location: dashboard.php');
                exit;
            }
            // The account exists but the sign-in did not take — send them to
            // the form rather than reporting a failure that would read as
            // "try registering again" and collide on the login they just took.
            header('Location: login.php');
            exit;
        }

        $error = $result['error'];
    }
}

function h($v): string { return htmlspecialchars((string)$v, ENT_QUOTES, 'UTF-8'); }

?><!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta name="robots" content="noindex,nofollow">
  <title>ASHB2 — Request Access</title>
<style>
:root{
  --void:#000000;
  --bone:#e8e6e1;
  --bone-dim:#8f8d88;
  --bone-faint:#54524e;
  --rule:rgba(232,230,225,.14);
  --rule-strong:rgba(232,230,225,.30);
  --magenta:#c98ba8;
  --emerald:#4fa882;
  --violet:#9b7bc4;
  --mono:ui-monospace,"IBM Plex Mono","SFMono-Regular","Roboto Mono","DejaVu Sans Mono",Menlo,Consolas,monospace;
}
*{margin:0;padding:0;box-sizing:border-box}
html{background:var(--void)}
body{
  background:var(--void);color:var(--bone);font-family:var(--mono);
  font-size:11px;line-height:1.5;letter-spacing:.02em;
  -webkit-font-smoothing:antialiased;
  padding:clamp(10px,2.2vw,34px);min-height:100vh;
  display:flex;flex-direction:column;
}
body::before,body::after{content:"";position:fixed;inset:0;pointer-events:none;z-index:0}
body::before{
  background-image:radial-gradient(circle at 50% 50%,rgba(255,255,255,.028) 0 1px,transparent 1px);
  background-size:3px 3px;mix-blend-mode:screen;
}
body::after{background:radial-gradient(ellipse at 50% 42%,transparent 55%,rgba(0,0,0,.85) 100%)}

.plate{position:relative;z-index:1;width:100%;max-width:460px;margin:auto;min-width:0}
.micro{font-size:8.5px;letter-spacing:.14em;text-transform:uppercase;color:var(--bone-faint)}
a{color:var(--violet);text-decoration:none;border-bottom:1px solid rgba(155,123,196,.35)}
a:hover{color:var(--bone);border-bottom-color:var(--bone)}

.institute{font-size:9px;letter-spacing:.42em;text-transform:uppercase;color:var(--bone-dim)}
.plate-title{font-size:clamp(15px,2.1vw,22px);letter-spacing:.30em;text-transform:uppercase;margin-top:8px}
.plate-sub{font-size:9px;letter-spacing:.2em;text-transform:uppercase;color:var(--bone-faint);margin-top:6px}

.card{border:1px solid var(--rule);margin-top:12px;position:relative;background:var(--void)}
.card::before,.card::after{
  content:"";position:absolute;width:9px;height:9px;border:1px solid var(--rule-strong);pointer-events:none;
}
.card::before{top:-1px;left:-1px;border-right:0;border-bottom:0}
.card::after{bottom:-1px;right:-1px;border-left:0;border-top:0}

.panel{padding:14px 15px 16px}
.panel + .panel{border-top:1px solid var(--rule)}
.panel-head{display:flex;align-items:baseline;gap:8px;margin-bottom:12px}
.panel-no{font-size:8.5px;letter-spacing:.1em;color:var(--void);background:var(--bone-faint);padding:1px 4px;flex:none}
.panel-title{font-size:8.5px;letter-spacing:.24em;text-transform:uppercase;color:var(--bone-dim)}
.panel-note{margin-left:auto;font-size:8px;letter-spacing:.12em;color:var(--bone-faint)}

.field + .field{margin-top:12px}
label{display:block;font-size:8.5px;letter-spacing:.18em;text-transform:uppercase;color:var(--bone-dim);margin-bottom:5px}
input[type=text],input[type=password]{
  width:100%;background:var(--void);color:var(--bone);font-family:var(--mono);
  font-size:12px;letter-spacing:.04em;padding:9px 10px;
  border:1px solid var(--rule);border-radius:0;outline:0;
  transition:border-color .18s ease,background .18s ease;
}
input::placeholder{color:var(--bone-faint);letter-spacing:.12em}
input:hover{border-color:var(--rule-strong)}
input:focus{border-color:var(--violet);background:#050505}
input:focus-visible{box-shadow:0 0 0 1px rgba(155,123,196,.45)}
input:-webkit-autofill,input:-webkit-autofill:hover,input:-webkit-autofill:focus{
  -webkit-text-fill-color:var(--bone);
  -webkit-box-shadow:0 0 0 1000px #050505 inset;
  caret-color:var(--bone);
}
.hint{margin-top:5px;font-size:8px;letter-spacing:.1em;color:var(--bone-faint)}

button{
  width:100%;margin-top:16px;background:var(--void);color:var(--bone);font-family:var(--mono);
  font-size:9px;letter-spacing:.28em;text-transform:uppercase;padding:11px 10px;
  border:1px solid var(--rule-strong);border-radius:0;cursor:pointer;
  transition:background .18s ease,border-color .18s ease,color .18s ease;
}
button:hover:not(:disabled){background:var(--bone);color:var(--void);border-color:var(--bone)}
button:focus-visible{outline:1px solid var(--violet);outline-offset:2px}
button:disabled{color:var(--bone-faint);border-color:var(--rule);cursor:not-allowed}

.alert{
  border:1px solid rgba(201,139,168,.5);color:var(--magenta);
  padding:8px 10px;margin-bottom:12px;font-size:9.5px;letter-spacing:.1em;
  display:flex;gap:8px;align-items:baseline;
}
.alert .tag{font-size:8px;letter-spacing:.2em;text-transform:uppercase;flex:none;opacity:.8}

p{font-size:10px;color:var(--bone-dim);letter-spacing:.03em}
p + p{margin-top:8px}

.readout{display:flex;justify-content:space-between;gap:10px;font-size:9.5px;padding:3px 0}
.readout + .readout{border-top:1px solid var(--rule)}
.readout .k{color:var(--bone-faint);letter-spacing:.12em;text-transform:uppercase;font-size:8.5px}
.readout .v{color:var(--bone);letter-spacing:.06em}
.v.ok{color:var(--emerald)}
.v.none{color:var(--magenta)}

.links{display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap;font-size:9px;letter-spacing:.1em}
.colophon{
  margin-top:12px;display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap;
  font-size:8px;letter-spacing:.14em;text-transform:uppercase;color:var(--bone-faint);
}
@media (max-width:420px){.plate-title{letter-spacing:.2em}.links{flex-direction:column;gap:6px}}
</style>
</head>
<body>

  <main class="plate">

    <header>
      <div class="institute">ASHB2 &middot; Human-in-the-Loop Simulation</div>
      <h1 class="plate-title">Request Access</h1>
      <div class="plate-sub">Assignment to a living subject</div>
    </header>

    <div class="card">

      <section class="panel">
        <div class="panel-head">
          <span class="panel-no">01</span>
          <span class="panel-title">Subject Pool</span>
          <span class="panel-note">World <?= (int)SIM_WORLD_ID ?></span>
        </div>

        <?php if (!$worldReady): ?>
          <div class="alert" role="alert">
            <span class="tag">Offline</span>
            <span>World <?= (int)SIM_WORLD_ID ?> has no data yet. Run the engine and load the spool before registering.</span>
          </div>
        <?php else: ?>
          <div class="readout">
            <span class="k">Civ day</span>
            <span class="v"><?= (int)$world['last_day'] ?></span>
          </div>
          <div class="readout">
            <span class="k">Living population</span>
            <span class="v"><?= (int)$world['population'] ?></span>
          </div>
          <div class="readout">
            <span class="k">Unclaimed</span>
            <span class="v <?= $available > 0 ? 'ok' : 'none' ?>"><?= $available ?></span>
          </div>
        <?php endif; ?>

        <p style="margin-top:12px;">
          You do not design your subject. The simulation has already lived them
          &mdash; their personality, their history, whoever they have come to
          care about. Registration assigns you one at random, and from then on
          you observe.
        </p>
      </section>

      <section class="panel">
        <div class="panel-head">
          <span class="panel-no">02</span>
          <span class="panel-title">Operator Credentials</span>
          <span class="panel-note">Required</span>
        </div>

        <?php if ($error !== ''): ?>
          <div class="alert" role="alert">
            <span class="tag">Refused</span>
            <span><?= h($error) ?></span>
          </div>
        <?php endif; ?>

        <form method="POST" action="register.php" novalidate>
          <input type="hidden" name="csrf" value="<?= h($csrf) ?>">

          <div class="field">
            <label for="login">Login</label>
            <input type="text" id="login" name="login"
                   placeholder="operator"
                   value="<?= h($login) ?>"
                   required autocomplete="username" autofocus
                   spellcheck="false" autocapitalize="none"
                   <?= $error !== '' ? 'aria-invalid="true"' : '' ?>>
            <div class="hint">3&ndash;60 characters &middot; letters, numbers, hyphen, underscore</div>
          </div>

          <div class="field">
            <label for="password">Passphrase</label>
            <input type="password" id="password" name="password"
                   placeholder="&bull;&bull;&bull;&bull;&bull;&bull;&bull;&bull;"
                   required minlength="8" autocomplete="new-password">
            <div class="hint">Minimum 8 characters &middot; cannot be recovered if lost</div>
          </div>

          <div class="field">
            <label for="password_confirm">Confirm Passphrase</label>
            <input type="password" id="password_confirm" name="password_confirm"
                   placeholder="&bull;&bull;&bull;&bull;&bull;&bull;&bull;&bull;"
                   required minlength="8" autocomplete="new-password">
          </div>

          <button type="submit" <?= (!$worldReady || $available < 1) ? 'disabled' : '' ?>>
            <?= (!$worldReady || $available < 1) ? 'No subjects available' : 'Claim a subject' ?>
          </button>
        </form>
      </section>

      <section class="panel">
        <div class="links">
          <a href="login.php">Already credentialled</a>
          <a href="index.php">Index</a>
        </div>
      </section>

    </div>

    <div class="colophon">
      <span><a href="index.php">&larr; Index</a></span>
      <span>Session &middot; <?= h(date('Y-m-d H:i T')) ?></span>
    </div>

  </main>

</body>
</html>
