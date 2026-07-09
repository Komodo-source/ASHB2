<?php
/**
 * ASHB2: Password Reset
 * 
 * Two-mode page:
 *   GET without token: Show email form to request reset.
 *   GET with token:    Show new password form.
 *   POST (request):    Generate reset token and display it (dev) / send email.
 *   POST (complete):   Validate token and update password.
 */

require_once __DIR__ . '/auth.php';
session_init();

$mode = 'request';   // 'request' | 'complete'
$token = $_GET['token'] ?? '';
$message = '';
$error = '';

// If token is provided, switch to complete mode
if (!empty($token)) {
    $mode = 'complete';
}

// ── Handle form submissions ─────────────────────────────────
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (isset($_POST['request_reset'])) {
        // ── Request mode ───────────────────────────────────
        $email = trim($_POST['email'] ?? '');
        if (empty($email)) {
            $error = 'Email address is required.';
        } else {
            $result = create_password_reset($email);
            if ($result['success']) {
                // In dev mode, show the token
                $tokenLink = 'reset-password.php?token=' . urlencode($result['token']);
                $message = 'Password reset link generated. In production, this would be emailed.';
                // Display the link for development convenience
                $message .= ' <a href="' . htmlspecialchars($tokenLink) . '" class="amber">Click here to reset</a>';
                // Store in session so we don't regenerate on refresh
                $_SESSION['reset_message'] = $message;
            } else {
                $error = $result['error'] ?? 'An error occurred.';
            }
        }
    } elseif (isset($_POST['complete_reset'])) {
        // ── Complete mode ──────────────────────────────────
        $token = $_POST['token'] ?? '';
        $password = $_POST['password'] ?? '';
        $confirm = $_POST['password_confirm'] ?? '';

        if (empty($token) || empty($password)) {
            $error = 'All fields are required.';
        } elseif ($password !== $confirm) {
            $error = 'Passwords do not match.';
        } else {
            $result = complete_password_reset($token, $password);
            if ($result['success']) {
                $message = 'Password has been reset successfully. <a href="login.php">Sign in with your new password.</a>';
                $mode = 'done';
            } else {
                $error = $result['error'];
            }
        }
    }
}

// Check for stored message (after redirect/post)
if (empty($message) && isset($_SESSION['reset_message'])) {
    $message = $_SESSION['reset_message'];
    unset($_SESSION['reset_message']);
}

?><!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ASHB2 — Reset Password</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>

  <nav>
    <div class="logo">ASHB2<span>.exe</span></div>
    <ul>
      <li><a href="index.php">Home</a></li>
      <li><a href="login.php">Login</a></li>
      <li><a href="register.php">Register</a></li>
      <li><a href="reset-password.php" class="active">Reset</a></li>
    </ul>
  </nav>

  <main>

    <div class="auth-container">
      <h1>Reset Password.</h1>

      <?php if ($error): ?>
        <div class="alert error"><?= htmlspecialchars($error) ?></div>
      <?php endif; ?>

      <?php if ($message): ?>
        <div class="alert success"><?= $message ?></div>
      <?php endif; ?>

      <?php if ($mode === 'request'): ?>
        <!-- ── Request form ──────────────────────────────── -->
        <p class="mono muted text-center" style="margin-bottom:2rem;">
          Enter your email to receive a reset token.
        </p>

        <form method="POST" action="reset-password.php">
          <input type="hidden" name="request_reset" value="1">

          <div class="form-group">
            <label for="email">Email</label>
            <input type="email" id="email" name="email"
                   placeholder="your@email.com" required autocomplete="email" autofocus>
          </div>

          <button type="submit" class="btn amber" style="width:100%;">Send Reset Token</button>
        </form>

        <div class="auth-links">
          <a href="login.php">Back to sign in.</a>
        </div>

      <?php elseif ($mode === 'complete'): ?>
        <!-- ── Complete reset form ───────────────────────── -->
        <p class="mono muted text-center" style="margin-bottom:2rem;">
          Enter your new password.
        </p>

        <form method="POST" action="reset-password.php">
          <input type="hidden" name="complete_reset" value="1">
          <input type="hidden" name="token" value="<?= htmlspecialchars($token) ?>">

          <div class="form-group">
            <label for="password">New Password</label>
            <input type="password" id="password" name="password"
                   placeholder="Min. 8 characters" required minlength="8"
                   autocomplete="new-password">
          </div>

          <div class="form-group">
            <label for="password_confirm">Confirm Password</label>
            <input type="password" id="password_confirm" name="password_confirm"
                   placeholder="Repeat password" required minlength="8"
                   autocomplete="new-password">
          </div>

          <button type="submit" class="btn amber" style="width:100%;">Reset Password</button>
        </form>

      <?php elseif ($mode === 'done'): ?>
        <!-- ── Success (message already shown) ───────────── -->
        <div class="auth-links">
          <a href="login.php">Sign in with your new password.</a>
        </div>
      <?php endif; ?>

      <div class="terminal-panel mt-2" style="font-size:0.75rem;">
        <span class="prompt">$</span> <span class="timestamp">[SECURITY]</span> Reset tokens expire after 1 hour.<br>
        <span class="prompt">$</span> <span class="timestamp">[SECURITY]</span> Tokens are single-use only.<br>
        <span class="prompt">$</span> <span class="timestamp">[SECURITY]</span> <span class="highlight">Your security is our priority.</span>
      </div>
    </div>

  </main>

  <footer>
    ASHB2 <span class="dot">◆</span> Human-in-the-Loop Simulation &nbsp;|&nbsp; Scientific Terminal
  </footer>

</body>
</html>
