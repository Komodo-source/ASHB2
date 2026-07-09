<?php
/**
 * ASHB2: Database Connection
 * 
 * Singleton PDO instance. All database access goes through this.
 * Prepared statements are used everywhere — no raw query concatenation.
 */

require_once __DIR__ . '/config.php';

class Database
{
    private static ?PDO $instance = null;

    /**
     * Get the PDO database connection.
     * Creates it on first call, returns the existing one thereafter.
     */
    public static function connect(): PDO
    {
        if (self::$instance === null) {
            $dsn = sprintf(
                'mysql:host=%s;port=%s;dbname=%s;charset=%s',
                DB_HOST,
                DB_PORT,
                DB_NAME,
                DB_CHARSET
            );

            $options = [
                PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
                PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
                PDO::ATTR_EMULATE_PREPARES   => false,
                PDO::MYSQL_ATTR_INIT_COMMAND => "SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci",
            ];

            try {
                self::$instance = new PDO($dsn, DB_USER, DB_PASS, $options);
            } catch (PDOException $e) {
                // Log the error but don't expose details in production
                error_log('Database connection failed: ' . $e->getMessage());
                
                if (APP_ENV === 'development') {
                    die('Database connection failed: ' . $e->getMessage());
                }
                
                http_response_code(500);
                die('An internal error occurred. Please try again later.');
            }
        }

        return self::$instance;
    }

    /**
     * Convenience wrapper: prepare + execute + return statement.
     */
    public static function query(string $sql, array $params = []): PDOStatement
    {
        $stmt = self::connect()->prepare($sql);
        $stmt->execute($params);
        return $stmt;
    }

    /**
     * Fetch a single row.
     */
    public static function fetchOne(string $sql, array $params = []): ?array
    {
        $result = self::query($sql, $params)->fetch();
        return $result ?: null;
    }

    /**
     * Fetch all rows.
     */
    public static function fetchAll(string $sql, array $params = []): array
    {
        return self::query($sql, $params)->fetchAll();
    }

    /**
     * Insert a row and return the last insert ID.
     */
    public static function insert(string $sql, array $params = []): string
    {
        self::query($sql, $params);
        return self::connect()->lastInsertId();
    }

    /**
     * Get the underlying PDO instance (for transactions, etc.).
     */
    public static function raw(): PDO
    {
        return self::connect();
    }

    /**
     * Prevent cloning.
     */
    private function __construct() {}
    private function __clone() {}
}
