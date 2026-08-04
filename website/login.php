<?php
/**
 * ASHB2 — Login
 *
 * Displays the sign-in form and processes submissions. On success, redirects
 * to the account dashboard (or wherever require_auth() bounced the visitor
 * from). On failure, re-renders with an error.
 *
 * Presentation follows dashboard/index.php: the specimen plate — absolute
 * black, bone-white monospace, hairline rules, no glow. Styles are embedded
 * rather than pulled from style.css because that stylesheet dresses the older
 * terminal look, and a page half in each language reads as a bug. Nothing here
 * needs to load before the form is usable.
 *
 * All PHP runs before any markup: session_start() after output is a "headers
 * already sent" warning and a session that never starts. Same order as every
 * other entry point in the app.
 */

require_once __DIR__ . '/auth.php';
session_init();

// Already signed in — nothing to do here.
if (session_user()) {
    header('Location: dashboard.php');
    exit;
}

/**
 * Where to go after a successful sign-in.
 *
 * require_auth() stores $_SERVER['REQUEST_URI'] here before bouncing someone,
 * so the value is normally a same-site path. It is still validated rather than
 * trusted: REQUEST_URI is attacker-influenced (it is just the request line),
 * and a stored "//evil.example/" or "https://evil.example/" would turn this
 * page into an open redirect — a login form is exactly the place phishing
 * wants one, since the victim has just proven the domain looks right.
 *
 * Accepted: a single leading slash followed by something that is not another
 * slash or backslash. That admits "/dashboard.php?x=1" and rejects both
 * protocol-relative "//host" and absolute "https://host" forms.
 */
function safe_redirect_target(?string $target, string $fallback = 'dashboard.php'): string
{
    if ($target === null || $target === '') {
        return $fallback;
    }
    if (preg_match('#^/(?![/\\\\])[^\s]*$#', $target)) {
        return $target;
    }
    // A bare relative path with no scheme and no leading slash is fine too.
    if (preg_match('#^[A-Za-z0-9._-]+\.php(?:[?\#][^\s]*)?$#', $target)) {
        return $target;
    }
    return $fallback;
}

// ── CSRF token ──────────────────────────────────────────────────────────────
// Login CSRF is the one people skip because "there is no session to protect
// yet". The attack it allows is real: an attacker submits their OWN
// credentials from the victim's browser, the victim ends up silently signed
// into the attacker's account, and everything they then do — characters they
// create, anything they type — lands in a history the attacker can read.
if (empty($_SESSION['csrf_login'])) {
    $_SESSION['csrf_login'] = bin2hex(random_bytes(32));
}
$csrf = $_SESSION['csrf_login'];

$error = '';
$login = '';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    // Login, not email: sim.users has no email column — see auth.php's header.
    $login    = trim((string)($_POST['login'] ?? ''));
    $password = (string)($_POST['password'] ?? '');
    $sent     = (string)($_POST['csrf'] ?? '');

    // hash_equals, not ===: token comparison is the textbook timing-attack
    // target, and it costs nothing to do correctly.
    if (!hash_equals($csrf, $sent)) {
        // Almost always a stale tab whose session expired, not an attack.
        $error = 'This form expired. Please try again.';
    } elseif ($login === '' || $password === '') {
        $error = 'Login and password are required.';
    } else {
        $result = login_user($login, $password);

        if ($result['success']) {
            // login_user() has already called session_regenerate_id(true).
            // Retire the token with the pre-login session so the next form
            // gets a fresh one.
            unset($_SESSION['csrf_login']);

            $redirect = safe_redirect_target($_SESSION['redirect_after_login'] ?? null);
            unset($_SESSION['redirect_after_login']);
            header('Location: ' . $redirect);
            exit;
        }

        // Whatever login_user() reports, it does not distinguish "no such
        // login" from "wrong password" — keep it that way, or this form
        // becomes an account-enumeration oracle.
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
  <title>ASHB2 — Authentication</title>
<style>
/* ─────────────────────────────────────────────────────────────────────────
   Palette and primitives lifted from dashboard/index.php so the two pages are
   one system. Museum archival: bone white on absolute black, accents used
   sparingly, no glow anywhere.
   ───────────────────────────────────────────────────────────────────────── */
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
  background:var(--void);
  color:var(--bone);
  font-family:var(--mono);
  font-size:11px;
  line-height:1.5;
  letter-spacing:.02em;
  -webkit-font-smoothing:antialiased;
  padding:clamp(10px,2.2vw,34px);
  min-height:100vh;
  display:flex;
  flex-direction:column;
}

/* Print grain + vignette, same two fixed layers as the plate. */
body::before,body::after{content:"";position:fixed;inset:0;pointer-events:none;z-index:0}
body::before{
  background-image:radial-gradient(circle at 50% 50%,rgba(255,255,255,.028) 0 1px,transparent 1px);
  background-size:3px 3px;
  mix-blend-mode:screen;
}
body::after{
  background:radial-gradient(ellipse at 50% 42%,transparent 55%,rgba(0,0,0,.85) 100%);
}

.plate{
  position:relative;z-index:1;
  width:100%;max-width:460px;
  margin:auto;                       /* optically centred in the viewport */
  min-width:0;
}

/* ── typographic primitives ───────────────────────────────────────────── */
.micro{font-size:8.5px;letter-spacing:.14em;text-transform:uppercase;color:var(--bone-faint)}
.rule{height:1px;background:var(--rule);border:0}
.rule-strong{height:1px;background:var(--rule-strong);border:0}
a{color:var(--violet);text-decoration:none;border-bottom:1px solid rgba(155,123,196,.35)}
a:hover{color:var(--bone);border-bottom-color:var(--bone)}

/* ── masthead ─────────────────────────────────────────────────────────── */
.institute{font-size:9px;letter-spacing:.42em;text-transform:uppercase;color:var(--bone-dim)}
.plate-title{font-size:clamp(15px,2.1vw,22px);letter-spacing:.30em;text-transform:uppercase;margin-top:8px}
.plate-sub{font-size:9px;letter-spacing:.2em;text-transform:uppercase;color:var(--bone-faint);margin-top:6px}

/* ── the card ─────────────────────────────────────────────────────────── */
.card{
  border:1px solid var(--rule);
  margin-top:12px;
  position:relative;
  background:var(--void);
}
/* Corner ticks — the same registration marks the specimen frame carries. */
.card::before,.card::after{
  content:"";position:absolute;width:9px;height:9px;border:1px solid var(--rule-strong);
  pointer-events:none;
}
.card::before{top:-1px;left:-1px;border-right:0;border-bottom:0}
.card::after{bottom:-1px;right:-1px;border-left:0;border-top:0}

.panel{padding:14px 15px 16px}
.panel + .panel{border-top:1px solid var(--rule)}
.panel-head{display:flex;align-items:baseline;gap:8px;margin-bottom:12px}
.panel-no{
  font-size:8.5px;letter-spacing:.1em;color:var(--void);
  background:var(--bone-faint);padding:1px 4px;flex:none;
}
.panel-title{font-size:8.5px;letter-spacing:.24em;text-transform:uppercase;color:var(--bone-dim)}
.panel-note{margin-left:auto;font-size:8px;letter-spacing:.12em;color:var(--bone-faint)}

/* ── form ─────────────────────────────────────────────────────────────── */
.field + .field{margin-top:12px}
label{
  display:block;
  font-size:8.5px;letter-spacing:.18em;text-transform:uppercase;
  color:var(--bone-dim);margin-bottom:5px;
}
input[type=text],input[type=password]{
  width:100%;
  background:var(--void);
  color:var(--bone);
  font-family:var(--mono);
  font-size:12px;
  letter-spacing:.04em;
  padding:9px 10px;
  border:1px solid var(--rule);
  border-radius:0;                      /* the plate has no rounded corners */
  outline:0;
  transition:border-color .18s ease,background .18s ease;
}
input::placeholder{color:var(--bone-faint);letter-spacing:.12em}
input:hover{border-color:var(--rule-strong)}
input:focus{
  border-color:var(--violet);
  background:#050505;
}
/* Keyboard users must still get a visible ring where colour alone is subtle. */
input:focus-visible{box-shadow:0 0 0 1px rgba(155,123,196,.45)}

/* Chrome's autofill repaints the field baby-blue, which detonates the palette.
   A long inset shadow is the only reliable way to override it. */
input:-webkit-autofill,
input:-webkit-autofill:hover,
input:-webkit-autofill:focus{
  -webkit-text-fill-color:var(--bone);
  -webkit-box-shadow:0 0 0 1000px #050505 inset;
  caret-color:var(--bone);
}

button{
  width:100%;
  margin-top:16px;
  background:var(--void);
  color:var(--bone);
  font-family:var(--mono);
  font-size:9px;letter-spacing:.28em;text-transform:uppercase;
  padding:11px 10px;
  border:1px solid var(--rule-strong);
  border-radius:0;
  cursor:pointer;
  transition:background .18s ease,border-color .18s ease,color .18s ease;
}
button:hover{background:var(--bone);color:var(--void);border-color:var(--bone)}
button:focus-visible{outline:1px solid var(--violet);outline-offset:2px}

/* ── error ────────────────────────────────────────────────────────────── */
.alert{
  border:1px solid rgba(201,139,168,.5);
  color:var(--magenta);
  padding:8px 10px;
  margin-bottom:12px;
  font-size:9.5px;letter-spacing:.1em;
  display:flex;gap:8px;align-items:baseline;
}
.alert .tag{font-size:8px;letter-spacing:.2em;text-transform:uppercase;flex:none;opacity:.8}

/* ── footer links ─────────────────────────────────────────────────────── */
.links{
  display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap;
  font-size:9px;letter-spacing:.1em;
}
.colophon{
  margin-top:12px;
  display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap;
  font-size:8px;letter-spacing:.14em;text-transform:uppercase;color:var(--bone-faint);
}

@media (max-width:420px){
  .plate-title{letter-spacing:.2em}
  .links{flex-direction:column;gap:6px}
}
</style>
</head>
<body>

  <main class="plate">

    <header>
      <div class="institute">ASHB2 &middot; Human-in-the-Loop Simulation</div>
      <h1 class="plate-title">Authentication</h1>
      <div class="plate-sub">Credentialled access &mdash; observation terminal</div>
    </header>

    <div class="card">

      <section class="panel">
        <div class="panel-head">
          <span class="panel-no">01</span>
          <span class="panel-title">Operator Credentials</span>
          <span class="panel-note">Required</span>
        </div>

        <?php if ($error !== ''): ?>
          <div class="alert" role="alert">
            <span class="tag">Denied</span>
            <span><?= h($error) ?></span>
          </div>
        <?php endif; ?>

        <form method="POST" action="login.php" novalidate>
          <input type="hidden" name="csrf" value="<?= h($csrf) ?>">

          <div class="field">
            <label for="login">Login</label>
            <input type="text" id="login" name="login"
                   placeholder="operator"
                   value="<?= h($login) ?>"
                   required autocomplete="username" autofocus
                   spellcheck="false" autocapitalize="none"
                   <?= $error !== '' ? 'aria-invalid="true"' : '' ?>>
          </div>

          <div class="field">
            <label for="password">Passphrase</label>
            <input type="password" id="password" name="password"
                   placeholder="&bull;&bull;&bull;&bull;&bull;&bull;&bull;&bull;"
                   required autocomplete="current-password"
                   <?= $error !== '' ? 'aria-invalid="true"' : '' ?>>
            <?php /* No minlength here. It is a rule for CHOOSING a passphrase,
                     and on a sign-in form it only tells an attacker the policy
                     while blocking legitimate older accounts from logging in. */ ?>
          </div>

          <button type="submit">Authenticate</button>
        </form>
      </section>

      <section class="panel">
        <div class="links">
          <a href="reset-password.php">Recover passphrase</a>
          <a href="register.php">Request access</a>
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
