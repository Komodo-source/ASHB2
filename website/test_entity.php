<?php
/**
 * Test script for entity.php class using the existing schema_pg.sql
 * Creates test data and tests all entity methods
 */

// Start session
session_start();

// Set up test session data for entity ID 1. The cache slot is per entity —
// see entity::cacheKey() — so a single shared slot would prime the wrong one.
$_SESSION['static_entity_information_1'] = [
    'sim_id' => 1,
    'name' => 'Test Entity',
    'sex' => 'M',
    'birth_day' => 100
];

// The simulation connection (PostgreSQL), not the account one: everything
// tested below reads sim.*.
require_once __DIR__ . '/sim_db.php';
require_once __DIR__ . '/entity.php';

// Create entity instance
$entity = new entity();
$entity->entityId = 1; // Set entity ID directly for testing

echo "=== ASHB2 Entity Test Script ===\n";
echo "Testing with entity ID: 1\n\n";

// Test get_static_user_info()
echo "1. Testing get_static_user_info():\n";
try {
    $staticInfo = $entity->get_static_user_info();
    if ($staticInfo) {
        echo "   SUCCESS: Retrieved static info\n";
        echo "   Sim ID: {$staticInfo['sim_id']}\n";
        echo "   Name: {$staticInfo['name']}\n";
        echo "   Sex: {$staticInfo['sex']}\n";
        echo "   Birth Day: " . ($staticInfo['bday'] ?? 'Not set') . "\n";
    } else {
        echo "   FAILED: No data returned\n";
    }
} catch (Exception $e) {
    echo "   ERROR: " . $e->getMessage() . "\n";
}
echo "\n";

// Test get_light_statistics()
echo "2. Testing get_light_statistics():\n";
try {
    $lightStats = $entity->get_light_statistics();
    if ($lightStats && is_array($lightStats)) {
        echo "   SUCCESS: Retrieved light stats\n";
        echo "   Age: " . ($lightStats['age'] ?? 'N/A') . "\n";
        echo "   Health: " . ($lightStats['health'] ?? 'N/A') . "\n";
        echo "   Happiness: " . ($lightStats['happiness'] ?? 'N/A') . "\n";
        echo "   Stress: " . ($lightStats['stress'] ?? 'N/A') . "\n";
        echo "   Mental Health: " . ($lightStats['mental_health'] ?? 'N/A') . "\n";
    } else {
        echo "   FAILED: No data returned\n";
        // Show what we got for debugging
        if ($lightStats === false) {
            echo "   (Database query returned false - likely connection error)\n";
        } else {
            var_dump($lightStats);
        }
    }
} catch (Exception $e) {
    echo "   ERROR: " . $e->getMessage() . "\n";
}
echo "\n";

// Test get_deeper_statistics()
echo "3. Testing get_deeper_statistics():\n";
try {
    $deepStats = $entity->get_deeper_statistics();
    if ($deepStats && is_array($deepStats)) {
        echo "   SUCCESS: Retrieved deep stats\n";
        echo "   Age: " . ($deepStats['age'] ?? 'N/A') . "\n";
        echo "   Health: " . ($deepStats['health'] ?? 'N/A') . "\n";
        echo "   Hunger: " . ($deepStats['hunger'] ?? 'N/A') . "\n";
        echo "   Food Store: " . ($deepStats['food_store'] ?? 'N/A') . "\n";
    } else {
        echo "   FAILED: No data returned\n";
        var_dump($deepStats);
    }
} catch (Exception $e) {
    echo "   ERROR: " . $e->getMessage() . "\n";
}
echo "\n";

// Test social relationships
echo "4. Testing get_social_pointed():\n";
try {
    $social = $entity->get_social_pointed();
    if ($social && is_array($social)) {
        echo "   SUCCESS: " . count($social) . " social relationships found\n";
        if (!empty($social)) {
            echo "   First relationship: to_id={$social[0]['to_id']}, value={$social[0]['value']}\n";
        }
    } else {
        echo "   FAILED: No data returned or not an array\n";
        var_dump($social);
    }
} catch (Exception $e) {
    echo "   ERROR: " . $e->getMessage() . "\n";
}
echo "\n";

// Test anger relationships
echo "5. Testing get_anger_pointed():\n";
try {
    $anger = $entity->get_anger_pointed();
    if ($anger && is_array($anger)) {
        echo "   SUCCESS: " . count($anger) . " anger relationships found\n";
        if (!empty($anger)) {
            echo "   First relationship: to_id={$anger[0]['to_id']}, value={$anger[0]['value']}\n";
        }
    } else {
        echo "   FAILED: No data returned or not an array\n";
        var_dump($anger);
    }
} catch (Exception $e) {
    echo "   ERROR: " . $e->getMessage() . "\n";
}
echo "\n";

// Test desire relationships (fixed: now uses entity_desire table correctly)
echo "6. Testing get_desire_pointed():\n";
try {
    $desire = $entity->get_desire_pointed();
    if ($desire && is_array($desire)) {
        echo "   SUCCESS: " . count($desire) . " desire relationships found\n";
        if (!empty($desire)) {
            echo "   First relationship: to_id={$desire[0]['to_id']}, value={$desire[0]['value']}\n";
        }
    } else {
        echo "   FAILED: No data returned or not an array\n";
        var_dump($desire);
    }
} catch (Exception $e) {
    echo "   ERROR: " . $e->getMessage() . "\n";
}
echo "\n";

// Test couple relationships
echo "7. Testing get_couple_pointed():\n";
try {
    $couple = $entity->get_couple_pointed();
    if ($couple && is_array($couple)) {
        echo "   SUCCESS: " . count($couple) . " couple relationships found\n";
        if (!empty($couple)) {
            $first = $couple[0];
            echo "   First relationship: to_id={$first['to_id']}, satisfaction={$first['satisfaction']}, commitment={$first['commitment']}\n";
        }
    } else {
        echo "   FAILED: No data returned or not an array\n";
        var_dump($couple);
    }
} catch (Exception $e) {
    echo "   ERROR: " . $e->getMessage() . "\n";
}
echo "\n";

// Test mental model
echo "8. Testing get_mental_model():\n";
try {
    $mentalModel = $entity->get_mental_model();
    if ($mentalModel && is_array($mentalModel)) {
        echo "   SUCCESS: " . count($mentalModel) . " mental model entries found\n";
        if (!empty($mentalModel)) {
            $first = $mentalModel[0];
            echo "   First entry: to_id={$first['to_id']}, perceived_extraversion={$first['perceived_extraversion']}, trust_level={$first['trust_level']}\n";
        }
    } else {
        echo "   FAILED: No data returned or not an array\n";
        var_dump($mentalModel);
    }
} catch (Exception $e) {
    echo "   ERROR: " . $e->getMessage() . "\n";
}
echo "\n";

echo "=== Test Summary ===\n";
echo "If you saw database connection errors, you need to:\n";
echo "1. Ensure PostgreSQL is installed and running\n";
echo "2. Install the PDO PostgreSQL driver for PHP\n";
echo "   - On Ubuntu/Debian: sudo apt-get install php-pgsql\n";
echo "   - On CentOS/RHEL: sudo yum install php-pdo_pgsql\n";
echo "   - On Windows: uncomment extension=php_pdo_pgsql.dll in php.ini\n";
echo "3. Create the database and apply the schema:\n";
echo "   createdb ashb2\n";
echo "   psql -d ashb2 -f website/sql/schema_pg.sql\n";
echo "4. Insert test data (see instructions below)\n";
echo "5. Ensure your db.php file is configured correctly for your database\n\n";

echo "Sample data to insert for testing:\n";
echo "-- ============================\n";
echo "INSERT INTO sim.world (world_id, label, seed, last_day) VALUES (1, 'Test World', 'test-seed', 1000);\n";
echo "\n";
echo "INSERT INTO sim.entity (world_id, sim_id, last_seen_day, alive, name, sex, birth_day, birth_year, age, life_stage, health, hunger, food_store, hygiene, fatigue_level, injury_level, sleep_pressure, sleep_quality, antibody, disease_type, base_immunity, happiness, stress, stress_baseline, mental_health, loneliness, boredom, general_anger, esteem, sense_of_purpose, grief_intensity, social_deficit, days_without_social_action, meeting_count)\n";
echo "VALUES (1, 1, 1000, true, 'Test Entity', 'M', 100, -1000, 25.5, 3, 80.0, 20.0, 50.0, 70.0, 30.0, 10.0, 40.0, 80.0, 75, -1, 60, 70.0, 20.0, 15.0, 75.0, 15.0, 10.0, 5.0, 70.0, 75.0, 5.0, 20.0, 5.0, 12);\n";
echo "\n";
echo "-- Sample relationships\n";
echo "INSERT INTO sim.entity_social (world_id, from_id, to_id, social, last_seen_day) VALUES (1, 1, 2, 75.0, 1000);\n";
echo "INSERT INTO sim.entity_anger (world_id, from_id, to_id, anger, last_seen_day) VALUES (1, 1, 2, 20.0, 1000);\n";
echo "INSERT INTO sim.entity_desire (world_id, from_id, to_id, desire, last_seen_day) VALUES (1, 1, 2, 60.0, 1000);\n";
echo "INSERT INTO sim.entity_couple (world_id, from_id, to_id, commitment, satisfaction, trust, suspicion, days_together, last_seen_day) VALUES (1, 1, 2, 80.0, 75.0, 70.0, 10.0, 365, 1000);\n";
echo "INSERT INTO sim.entity_mental_model (world_id, from_id, to_id, perceived_extraversion, perceived_agreeableness, perceived_neuroticism, estimated_happiness, estimated_anger, estimated_stress, trust_level, predictability, perceived_intentionality, confidence, last_observed_day, last_interaction_day, last_interaction_outcome, faked_by_them, fake_trust_gain, last_seen_day) VALUES (1, 1, 2, 70.0, 60.0, 30.0, 75.0, 20.0, 25.0, 0.8, 0.7, 0.6, 0.75, 1000, 990, 1, false, 0.0, 1000);\n";
echo "\n";
echo "-- You'll need to create entity 2 for the relationships to work:\n";
echo "INSERT INTO sim.entity (world_id, sim_id, last_seen_day, alive, name, sex, birth_day, birth_year, age, life_stage, health, hunger, food_store, hygiene, fatigue_level, injury_level, sleep_pressure, sleep_quality, antibody, disease_type, base_immunity, happiness, stress, stress_baseline, mental_health, loneliness, boredom, general_anger, esteem, sense_of_purpose, grief_intensity, social_deficit, days_without_social_action, meeting_count)\n";
echo "VALUES (1, 2, 1000, true, 'Test Entity 2', 'F', 200, -900, 22.0, 2, 85.0, 15.0, 60.0, 75.0, 25.0, 5.0, 35.0, 85.0, 80, -1, 65, 75.0, 15.0, 10.0, 80.0, 10.0, 5.0, 3.0, 75.0, 80.0, 3.0, 15.0, 3.0, 8);\n";
?>