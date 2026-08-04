<?php
/**
 * ASHB2: Password Reset — unavailable on this schema
 *
 * This page used to run the usual two-mode token flow: request a reset by
 * email, receive a link, set a new password. Both halves of that depend on
 * things sql/schema_pg.sql does not have.
 *
 *   - No `password_resets` table, so a token has nowhere to live. It has to be
 *     stored server-side with an expiry and a used flag, or it is not a reset
 *     token, it is a password.
 *   - No email column on sim.users. There is `login` and nothing else
 *     identifying, so there is no address to send a link to.
 *
 * The honest thing is to say so. The tempting alternative — keep the form,
 * accept the address, and always answer "if the email exists, a link has been
 * sent" — is worse than useless here: that message is deliberately
 * indistinguishable from success, so a visitor would wait for mail that no part
 * of this system is able to send.
 *
 * Restoring it needs a schema change (a resets table and an email column) plus
 * a mailer. Until then an administrator sets passwords directly; see the
 * snippet at the bottom of this page.
 */

require_once __DIR__ . '/auth.php';
session_init();

// Anyone already signed in has no business here.
if (session_user()) {
    header('Location: dashboard.php');
    exit;
}

function h($v): string { return htmlspecialchars((string)$v, ENT_QUOTES, 'UTF-8'); }

?><!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta name="robots" content="noindex,nofollow">
  <title>ASHB2 — Passphrase Recovery</title>
<style>
/* Same plate as login.php — the two pages are one system. */
:root{
  --void:#000000;
  --bone:#e8e6e1;
  --bone-dim:#8f8d88;
  --bone-faint:#54524e;
  --rule:rgba(232,230,225,.14);
  --rule-strong:rgba(232,230,225,.30);
  --magenta:#c98ba8;
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

.alert{
  border:1px solid rgba(201,139,168,.5);color:var(--magenta);
  padding:8px 10px;margin-bottom:12px;font-size:9.5px;letter-spacing:.1em;
  display:flex;gap:8px;align-items:baseline;
}
.alert .tag{font-size:8px;letter-spacing:.2em;text-transform:uppercase;flex:none;opacity:.8}

p{font-size:10px;color:var(--bone-dim);letter-spacing:.03em}
p + p{margin-top:8px}
code{
  display:block;background:#070707;border:1px solid var(--rule);
  padding:8px 10px;margin-top:10px;font-size:9.5px;color:var(--bone);
  white-space:pre;overflow-x:auto;
}
a{color:var(--violet);text-decoration:none;border-bottom:1px solid rgba(155,123,196,.35)}
a:hover{color:var(--bone);border-bottom-color:var(--bone)}
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
      <h1 class="plate-title">Recovery</h1>
      <div class="plate-sub">Passphrase reset &mdash; unavailable</div>
    </header>

    <div class="card">

      <section class="panel">
        <div class="panel-head">
          <span class="panel-no">01</span>
          <span class="panel-title">Not Configured</span>
          <span class="panel-note">Read only</span>
        </div>

        <div class="alert" role="alert">
          <span class="tag">Unavailable</span>
          <span>Self-service passphrase recovery is not enabled on this deployment.</span>
        </div>

        <p>
          Accounts here are identified by login alone. No email address is
          recorded, so there is nowhere to send a reset link &mdash; and no
          token store to validate one against.
        </p>
        <p>
          If you have lost your passphrase, an administrator can set a new one
          directly against the database.
        </p>
      </section>

      <section class="panel">
        <div class="panel-head">
          <span class="panel-no">02</span>
          <span class="panel-title">Administrator Procedure</span>
        </div>
        <p>Generate a hash, then store it &mdash; never write a plaintext passphrase into the table:</p>
        <code>php -r 'echo password_hash("NEW-PASSPHRASE", PASSWORD_BCRYPT, ["cost"=&gt;12]), PHP_EOL;'

psql "$PG_POOLER_URL" -c \
  "UPDATE sim.users SET password = '&lt;hash&gt;'
    WHERE world_id = <?= (int)SIM_WORLD_ID ?> AND login = '&lt;login&gt;';"</code>
      </section>

      <section class="panel">
        <div class="links">
          <a href="login.php">Back to sign in</a>
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
