# ASHB2 Website — Human-in-the-Loop Simulation

PHP front-end for the ASHB2 simulation. Users create a "digital twin" from a
psychometric questionnaire, release it into the C++ world engine, and review
its life day by day.

## Requirements

- PHP 8+ (CLI + built-in web server; `pdo_mysql` extension enabled)
- A remote MySQL / MariaDB server (no local DB required)

## Setup

1. **Configure credentials**

   ```
   cd website
   copy .env.example .env      # (cp on Linux/macOS)
   ```

   Edit `.env` and fill in your remote MySQL host, database name, user and
   password. Real environment variables take precedence over `.env` values.
   `.env` is gitignored — never commit it.

2. **Import the schema** (creates all 17 tables + the default simulation config)

   ```
   php bin/import_schema.php
   ```

3. **Seed demo data** (optional but recommended — a demo user with 8 fake
   simulation days so the dashboard renders without the engine)

   ```
   php bin/seed_demo.php
   ```

4. **Run the dev server** (from the `website/` directory)

   ```
   php -S localhost:8080
   ```

5. Open http://localhost:8080 and log in as **demo / demo1234**
   (or register your own account, which creates a character from the
   questionnaire).

## What's not here yet

The `bridge/` directory (inject/report files exchanged with the C++ engine)
and the 6-hour scheduler that advances the world 4 simulation days per human
day are **coming in later phases**. Until then, character state only changes
via the seeder.
