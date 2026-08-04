# ASHB2 Website — Human-in-the-Loop Simulation

PHP front-end for the ASHB2 simulation. Users create a "digital twin" from a
psychometric questionnaire, release it into the C++ world engine, and review
its life day by day.

## Two databases

The site reads from two servers, and which one a query goes to is decided by
which class it uses:

| Class | File | Engine | Holds | Schema |
|---|---|---|---|---|
| `Database::` | `db.php` | MySQL | accounts, characters, password resets, feedback | `sql/schema.sql` |
| `SimDb::` | `sim_db.php` | PostgreSQL | the world — `sim.*` | `sql/schema_pg.sql` |

They are separate because they have separate owners. The account tables are
written by this app. The `sim.*` tables are written only by
`scripts/db_spool_loader.py`, which COPYs what the C++ engine spools
(`--db-export`) into PostgreSQL. **Nothing in PHP writes to `sim.*`** — the
loader upserts whole chunks against a high-water mark, so a write from here
would be reverted by the next push at best.

## Requirements

- PHP 8+ (CLI + built-in web server) with **`pdo_mysql` and `pdo_pgsql`**
  enabled. The DLLs ship with PHP on Windows but are off by default; add
  `extension=pdo_pgsql` and `extension=pgsql` to the `php.ini` that
  `php --ini` reports, then check with
  `php -r "print_r(PDO::getAvailableDrivers());"`
- A MySQL / MariaDB server for accounts, and a PostgreSQL server for the
  simulation (neither has to be local)

## Setup

1. **Configure credentials**

   ```
   cd website
   copy .env.example .env      # (cp on Linux/macOS)
   ```

   Fill in `DB_*` for MySQL and `PG_DSN` for PostgreSQL. `PG_DSN` is the URL
   your provider gives you, sslmode included:

   ```
   PG_DSN=postgresql://user:password@host.example.com/ashb2?sslmode=require
   ```

   Real environment variables take precedence over `.env` values. `.env` is
   gitignored — never commit it.

2. **Import the schemas**

   ```
   php bin/import_schema.php          # MySQL: 17 tables + default sim config
   php bin/pg_check.php --import      # PostgreSQL: sim.*
   ```

   `php bin/pg_check.php` on its own is the one command that tells you where
   the simulation side stands: driver loaded, host reachable, schema present,
   and which worlds have data. Run it first whenever a page shows no world.

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
