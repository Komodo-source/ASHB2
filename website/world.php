<?php
require_once __DIR__ . '/sim_db.php';

class word{
    // Cached in the session because none of it changes while a world runs:
    // label, seed and started_at are fixed when the world is created.
    function get_static_world_info(){
    if (session_status() === PHP_SESSION_NONE) {
        session_start();          // $_SESSION does not exist until this runs
    }
    $data_cached = $_SESSION['static_world_info'] ?? null;
    if($data_cached){ //if world data are already cache just display them
      return $data_cached;
    }
    // A named world, not "the first row": the engine writes one row per
    // --world-id, so SELECT * with no WHERE returns whichever row the planner
    // happens to hand back as soon as a second world exists.
    $world = SimDb::fetchOne(
        'SELECT world_id, label, seed, started_at
           FROM sim.world WHERE world_id = ?',
        [SIM_WORLD_ID]
    );
    if(!$world){
        // No world row yet — the loader has never run, or never against this
        // database. Not an error: there is simply nothing to show, and the
        // caller decides how to say so.
        return null;
    }
        $_SESSION['static_world_info']["world_id"] = $world["world_id"];
        $_SESSION['static_world_info']["label"] = $world["label"];
        $_SESSION['static_world_info']["seed"] = $world["seed"];
        $_SESSION['static_world_info']["started_at"] = $world["started_at"];
        return $_SESSION['static_world_info'];
    }

    // Deliberately NOT cached: this is the part the engine moves. last_day is
    // the high-water mark the loader commits with every chunk, so it is also
    // how you tell a live world from a stopped one.
    function get_live_world_info(){
        return SimDb::fetchOne(
            'SELECT w.last_day,
                    w.last_push_at,
                    (SELECT count(*) FROM sim.entity e
                      WHERE e.world_id = w.world_id AND e.alive) AS population,
                    (SELECT count(*) FROM sim.entity e
                      WHERE e.world_id = w.world_id AND NOT e.alive) AS dead
               FROM sim.world w WHERE w.world_id = ?',
            [SIM_WORLD_ID]
        );
    }

    // Peoples ranked by the thing this world spends universities on. There is
    // no sim.tribe table — the engine exports entities and their tribe_id, and
    // nothing else about a tribe — so a people's mind is aggregated from its
    // living members here rather than read from a row. `elite_qi` is the top
    // decile approximated by the single highest member, which is what a group
    // this size has instead of a decile.
    function get_tribe_minds($limit = 20){
        return SimDb::fetchAll(
            'SELECT tribe_id,
                    count(*)                         AS members,
                    round(avg(qi)::numeric, 1)       AS mean_qi,
                    round(max(qi)::numeric, 1)       AS elite_qi,
                    round(avg(school_years)::numeric, 1) AS mean_school_years,
                    count(*) FILTER (WHERE school_years >= 4) AS schooled
               FROM sim.entity
              WHERE world_id = ? AND alive AND tribe_id >= 0 AND qi IS NOT NULL
              GROUP BY tribe_id
             HAVING count(*) >= 3
              ORDER BY mean_qi DESC
              LIMIT ?',
            [SIM_WORLD_ID, $limit]
        );
    }
}
?>
