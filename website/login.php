<?php
/**
 * ASHB2: Login
 * 
 * Handles both displaying the login form and processing login submissions.
 * On success, redirects to dashboard (or redirect_after_login if set).
 * On failure, shows an error message.
 */

require_once __DIR__ . '/auth.php';
session_init();

// If already logged in, redirect to dashboard
$user = session_user();
if ($user) {
    header('Location: dashboard.php');
    exit;
}

$error = '';
$email = '';

// Process login form
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $email    = trim($_POST['email'] ?? '');
    $password = $_POST['password'] ?? '';

    if (empty($email) || empty($password)) {
        $error = 'Email and password are required.';
    } else {
        $result = login_user($email, $password);

        if ($result['success']) {
            // Check for redirect after login
            $redirect = $_SESSION['redirect_after_login'] ?? 'dashboard.php';
            unset($_SESSION['redirect_after_login']);
            header('Location: ' . $redirect);
            exit;
        }

        $error = $result['error'];
    }
}

?><!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ASHB2 — Sign In</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>

  <nav>
    <div class="logo">ASHB2<span>.exe</span></div>
    <ul>
      <li><a href="index.php">Home</a></li>
      <li><a href="login.php" class="active">Login</a></li>
      <li><a href="register.php">Register</a></li>
    </ul>
  </nav>

  <main>

    <div class="auth-container">
      <h1>Sign In.</h1>
      <p class="mono muted text-center" style="margin-bottom:2rem;">Authenticate to access the simulation dashboard.</p>

      <?php if ($error): ?>
        <div class="alert error"><?= htmlspecialchars($error) ?></div>
      <?php endif; ?>

      <form method="POST" action="login.php">
        <div class="form-group">
          <label for="email">Email</label>
          <input type="email" id="email" name="email"
                 placeholder="your@email.com"
                 value="<?= htmlspecialchars($email) ?>"
                 required autocomplete="email" autofocus>
        </div>

        <div class="form-group">
          <label for="password">Password</label>
          <input type="password" id="password" name="password"
                 placeholder="········"
                 required autocomplete="current-password" minlength="8">
        </div>

        <button type="submit" class="btn amber" style="width:100%;">Authenticate</button>
      </form>

      <div class="auth-links">
        <a href="reset-password.php">Forgot password?</a>
        <span class="muted"> &middot; </span>
        <a href="register.php">Create account</a>
      </div>

      <div class="terminal-panel mt-2" style="font-size:0.75rem;">
        <span class="prompt">$</span> <span class="timestamp">[AUTH]</span> Session required for dashboard access.<br>
        <span class="prompt">$</span> <span class="timestamp">[AUTH]</span> All passwords hashed with bcrypt (cost=12).<br>
        <span class="prompt">$</span> <span class="timestamp">[AUTH]</span> <span class="highlight">No plaintext storage. Ever.</span>
      </div>
    </div>

  </main>

  <footer>
    ASHB2 <span class="dot">◆</span> Human-in-the-Loop Simulation &nbsp;|&nbsp; Scientific Terminal
  </footer>

</body>
</html>
