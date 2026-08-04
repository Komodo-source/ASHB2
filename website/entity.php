<?php
require_once __DIR__ . '/sim_db.php';

// TODO: rajouter try et catch avec message utilisateur, simple évite
//remove session lorsque logout

class entity {
    public $entityId;
    public $entityAge;
    public $entityHealth;
    public $entityHapiness;
    public $entityStress;
    public $entityMentalHealth;
    public $name;
    public $entityLoneliness;
    public $entityBoredom;
    public $entityGeneralAnger;
    public $entityHygiene;
    public $entitySex;
    public $entityBDay;       // birth day of the year (0-364)
    public $entityBirthYear;   // BC/AD year of birth (e.g. -4985 for 49 for 49895 BC)
    public $entityAntiBody; // pourcentage
    public $entityDiseaseType; //-1 if no disease

  // Keyed by entity id, not one slot for the whole session: name/sex/birthday
  // are per person, and a single slot hands you the FIRST entity you looked at
  // for every one you look at afterwards.
  private function cacheKey(){
    return 'static_entity_information_' . (int)$this->entityId;
  }

  function get_static_user_info(){
    if (session_status() === PHP_SESSION_NONE) {
      session_start();          // $_SESSION does not exist until this runs
    }
    $data_cached = $_SESSION[$this->cacheKey()] ?? null;
    if($data_cached){ //if user entity data are already cache just display them
      return $data_cached;
    }
    $users = SimDb::fetchOne(
        'SELECT sim_id, name, sex, birth_day FROM sim.entity WHERE world_id = ? AND sim_id = ?',
        [SIM_WORLD_ID, $this->entityId]
    );
    if($users){
        $_SESSION[$this->cacheKey()]["sim_id"] = $users["sim_id"];
        $_SESSION[$this->cacheKey()]["name"] = $users["name"];
        $_SESSION[$this->cacheKey()]["sex"] = $users["sex"];
        $_SESSION[$this->cacheKey()]["bday"] = $users["birth_day"];
        return $_SESSION[$this->cacheKey()];
    }
    return null;
  }

  function get_social_pointed(){
    if(empty($_SESSION[$this->cacheKey()])){
      $this->get_static_user_info();
    }
    $pointed_attrributes = SimDb::fetchAll(
        'SELECT to_id, social, last_seen_day FROM sim.entity_social
        WHERE world_id = ? AND from_id = ?', [SIM_WORLD_ID, $this->entityId]
    );
    $values = array();
    foreach($pointed_attrributes as $attributes){
        array_push($values, array("from_id" => $this->entityId, "to_id" => $attributes["to_id"], "value" => $attributes["social"], "last_seen_day" => $attributes["last_seen_day"]));
    }
    return $values;
  }

  function get_anger_pointed(){
    if(empty($_SESSION[$this->cacheKey()])){
      $this->get_static_user_info();
    }
    $pointed_attrributes = SimDb::fetchAll(
        'SELECT to_id, anger, last_seen_day FROM sim.entity_anger
        WHERE world_id = ? AND from_id = ?', [SIM_WORLD_ID, $this->entityId]
    );
    $values = array();
    foreach($pointed_attrributes as $attributes){
        array_push($values, array("from_id" => $this->entityId, "to_id" => $attributes["to_id"], "value" => $attributes["anger"], "last_seen_day" => $attributes["last_seen_day"]));
    }
    return $values;
  }

  function get_desire_pointed(){
    if(empty($_SESSION[$this->cacheKey()])){
      $this->get_static_user_info();
    }
    $pointed_attrributes = SimDb::fetchAll(
        'SELECT to_id, desire, last_seen_day FROM sim.entity_desire
        WHERE world_id = ? AND from_id = ?', [SIM_WORLD_ID, $this->entityId]
    );
    $values = array();
    foreach($pointed_attrributes as $attributes){
        array_push($values, array("from_id" => $this->entityId, "to_id" => $attributes["to_id"], "value" => $attributes["desire"], "last_seen_day" => $attributes["last_seen_day"]));
    }
    return $values;
  }

  function get_couple_pointed(){
    if(empty($_SESSION[$this->cacheKey()])){
      $this->get_static_user_info();
    }
    $pointed_attrributes = SimDb::fetchAll(
        'SELECT to_id, commitment, satisfaction, trust, suspicion, days_together, last_seen_day FROM sim.entity_couple
        WHERE world_id = ? AND from_id = ?', [SIM_WORLD_ID, $this->entityId]
    );
    $values = array();
    foreach($pointed_attrributes as $attributes){
        array_push($values, array("from_id" => $this->entityId,
        "to_id" => $attributes["to_id"],
        "satisfaction" => $attributes["satisfaction"],
        "commitment" => $attributes["commitment"],
        "days_together" => $attributes["days_together"],
        "suspicion" => $attributes["suspicion"],
        "trust" => $attributes["trust"],
        "last_seen_day" => $attributes["last_seen_day"]));
    }
    return $values;
  }

  function get_mental_model(){
    if(empty($_SESSION[$this->cacheKey()])){
      $this->get_static_user_info();
    }
    $pointed_attrributes = SimDb::fetchAll(
        'SELECT
        from_id,
        to_id,
        perceived_extraversion,
        perceived_agreeableness,
        perceived_neuroticism,
        estimated_happiness,
        estimated_anger,
        estimated_stress,
        trust_level,
        predictability,
        perceived_intentionality,
        confidence,
        last_observed_day,
        last_interaction_day,
        last_interaction_outcome,
        faked_by_them,
        fake_trust_gain,
        last_seen_day
     FROM sim.entity_mental_model
        WHERE world_id = ? AND from_id = ?', [SIM_WORLD_ID, $this->entityId]
    );
    return $pointed_attrributes;
  }

  function get_light_statistics(){
      $users = SimDb::fetchOne(
          'SELECT
          age,
          life_stage,
          health,
          hunger,
          food_store,
          hygiene,
          fatigue_level,
          injury_level,
          sleep_pressure,
          sleep_quality,
          antibody,
          disease_type,
          base_immunity,
          happiness,
          stress,
          stress_baseline,
          mental_health,
          loneliness,
          boredom,
          general_anger,
          esteem,
          sense_of_purpose,
          grief_intensity,
          social_deficit,
          days_without_social_action,
          meeting_count,
          -- QI: what this mind became, the ceiling it was born with, and the
          -- schooling that closed (or failed to close) the gap between them.
          qi,
          qi_potential,
          school_years
          FROM sim.entity WHERE world_id = ? AND sim_id = ?',
          [SIM_WORLD_ID, $this->entityId]
      );
      return $users;
  }


  /**
   * Sign-in lives in auth.php now — login_user(), which also starts the
   * session and regenerates its id.
   *
   * The version that stood here could not have worked, in three separate ways,
   * and they are worth naming because the first two are easy to write again:
   *
   *   1. password_hash($psswd) omits the required $algo argument, so PHP 8
   *      raises ArgumentCountError before any query runs.
   *   2. Matching `password = <a fresh hash>` in SQL never finds a row. bcrypt
   *      salts every call, so hashing the same passphrase twice gives two
   *      different strings. A hash is verified with password_verify against
   *      the STORED value, never compared in the database.
   *   3. fetchAll returns a list of rows; indexing it with ["user_id"] reads a
   *      column off the list itself.
   *
   * Kept as a delegating shim rather than deleted, so any caller still holding
   * an `entity` gets the working path instead of a fatal.
   */
  function user_login($login, $psswd){
    require_once __DIR__ . '/auth.php';

    $result = login_user($login, $psswd);
    if (!$result['success']) {
      return null;
    }
    $this->entityId = $result['user']['entity_id'];

    return array(
      $result['user']['id'],
      $result['user']['world_id'],
      $result['user']['entity_id'],
    );
  }


  function get_deeper_statistics(){
      $users = SimDb::fetchOne(
          'SELECT
          age,
          life_stage,
          health,
          hunger,
          food_store,
          hygiene,
          fatigue_level,
          injury_level,
          sleep_pressure,
          sleep_quality,
          antibody,
          disease_type,
          base_immunity,
          happiness,
          stress,
          stress_baseline,
          mental_health,
          loneliness,
          boredom,
          general_anger,
          esteem,
          sense_of_purpose,
          grief_intensity,
          social_deficit,
          days_without_social_action,
          meeting_count,
          -- QI: what this mind became, the ceiling it was born with, and the
          -- schooling that closed (or failed to close) the gap between them.
          qi,
          qi_potential,
          school_years
          FROM sim.entity WHERE world_id = ? AND sim_id = ?',
          [SIM_WORLD_ID, $this->entityId]
      );
      return $users;
  }


}
?>
