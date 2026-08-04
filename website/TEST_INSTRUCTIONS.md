# Test Files for ASHB2 Entity Debugging

## Files Created

1. `test_schema.sql` - SQL schema and sample data for testing
2. `test_entity.php` - Test script to exercise the entity.php class

## Setup Instructions

### 1. Database Setup

If you're using PostgreSQL (based on the schema_pg.sql file in the repo):

```bash
# Create database
createdb ashb2_test

# Apply schema
psql -d ashb2_test -f test_schema.sql
```

If you're using MySQL, you'll need to convert the SQL (change SERIAL to AUTO_INCREMENT, adjust syntax slightly).

### 2. Database Configuration

Make sure your `db.php` file is configured to connect to your test database:
- Host: localhost (or your DB host)
- Database: ashb2_test (or whatever you named it)
- Username/password: your DB credentials

### 3. Running the Tests

```bash
php test_entity.php
```

This will:
- Start a PHP session
- Set up test session data for entity ID 1
- Instantiate the entity class
- Test all the major methods:
  - get_static_user_info()
  - get_light_statistics()
  - get_deeper_statistics()
  - get_social_pointed()
  - get_anger_pointed()
  - get_desire_pointed()
  - get_couple_pointed()
  - get_mental_model()

## Expected Output

You should see formatted arrays showing the data returned from each method, matching the sample data inserted in the test schema.

## Troubleshooting

If you get database connection errors:
1. Check your db.php configuration
2. Verify the database exists and is accessible
3. Ensure the tables were created properly

If you get "undefined variable" or similar PHP errors:
1. Make sure entity.php is in the same directory
2. Check that your PHP version is compatible
3. Verify session_start() is working

## Notes

- The test data includes 3 entities with various relationships
- Entity ID 1 is set as the "current user" in the session
- Relationships are set up between entities 1, 2, and 3
- All values are sample data for testing purposes only

## Customizing

You can modify:
- test_entity.php to test different entity IDs
- test_schema.sql to add more varied test data
- The session data in test_entity.php to simulate different users