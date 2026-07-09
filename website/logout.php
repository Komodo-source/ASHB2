<?php
/**
 * ASHB2: Logout
 * 
 * Destroys the user's session and redirects to the landing page.
 */

require_once __DIR__ . '/auth.php';
logout_user();

// Redirect to landing page
header('Location: index.php');
exit;
