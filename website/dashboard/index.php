<?php
/**
 * ASHB2 — SPECIMEN PLATE
 *
 * The dashboard as a museum-grade botanical plate: a single specimen, centred,
 * surrounded by dense technical apparatus on absolute black. Swiss grid,
 * hairline rules, very small monospaced type, no glow anywhere.
 *
 * The one rule that keeps it from being decoration: every number, label,
 * annotation and micro-type block on this page is real data out of sim.*.
 * Authentic scientific texture means the texture IS the science — a panel of
 * invented readings would look identical and mean nothing.
 *
 * All PHP runs before a single byte of markup, because session_start() after
 * output is a "headers already sent" warning and a session that never starts.
 */

// __DIR__, not a bare relative path: '../entity.php' resolves against the
// process's working directory, which is wherever the server was launched.
//
// These come BEFORE session_start(): they pull in config.php, which sets the
// session cookie flags with ini_set(), and ini_set on a live session is a
// warning per line. Every other entry point in the app has the same order.
require_once __DIR__ . '/../entity.php';
require_once __DIR__ . '/../world.php';

if (session_status() === PHP_SESSION_NONE) {
    session_start();
}

// ── helpers ─────────────────────────────────────────────────────────────────

function h($v): string { return htmlspecialchars((string)$v, ENT_QUOTES, 'UTF-8'); }

/** A measurement, or the typographic em-dash that means "not recorded". */
function num($v, int $dp = 1): string {
    return ($v === null || $v === '') ? '—' : number_format((float)$v, $dp);
}

function clamp100($v): float {
    $f = (float)$v;
    return $f < 0 ? 0.0 : ($f > 100 ? 100.0 : $f);
}

/** Fixed-width catalogue numbers, the way a real accession register runs. */
function pad($v, int $len = 6): string {
    return str_pad((string)(int)$v, $len, '0', STR_PAD_LEFT);
}

$LIFE_STAGE  = ['INFANT', 'CHILD', 'ADOLESCENT', 'ADULT', 'ELDER'];
$ATTACHMENT  = ['SECURE', 'ANXIOUS', 'AVOIDANT', 'DISORGANIZED'];
$SOCIAL_CLASS = ['SERVILE', 'PLEBEIAN', 'PATRICIAN'];

function enumName(array $table, $i, string $fallback = 'INDET.'): string {
    $i = (int)$i;
    return $table[$i] ?? $fallback;
}

// ── data acquisition ────────────────────────────────────────────────────────
// The simulation database is a separate server written to by a separate
// process. It being down is an ordinary Tuesday, not an error page: the plate
// renders in its VACAT state and says which panel is missing its source.

$online   = SimDb::isAvailable();
$world    = null;
$live     = null;
$identity = null;
$vitals   = null;
$profile  = null;
$edges    = ['social' => [], 'anger' => [], 'desire' => [], 'couple' => [], 'model' => []];
$roster   = [];
$simId    = null;

if ($online) {
    $w = new word();
    $world = $w->get_static_world_info();
    $live  = $w->get_live_world_info();

    // Which specimen: an explicit ?id, else whoever this session was looking
    // at, else the lowest living id so the plate is never empty for a visitor
    // who arrived without a selection.
    $requested = filter_input(INPUT_GET, 'id', FILTER_VALIDATE_INT);
    if ($requested === null || $requested === false) {
        $requested = $_SESSION['plate_specimen'] ?? null;
    }
    if ($requested === null) {
        $requested = SimDb::fetchValue(
            'SELECT sim_id FROM sim.entity WHERE world_id = ? AND alive ORDER BY sim_id LIMIT 1',
            [SIM_WORLD_ID]
        );
    }

    if ($requested !== null) {
        $simId = (int)$requested;
        $_SESSION['plate_specimen'] = $simId;

        $e = new entity();
        $e->entityId = $simId;

        $identity = $e->get_static_user_info();
        $vitals   = $e->get_light_statistics();

        // The class covers vitals and edges. The plate also documents lineage,
        // temperament, standing and the specimen's own words — one more read
        // rather than eight, since they all come off the same row.
        $profile = SimDb::fetchOne(
            'SELECT alive, last_seen_day, birth_year, life_stage, attachment_style,
                    openness, conscientiousness, extraversion, agreeableness, neuroticism,
                    value_family, value_achievement, value_spiritual, value_hedonism,
                    value_collectivism,
                    self_esteem, self_efficacy, self_calibration, self_primary_identity,
                    raw_anger, expressed_anger, suppression_debt,
                    nutritional_status, hydration_level, energy_level,
                    childhood_trauma_score, childhood_nurturing_score, had_secure_attachment,
                    parent1_id, parent2_id, family_id, lineage_depth,
                    tribe_id, religion_id, origin_region_id, social_class,
                    dominance_rank, auctoritas, integrity, cultural_capital,
                    specialization, is_specialist, wealth, pos_x, pos_y,
                    narrative_coherence, dominant_value, self_story,
                    current_goal, last_action, inner_monologue
               FROM sim.entity WHERE world_id = ? AND sim_id = ?',
            [SIM_WORLD_ID, $simId]
        );

        if ($identity) {
            $edges['social'] = $e->get_social_pointed();
            $edges['anger']  = $e->get_anger_pointed();
            $edges['desire'] = $e->get_desire_pointed();
            $edges['couple'] = $e->get_couple_pointed();
            $edges['model']  = $e->get_mental_model();
        }

        // The drawer of neighbouring plates.
        $roster = SimDb::fetchAll(
            'SELECT sim_id, name, age, health FROM sim.entity
              WHERE world_id = ? AND alive AND sim_id <> ?
              ORDER BY abs(sim_id - ?) LIMIT 12',
            [SIM_WORLD_ID, $simId, $simId]
        );
    }
}

$found = ($identity !== null && $vitals !== null);

// ── derived description ─────────────────────────────────────────────────────
// A plate names its specimen. The binomial is built from what the world
// actually recorded, so it changes when the specimen does.

$name   = $found ? ($identity['name'] ?? '') : '';
$stage  = $found ? enumName($LIFE_STAGE, $vitals['life_stage'] ?? -1) : '';
$genus  = 'ANTHROPOS';
$epithet = $name !== '' ? strtolower(preg_replace('/[^A-Za-z]/', '', $name)) : 'indeterminata';
$variety = $found && !empty($profile['specialization'])
    ? strtolower($profile['specialization'])
    : strtolower($stage ?: 'incertae sedis');

$catalogue = $found
    ? sprintf('ASHB·W%02d·E%s', SIM_WORLD_ID, pad($simId, 6))
    : 'ASHB·W--·E------';

$seed = $world['seed'] ?? '';

// The marker strip is a rendering of the specimen's provenance hash — the world
// seed and the entity id — mapped onto four bases. It is a fingerprint drawn as
// a sequence, not sequence data, and the panel says so.
$markers = '';
if ($found) {
    $bases = ['A', 'C', 'G', 'T'];
    foreach (str_split(hash('sha256', $seed . ':' . $simId)) as $nibble) {
        $markers .= $bases[hexdec($nibble) % 4];
    }
}

// ── the plate geometry ──────────────────────────────────────────────────────
// Eight tepals, each one a vital. Petal length is the reading, so the corolla
// is a radar chart wearing a flower's clothes: a starved, sleepless specimen
// draws a visibly lopsided bloom. Nothing here is arbitrary shape-making.

$tepals = [];
if ($found) {
    $tepals = [
        ['a', 'HEALTH',     clamp100($vitals['health'] ?? 0)],
        ['b', 'VIGOUR',     clamp100($profile['energy_level'] ?? 0)],
        ['c', 'NUTRITION',  clamp100($profile['nutritional_status'] ?? 0)],
        ['d', 'HYDRATION',  clamp100($profile['hydration_level'] ?? 0)],
        ['e', 'REST',       clamp100(100 - (float)($vitals['sleep_pressure'] ?? 0))],
        ['f', 'HYGIENE',    clamp100($vitals['hygiene'] ?? 0)],
        ['g', 'AFFECT',     clamp100($vitals['happiness'] ?? 0)],
        ['h', 'COGNITION',  clamp100($vitals['mental_health'] ?? 0)],
    ];
}

// Stress pushes the corolla from bone-white toward dusty magenta; the veins
// stay emerald until the specimen is truly failing. One physiological reading,
// one visible consequence.
$stress   = $found ? clamp100($vitals['stress'] ?? 0) : 0;
$distress = $stress / 100.0;
$petalHue = sprintf('rgb(%d,%d,%d)',
    (int)(232 - 8 * $distress), (int)(230 - 60 * $distress), (int)(225 - 40 * $distress));
$veinHue  = sprintf('rgb(%d,%d,%d)',
    (int)(46 + 120 * $distress), (int)(125 - 40 * $distress), (int)(91 - 20 * $distress));

$edgeTotal = count($edges['social']) + count($edges['anger'])
           + count($edges['desire']) + count($edges['couple']);
$traceCount = max(3, min(14, $edgeTotal));
$pollen     = min(60, count($edges['model']) * 2 + 8);
?>
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title><?= h($catalogue) ?> — Specimen Plate — ASHB2</title>
<style>
/* ─────────────────────────────────────────────────────────────────────────
   Palette. Museum archival, not screen-neon: bone white on absolute black,
   with three accents used sparingly enough to stay expensive. No glow, no
   saturated cyan, no terminal green — those read as sci-fi set dressing and
   the whole point here is that the page looks like it was printed.
   ───────────────────────────────────────────────────────────────────────── */
:root{
  --void:#000000;
  --bone:#e8e6e1;          /* primary text */
  --bone-dim:#8f8d88;      /* secondary */
  --bone-faint:#54524e;    /* captions, micro-type */
  --rule:rgba(232,230,225,.14);
  --rule-strong:rgba(232,230,225,.30);
  --magenta:#c98ba8;       /* dusty pink — affect, warnings */
  --emerald:#4fa882;       /* vasculature, nominal state */
  --violet:#9b7bc4;        /* pistil, cognition, links */
  --copper:#b08356;        /* substrate traces */
  --mono:ui-monospace,"IBM Plex Mono","SFMono-Regular","Roboto Mono","DejaVu Sans Mono",Menlo,Consolas,monospace;
}

*{margin:0;padding:0;box-sizing:border-box}

html{background:var(--void)}
body{
  background:var(--void);
  color:var(--bone);
  font-family:var(--mono);
  font-size:11px;
  line-height:1.5;
  letter-spacing:.02em;
  -webkit-font-smoothing:antialiased;
  padding:clamp(10px,2.2vw,34px);
}

/* Print grain + vignette. Two fixed layers rather than a background-image so
   nothing has to load, and both sit under the content at very low opacity —
   enough to kill the flatness of pure #000 on an OLED, not enough to notice. */
body::before,body::after{content:"";position:fixed;inset:0;pointer-events:none;z-index:0}
body::before{
  background-image:radial-gradient(circle at 50% 50%,rgba(255,255,255,.028) 0 1px,transparent 1px);
  background-size:3px 3px;
  mix-blend-mode:screen;
}
body::after{
  background:radial-gradient(ellipse at 50% 42%,transparent 55%,rgba(0,0,0,.85) 100%);
}
.plate{position:relative;z-index:1;max-width:1680px;margin:0 auto}

/* ── typographic primitives ───────────────────────────────────────────── */
.micro{font-size:8.5px;letter-spacing:.14em;text-transform:uppercase;color:var(--bone-faint)}
.rule{height:1px;background:var(--rule);border:0}
.rule-strong{height:1px;background:var(--rule-strong);border:0}
a{color:var(--violet);text-decoration:none;border-bottom:1px solid rgba(155,123,196,.35)}
a:hover{color:var(--bone);border-bottom-color:var(--bone)}

/* ── masthead ─────────────────────────────────────────────────────────── */
.masthead{display:grid;grid-template-columns:1fr auto;gap:16px;align-items:end;padding-bottom:10px}
.institute{font-size:9px;letter-spacing:.42em;text-transform:uppercase;color:var(--bone-dim)}
.plate-title{font-size:clamp(15px,2.1vw,26px);letter-spacing:.30em;text-transform:uppercase;margin-top:8px}
.plate-sub{font-size:9px;letter-spacing:.2em;text-transform:uppercase;color:var(--bone-faint);margin-top:6px}
.stamp{
  border:1px solid var(--rule-strong);
  padding:6px 10px;
  font-size:8.5px;letter-spacing:.22em;text-transform:uppercase;
  color:var(--bone-dim);text-align:right;white-space:nowrap;
}
.stamp b{display:block;color:var(--bone);letter-spacing:.16em;font-weight:400}
.stamp.alert{border-color:rgba(201,139,168,.5);color:var(--magenta)}

/* ── the grid ─────────────────────────────────────────────────────────── */
.body-grid{
  display:grid;
  grid-template-columns:minmax(0,3fr) minmax(0,6fr) minmax(0,3fr);
  gap:1px;                                   /* the hairlines ARE the gutters */
  background:var(--rule);
  border:1px solid var(--rule);
  margin-top:12px;
}
/* min-width:0 on every level that can hold long content. A grid or flex child
   defaults to a min-content floor, so one unbreakable accession number is
   enough to push the whole page wider than the screen. */
.col{background:var(--void);display:flex;flex-direction:column;gap:1px;min-width:0}
.col > *{background:var(--void);min-width:0}

/* ── panels ───────────────────────────────────────────────────────────── */
.panel{padding:12px 13px 14px;position:relative;transition:background .18s ease}
.panel + .panel{border-top:1px solid var(--rule)}
.panel:hover{background:#050505}
.panel-head{display:flex;align-items:baseline;gap:8px;margin-bottom:9px}
.panel-no{
  font-size:8.5px;letter-spacing:.1em;color:var(--void);
  background:var(--bone-faint);padding:1px 4px;flex:none;
}
.panel-title{font-size:8.5px;letter-spacing:.24em;text-transform:uppercase;color:var(--bone-dim)}
.panel-note{margin-left:auto;font-size:8px;letter-spacing:.12em;color:var(--bone-faint)}

/* ── data rows ────────────────────────────────────────────────────────── */
.rows{display:grid;gap:0}
.row{
  display:grid;grid-template-columns:minmax(0,1fr) auto;gap:10px;align-items:baseline;
  padding:3.5px 0;border-bottom:1px dotted rgba(232,230,225,.10);
}
.row:last-child{border-bottom:0}
.row dt{font-size:9px;letter-spacing:.1em;text-transform:uppercase;color:var(--bone-faint);min-width:0}
.row dd{
  font-size:11px;color:var(--bone);text-align:right;font-variant-numeric:tabular-nums;
  min-width:0;overflow-wrap:anywhere;      /* a long goal or identity wraps, never overflows */
}
.row dd.em{color:var(--magenta)}
.row dd.ok{color:var(--emerald)}
.row dd.vi{color:var(--violet)}

/* Measurement bars: hairline scale, filled to the reading. A gauge, not a
   progress bar — no rounded ends, no gradient, no animation. */
.gauge{display:grid;grid-template-columns:1fr 42px;gap:8px;align-items:center;padding:4px 0}
.gauge-label{font-size:9px;letter-spacing:.08em;text-transform:uppercase;color:var(--bone-faint)}
.gauge-val{font-size:10px;text-align:right;font-variant-numeric:tabular-nums;color:var(--bone)}
.track{grid-column:1/-1;height:3px;background:rgba(232,230,225,.09);position:relative}
.fill{position:absolute;inset:0 auto 0 0;background:var(--bone)}
.fill.ok{background:var(--emerald)}
.fill.warn{background:var(--magenta)}
.fill.vi{background:var(--violet)}
/* Calibration ticks every 25% — the thing that makes it read as an instrument. */
.track::after{
  content:"";position:absolute;inset:0;
  background:repeating-linear-gradient(to right,transparent 0 24.6%,rgba(0,0,0,.9) 24.6% 25%);
}

/* ── central plate ────────────────────────────────────────────────────── */
/* The frame takes the drawing's own height and no more. Stretching it to match
   the instrument columns (which are far taller) opened a void above the corolla
   and crushed the field notes underneath it. */
.specimen{padding:14px 14px 10px;display:flex;flex-direction:column}
.specimen-frame{border:1px solid var(--rule);position:relative}
/* Corner ticks: the registration marks of a printed plate. */
.specimen-frame::before,.specimen-frame::after{
  content:"";position:absolute;width:9px;height:9px;border:1px solid var(--rule-strong);
}
.specimen-frame::before{top:-1px;left:-1px;border-right:0;border-bottom:0}
.specimen-frame::after{bottom:-1px;right:-1px;border-left:0;border-top:0}
.specimen svg{width:100%;height:auto;display:block}
.caption{
  margin-top:10px;font-size:8.5px;letter-spacing:.1em;color:var(--bone-faint);
  display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:2px 14px;
}
.caption b{color:var(--bone-dim);font-weight:400}
.binomial{font-style:italic;letter-spacing:.06em;font-size:13px;color:var(--bone)}
.binomial span{font-style:normal;color:var(--bone-faint);font-size:9px;letter-spacing:.16em}

/* ── micro-type texture blocks ────────────────────────────────────────── */
/* Set at 7px on purpose: at a glance it is texture, and it survives being
   read because every character is a real measurement. */
.microtype{
  font-size:7px;line-height:1.35;letter-spacing:.02em;color:var(--bone-faint);
  max-height:150px;overflow:hidden;white-space:pre;
  -webkit-mask-image:linear-gradient(to bottom,#000 68%,transparent 100%);
          mask-image:linear-gradient(to bottom,#000 68%,transparent 100%);
}
.strip{
  font-size:7.5px;letter-spacing:.24em;color:var(--bone-faint);
  word-break:break-all;line-height:1.7;
}
.strip i{font-style:normal;color:var(--emerald)}
.strip u{text-decoration:none;color:var(--violet)}

/* ── roster ───────────────────────────────────────────────────────────── */
/* Hairlines on the items, not a ruled background showing through the gaps:
   auto-fill leaves phantom cells on the last row, and those were rendering as
   a grey slab where nothing exists. */
.roster{display:grid;grid-template-columns:repeat(auto-fill,minmax(72px,1fr));gap:1px;background:transparent}
.roster a{
  background:var(--void);padding:5px 6px;border:1px solid var(--rule);display:block;
  font-size:8.5px;letter-spacing:.06em;color:var(--bone-dim);
}
.roster a:hover{background:#0a0a0a;color:var(--bone)}
.roster a b{display:block;color:var(--bone-faint);font-weight:400;font-size:7.5px;letter-spacing:.1em}

/* ── colophon ─────────────────────────────────────────────────────────── */
.colophon{
  margin-top:12px;padding-top:10px;border-top:1px solid var(--rule);
  display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:10px 22px;
  font-size:8px;letter-spacing:.13em;text-transform:uppercase;color:var(--bone-faint);
}
.colophon b{display:block;color:var(--bone-dim);font-weight:400;letter-spacing:.06em;
  text-transform:none;font-size:10px;margin-top:2px}
.scalebar{display:flex;align-items:center;gap:6px;margin-top:4px}
.scalebar i{display:block;height:5px;width:56px;
  background:repeating-linear-gradient(to right,var(--bone-dim) 0 7px,transparent 7px 14px)}

/* ── vacat state ──────────────────────────────────────────────────────── */
.vacat{padding:26px 14px;text-align:center;color:var(--bone-faint)}
.vacat h2{font-size:11px;letter-spacing:.34em;text-transform:uppercase;color:var(--magenta);font-weight:400}
.vacat p{margin-top:8px;font-size:9px;letter-spacing:.08em;line-height:1.8}
.vacat code{color:var(--bone-dim)}

/* ── responsive ───────────────────────────────────────────────────────── */
@media (max-width:1180px){
  .body-grid{grid-template-columns:repeat(2,minmax(0,1fr))}
  .col-plate{grid-column:1/-1;order:-1}
}
@media (max-width:720px){
  /* minmax(0,1fr), never a bare 1fr: the latter's implicit min-content floor
     is what pushed the panels past the right edge of a phone. */
  .body-grid{grid-template-columns:minmax(0,1fr)}
  .col-plate{grid-column:auto}
  .masthead{grid-template-columns:minmax(0,1fr)}
  .stamp{text-align:left;white-space:normal}
  .plate-title{overflow-wrap:anywhere;letter-spacing:.16em}
  .caption{grid-template-columns:minmax(0,1fr)}
}
@media print{
  body{padding:0}
  body::after{display:none}
  .panel:hover{background:transparent}
}
</style>
</head>
<body>
<div class="plate">

  <!-- ── masthead ───────────────────────────────────────────────────── -->
  <header class="masthead">
    <div>
      <div class="institute">Institute for Synthetic Anthropology · Division of Speculative Botany</div>
      <h1 class="plate-title">Specimen Plate <?= h($catalogue) ?></h1>
      <div class="plate-sub">
        Bio-digital organism · substrate-fused · living record ·
        world <?= h($world['label'] ?? 'unregistered') ?>
      </div>
    </div>
    <div class="stamp <?= $online ? '' : 'alert' ?>">
      <?= $online ? 'Archive link' : 'Archive link severed' ?>
      <b><?= $online ? 'ESTABLISHED' : 'NO SOURCE' ?></b>
      <?= h(gmdate('Y-m-d H:i:s')) ?> UTC
    </div>
  </header>
  <hr class="rule-strong">

<?php if (!$found): ?>
  <!-- Nothing to document. Say which link is missing rather than drawing an
       empty flower and letting the visitor guess. -->
  <div class="vacat">
    <h2>Plate Vacat — no specimen on record</h2>
    <?php if (!$online): ?>
      <p>
        The simulation archive is unreachable.<br>
        Set <code>PG_DSN</code> in <code>website/.env</code>, then verify with
        <code>php website/bin/pg_check.php</code>.
      </p>
    <?php elseif (!$world): ?>
      <p>
        The archive is connected but holds no world <code><?= h(SIM_WORLD_ID) ?></code>.<br>
        Run the engine with <code>--db-export bridge/spool</code>, then
        <code>python scripts/db_spool_loader.py</code>.
      </p>
    <?php else: ?>
      <p>
        World <b><?= h($world['label']) ?></b> is on record, but specimen
        <code><?= h((string)$simId) ?></code> is not in it.<br>
        Every entity may have died, or the id may belong to another world.
      </p>
    <?php endif; ?>
  </div>
<?php else: ?>

  <div class="body-grid">

    <!-- ── LEFT COLUMN: identity, provenance, condition ──────────────── -->
    <div class="col">

      <section class="panel">
        <div class="panel-head"><span class="panel-no">01</span>
          <span class="panel-title">Determination</span>
          <span class="panel-note"><?= h($stage) ?></span></div>
        <div class="binomial"><?= h(ucfirst(strtolower($genus))) ?> <?= h($epithet) ?>
          <span>var. <?= h($variety) ?></span></div>
        <dl class="rows" style="margin-top:9px">
          <div class="row"><dt>Vernacular</dt><dd><?= h($name ?: 'unnamed') ?></dd></div>
          <div class="row"><dt>Accession</dt><dd><?= h($catalogue) ?></dd></div>
          <div class="row"><dt>Sex</dt><dd><?= h($identity['sex'] ?? '—') ?></dd></div>
          <div class="row"><dt>Age</dt><dd><?= num($vitals['age'] ?? null, 2) ?> yr</dd></div>
          <div class="row"><dt>Condition</dt>
            <dd class="<?= !empty($profile['alive']) ? 'ok' : 'em' ?>">
              <?= !empty($profile['alive']) ? 'LIVING' : 'DECEASED' ?></dd></div>
          <div class="row"><dt>Last observed</dt><dd>day <?= h($profile['last_seen_day'] ?? '—') ?></dd></div>
        </dl>
      </section>

      <section class="panel">
        <div class="panel-head"><span class="panel-no">02</span>
          <span class="panel-title">Provenance</span></div>
        <dl class="rows">
          <div class="row"><dt>Collected</dt>
            <dd>d<?= h($identity['bday'] ?? '—') ?> · yr <?= h($profile['birth_year'] ?? '—') ?></dd></div>
          <div class="row"><dt>Lineage depth</dt><dd><?= h($profile['lineage_depth'] ?? '—') ?></dd></div>
          <div class="row"><dt>Parent A</dt>
            <dd><?= $profile['parent1_id'] !== null
                  ? '<a href="?id=' . (int)$profile['parent1_id'] . '">E' . h(pad($profile['parent1_id'])) . '</a>'
                  : '—' ?></dd></div>
          <div class="row"><dt>Parent B</dt>
            <dd><?= $profile['parent2_id'] !== null
                  ? '<a href="?id=' . (int)$profile['parent2_id'] . '">E' . h(pad($profile['parent2_id'])) . '</a>'
                  : '—' ?></dd></div>
          <div class="row"><dt>Family cluster</dt><dd><?= h($profile['family_id'] ?? '—') ?></dd></div>
          <div class="row"><dt>Locality</dt><dd>reg <?= h($profile['origin_region_id'] ?? '—') ?></dd></div>
          <div class="row"><dt>Coordinates</dt>
            <dd><?= num($profile['pos_x'] ?? null, 0) ?> / <?= num($profile['pos_y'] ?? null, 0) ?></dd></div>
        </dl>
      </section>

      <section class="panel">
        <div class="panel-head"><span class="panel-no">03</span>
          <span class="panel-title">Social stratum</span></div>
        <dl class="rows">
          <div class="row"><dt>Class</dt>
            <dd><?= h(enumName($SOCIAL_CLASS, $profile['social_class'] ?? -1)) ?></dd></div>
          <div class="row"><dt>Colony</dt><dd>tribe <?= h($profile['tribe_id'] ?? '—') ?></dd></div>
          <div class="row"><dt>Creed</dt><dd>rel <?= h($profile['religion_id'] ?? '—') ?></dd></div>
          <div class="row"><dt>Function</dt>
            <dd><?= h($profile['specialization'] ?: 'undifferentiated') ?></dd></div>
          <div class="row"><dt>Dominance</dt><dd><?= num($profile['dominance_rank'] ?? null, 2) ?></dd></div>
          <div class="row"><dt>Auctoritas</dt><dd><?= num($profile['auctoritas'] ?? null) ?></dd></div>
          <div class="row"><dt>Integrity</dt><dd><?= num($profile['integrity'] ?? null) ?></dd></div>
          <div class="row"><dt>Accumulation</dt><dd><?= num($profile['wealth'] ?? null, 2) ?></dd></div>
        </dl>
      </section>

      <section class="panel">
        <div class="panel-head"><span class="panel-no">04</span>
          <span class="panel-title">Developmental history</span></div>
        <dl class="rows">
          <div class="row"><dt>Attachment</dt>
            <dd class="vi"><?= h(enumName($ATTACHMENT, $profile['attachment_style'] ?? -1)) ?></dd></div>
          <div class="row"><dt>Secure base</dt>
            <dd><?= !empty($profile['had_secure_attachment']) ? 'PRESENT' : 'ABSENT' ?></dd></div>
          <div class="row"><dt>Trauma index</dt>
            <dd class="<?= (float)($profile['childhood_trauma_score'] ?? 0) > 50 ? 'em' : '' ?>">
              <?= num($profile['childhood_trauma_score'] ?? null) ?></dd></div>
          <div class="row"><dt>Nurture index</dt><dd><?= num($profile['childhood_nurturing_score'] ?? null) ?></dd></div>
          <div class="row"><dt>Narrative coherence</dt><dd><?= num($profile['narrative_coherence'] ?? null, 2) ?></dd></div>
          <div class="row"><dt>Dominant value</dt><dd><?= h($profile['dominant_value'] ?: '—') ?></dd></div>
        </dl>
      </section>

    </div>

    <!-- ── CENTRE: the specimen ──────────────────────────────────────── -->
    <div class="col col-plate">
      <section class="panel specimen">
        <div class="panel-head"><span class="panel-no">00</span>
          <span class="panel-title">Habitus · corolla morphometry · substrate interface</span>
          <span class="panel-note">plate 1:1</span></div>

        <div class="specimen-frame">
        <?php
        // ── the drawing ──────────────────────────────────────────────────
        // Polar geometry, computed here rather than hand-drawn: each tepal is
        // one vital, its reach IS the reading. A well specimen opens a full
        // symmetrical corolla; a failing one draws itself lopsided, and no
        // legend is needed to see it.
        $cx = 300; $cy = 268; $rMin = 46; $rMax = 176;
        $n  = count($tepals);
        $svg = '';

        // Substrate: printed-circuit root system. One trace per relational
        // edge the specimen carries, fanning below the stem.
        for ($i = 0; $i < $traceCount; $i++) {
            $t   = $traceCount > 1 ? $i / ($traceCount - 1) : 0.5;
            $x0  = $cx + ($t - 0.5) * 22;
            $x1  = $cx + ($t - 0.5) * 460;
            $y1  = 470 + (($i % 3) * 22);
            $mid = 402 + (($i % 4) * 13);
            // Orthogonal-then-diagonal, the way a board is actually routed.
            $svg .= sprintf(
                '<path d="M%.1f 392 L%.1f %.1f L%.1f %.1f L%.1f %.1f" fill="none" '
                . 'stroke="%s" stroke-width="1" opacity="%.2f"/>',
                $x0, $x0, $mid, $x1 - ($x1 > $cx ? 26 : -26), $y1, $x1, $y1,
                ($i % 3 === 0 ? '#b08356' : 'rgba(176,131,86,.55)'), 0.85 - 0.03 * $i
            );
            $svg .= sprintf('<circle cx="%.1f" cy="%.1f" r="2.6" fill="none" stroke="#b08356" '
                . 'stroke-width="1" opacity=".8"/>', $x1, $y1);
            $svg .= sprintf('<circle cx="%.1f" cy="%.1f" r="1" fill="#b08356" opacity=".8"/>', $x1, $y1);
        }
        // Surface-mount components along the trunk of the substrate.
        for ($i = 0; $i < 6; $i++) {
            $sx = $cx - 58 + $i * 23; $sy = 424 + (($i % 2) * 17);
            $svg .= sprintf('<rect x="%.1f" y="%.1f" width="11" height="5" fill="none" '
                . 'stroke="rgba(176,131,86,.6)" stroke-width="1"/>', $sx, $sy);
        }
        // The die: an integrated circuit at the crown of the root mass, where a
        // bulb would be. This is the fusion the plate is documenting.
        $svg .= '<rect x="' . ($cx - 30) . '" y="384" width="60" height="26" fill="#050505" '
              . 'stroke="rgba(176,131,86,.75)" stroke-width="1"/>';
        for ($i = 0; $i < 7; $i++) {
            $px = $cx - 24 + $i * 8;
            $svg .= sprintf('<line x1="%.1f" y1="384" x2="%.1f" y2="378" stroke="rgba(176,131,86,.6)" '
                . 'stroke-width="1"/>', $px, $px);
            $svg .= sprintf('<line x1="%.1f" y1="410" x2="%.1f" y2="416" stroke="rgba(176,131,86,.6)" '
                . 'stroke-width="1"/>', $px, $px);
        }

        // Stem, with the vascular bundle running through it.
        $svg .= sprintf('<path d="M%d 384 C %d 356, %d 330, %d %d" fill="none" stroke="%s" '
            . 'stroke-width="2.2" opacity=".9"/>', $cx, $cx - 7, $cx + 6, $cx, $cy + 44, $veinHue);
        $svg .= sprintf('<path d="M%d 380 C %d 354, %d 332, %d %d" fill="none" stroke="%s" '
            . 'stroke-width=".6" opacity=".55"/>', $cx, $cx + 5, $cx - 4, $cx, $cy + 48, $petalHue);

        // Corolla.
        foreach ($tepals as $k => [$letter, $label, $value]) {
            $ang  = (-90 + $k * (360 / $n)) * M_PI / 180;
            $len  = $rMin + ($rMax - $rMin) * ($value / 100);
            // Slightly wider than half the sector, so adjacent tepals just
            // overlap and the corolla reads as a bloom rather than a star.
            $half = deg2rad(360 / $n / 2) * 1.12;

            $tipX = $cx + cos($ang) * $len;
            $tipY = $cy + sin($ang) * $len;
            $c1X  = $cx + cos($ang - $half) * $len * 0.66;
            $c1Y  = $cy + sin($ang - $half) * $len * 0.66;
            $c2X  = $cx + cos($ang + $half) * $len * 0.66;
            $c2Y  = $cy + sin($ang + $half) * $len * 0.66;

            // Tepal body: two mirrored curves from the receptacle to the tip.
            $svg .= sprintf(
                '<path d="M%.1f %.1f Q%.1f %.1f %.1f %.1f Q%.1f %.1f %.1f %.1f Z" '
                . 'fill="%s" fill-opacity=".07" stroke="%s" stroke-width=".9" stroke-opacity=".72"/>',
                $cx, $cy, $c1X, $c1Y, $tipX, $tipY, $c2X, $c2Y, $cx, $cy,
                $petalHue, $petalHue
            );
            // Midrib and two laterals — the vasculature, in emerald.
            $svg .= sprintf('<line x1="%d" y1="%d" x2="%.1f" y2="%.1f" stroke="%s" '
                . 'stroke-width=".8" opacity=".75"/>', $cx, $cy, $tipX, $tipY, $veinHue);
            foreach ([-0.45, 0.45] as $off) {
                $vx = $cx + cos($ang + $half * $off) * $len * 0.74;
                $vy = $cy + sin($ang + $half * $off) * $len * 0.74;
                $svg .= sprintf('<line x1="%d" y1="%d" x2="%.1f" y2="%.1f" stroke="%s" '
                    . 'stroke-width=".45" opacity=".42"/>', $cx, $cy, $vx, $vy, $veinHue);
            }
            // Leader line to the annotation, in the manner of a botanical plate.
            $lx = $cx + cos($ang) * ($len + 16);
            $ly = $cy + sin($ang) * ($len + 16);
            $ex = $cx + cos($ang) * ($len + 34);
            $ey = $cy + sin($ang) * ($len + 34);
            $anchor = (cos($ang) < -0.25) ? 'end' : ((cos($ang) > 0.25) ? 'start' : 'middle');
            $tx = $ex + (cos($ang) > 0.25 ? 5 : (cos($ang) < -0.25 ? -5 : 0));
            $svg .= sprintf('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="rgba(232,230,225,.28)" '
                . 'stroke-width=".6"/>', $lx, $ly, $ex, $ey);
            $svg .= sprintf(
                '<text x="%.1f" y="%.1f" text-anchor="%s" font-size="7.5" letter-spacing="1.2" '
                . 'fill="#8f8d88">%s. %s <tspan fill="#54524e">%s</tspan></text>',
                $tx, $ey + 2.5, $anchor, $letter, $label, number_format($value, 0)
            );
        }

        // Receptacle and pistil. Pollen count follows how many other minds this
        // specimen carries a model of — cognition made visible as fertility.
        $svg .= sprintf('<circle cx="%d" cy="%d" r="30" fill="#050505" stroke="%s" '
            . 'stroke-width=".8" opacity=".9"/>', $cx, $cy, $veinHue);
        $svg .= sprintf('<circle cx="%d" cy="%d" r="19" fill="none" stroke="#9b7bc4" '
            . 'stroke-width="1" opacity=".85"/>', $cx, $cy);
        mt_srand(($simId ?? 0) + 977);   // same specimen, same pollen, every load
        for ($i = 0; $i < $pollen; $i++) {
            $a = mt_rand(0, 6283) / 1000.0;
            $r = sqrt(mt_rand(0, 1000) / 1000.0) * 17;
            $svg .= sprintf('<circle cx="%.1f" cy="%.1f" r="%.2f" fill="#9b7bc4" opacity="%.2f"/>',
                $cx + cos($a) * $r, $cy + sin($a) * $r, 0.6 + ($i % 3) * 0.35,
                0.45 + ($i % 4) * 0.13);
        }
        mt_srand();

        // Scale reference and plate mark, along the TOP margin. They belong at
        // the foot of a plate by convention, but that is exactly where this
        // drawing's root traces fan out — the rule and its caption were being
        // crossed by copper. The head of the plate is empty and clear.
        $svg .= '<line x1="40" y1="34" x2="140" y2="34" stroke="#8f8d88" stroke-width="1"/>';
        for ($i = 0; $i <= 4; $i++) {
            $svg .= sprintf('<line x1="%d" y1="34" x2="%d" y2="29" stroke="#8f8d88" '
                . 'stroke-width="1"/>', 40 + $i * 25, 40 + $i * 25);
        }
        $svg .= '<text x="40" y="24" font-size="7" letter-spacing="1.4" fill="#54524e">'
              . '0 —————— 40 MM (NOMINAL)</text>';
        $svg .= '<text x="560" y="24" font-size="7" letter-spacing="1.4" fill="#54524e" '
              . 'text-anchor="end">FIG. 1 · HABITUS</text>';
        ?>
        <svg viewBox="0 0 600 530" role="img"
             aria-label="Specimen plate for <?= h($name) ?>: corolla with eight tepals sized by vital signs, fused to a printed-circuit root system.">
          <?= $svg ?>
        </svg>
        </div>

        <div class="caption">
          <div><b>FIG. 1</b> Habitus, dorsal aspect. Corolla radius per tepal = recorded value, 0–100.</div>
          <div><b>VASCULATURE</b> Hue tracks distress (<?= num($stress, 0) ?> / 100).</div>
          <div><b>SUBSTRATE</b> <?= h($traceCount) ?> traces = relational edges borne.</div>
          <div><b>PISTIL</b> <?= h(count($edges['model'])) ?> models of other minds.</div>
        </div>
      </section>

      <section class="panel">
        <div class="panel-head"><span class="panel-no">09</span>
          <span class="panel-title">Field notes · verbatim</span>
          <span class="panel-note">specimen's own record</span></div>
        <dl class="rows">
          <div class="row"><dt>Current goal</dt><dd><?= h($profile['current_goal'] ?: 'none declared') ?></dd></div>
          <div class="row"><dt>Last action</dt><dd><?= h($profile['last_action'] ?: '—') ?></dd></div>
          <div class="row"><dt>Self-concept</dt><dd><?= h($profile['self_primary_identity'] ?: '—') ?></dd></div>
        </dl>
        <?php if (!empty($profile['inner_monologue'])): ?>
          <p style="margin-top:9px;font-size:10px;line-height:1.75;color:var(--bone);
                    border-left:1px solid var(--violet);padding-left:10px">
            <?= h($profile['inner_monologue']) ?>
          </p>
        <?php endif; ?>
        <?php if (!empty($profile['self_story'])): ?>
          <p style="margin-top:8px;font-size:9px;line-height:1.7;color:var(--bone-dim)">
            <span class="micro">Narrative identity —</span> <?= h($profile['self_story']) ?>
          </p>
        <?php endif; ?>
      </section>
    </div>

    <!-- ── RIGHT COLUMN: instrumentation ─────────────────────────────── -->
    <div class="col">

      <section class="panel">
        <div class="panel-head"><span class="panel-no">05</span>
          <span class="panel-title">Morphometrics</span>
          <span class="panel-note">0–100</span></div>
        <?php
        $bars = [
            ['HEALTH', $vitals['health'] ?? 0, 'ok'],
            ['HUNGER', $vitals['hunger'] ?? 0, 'warn'],
            ['HYGIENE', $vitals['hygiene'] ?? 0, 'ok'],
            ['FATIGUE', $vitals['fatigue_level'] ?? 0, 'warn'],
            ['INJURY', $vitals['injury_level'] ?? 0, 'warn'],
            ['SLEEP PRESSURE', $vitals['sleep_pressure'] ?? 0, 'warn'],
            ['SLEEP QUALITY', $vitals['sleep_quality'] ?? 0, 'ok'],
            ['IMMUNITY', $vitals['base_immunity'] ?? 0, 'ok'],
        ];
        foreach ($bars as [$label, $value, $tone]):
            $v = clamp100($value);
        ?>
          <div class="gauge">
            <span class="gauge-label"><?= h($label) ?></span>
            <span class="gauge-val"><?= num($v, 1) ?></span>
            <span class="track"><span class="fill <?= $tone ?>" style="width:<?= $v ?>%"></span></span>
          </div>
        <?php endforeach; ?>
        <dl class="rows" style="margin-top:8px">
          <div class="row"><dt>Antibody titre</dt><dd><?= num($vitals['antibody'] ?? null, 0) ?></dd></div>
          <div class="row"><dt>Pathogen</dt>
            <dd class="<?= $vitals['disease_type'] !== null ? 'em' : 'ok' ?>">
              <?= $vitals['disease_type'] !== null
                    ? 'TYPE ' . h($vitals['disease_type']) : 'NONE DETECTED' ?></dd></div>
          <div class="row"><dt>Food store</dt><dd><?= num($vitals['food_store'] ?? null, 2) ?></dd></div>
        </dl>
      </section>

      <section class="panel">
        <div class="panel-head"><span class="panel-no">06</span>
          <span class="panel-title">Spectral profile · temperament</span></div>
        <?php
        $five = [
            ['OPENNESS', $profile['openness'] ?? 0],
            ['CONSCIENTIOUSNESS', $profile['conscientiousness'] ?? 0],
            ['EXTRAVERSION', $profile['extraversion'] ?? 0],
            ['AGREEABLENESS', $profile['agreeableness'] ?? 0],
            ['NEUROTICISM', $profile['neuroticism'] ?? 0],
        ];
        foreach ($five as [$label, $value]):
            $v = clamp100($value);
        ?>
          <div class="gauge">
            <span class="gauge-label"><?= h($label) ?></span>
            <span class="gauge-val"><?= num($v, 1) ?></span>
            <span class="track"><span class="fill vi" style="width:<?= $v ?>%"></span></span>
          </div>
        <?php endforeach; ?>
        <hr class="rule" style="margin:10px 0 8px">
        <dl class="rows">
          <div class="row"><dt>Family</dt><dd><?= num($profile['value_family'] ?? null, 0) ?></dd></div>
          <div class="row"><dt>Achievement</dt><dd><?= num($profile['value_achievement'] ?? null, 0) ?></dd></div>
          <div class="row"><dt>Spiritual</dt><dd><?= num($profile['value_spiritual'] ?? null, 0) ?></dd></div>
          <div class="row"><dt>Hedonic</dt><dd><?= num($profile['value_hedonism'] ?? null, 0) ?></dd></div>
          <div class="row"><dt>Collective</dt><dd><?= num($profile['value_collectivism'] ?? null, 0) ?></dd></div>
        </dl>
      </section>

      <section class="panel">
        <div class="panel-head"><span class="panel-no">07</span>
          <span class="panel-title">Affective assay</span></div>
        <dl class="rows">
          <div class="row"><dt>Happiness</dt><dd><?= num($vitals['happiness'] ?? null) ?></dd></div>
          <div class="row"><dt>Stress</dt>
            <dd class="<?= $stress > 70 ? 'em' : '' ?>"><?= num($stress) ?></dd></div>
          <div class="row"><dt>Baseline stress</dt><dd><?= num($vitals['stress_baseline'] ?? null) ?></dd></div>
          <div class="row"><dt>Mental health</dt>
            <dd class="<?= (float)($vitals['mental_health'] ?? 100) < 25 ? 'em' : '' ?>">
              <?= num($vitals['mental_health'] ?? null) ?></dd></div>
          <div class="row"><dt>Loneliness</dt><dd><?= num($vitals['loneliness'] ?? null) ?></dd></div>
          <div class="row"><dt>Boredom</dt><dd><?= num($vitals['boredom'] ?? null) ?></dd></div>
          <div class="row"><dt>Grief intensity</dt>
            <dd class="<?= (float)($vitals['grief_intensity'] ?? 0) > 0.3 ? 'em' : '' ?>">
              <?= num($vitals['grief_intensity'] ?? null, 3) ?></dd></div>
          <div class="row"><dt>Esteem</dt><dd><?= num($vitals['esteem'] ?? null) ?></dd></div>
          <div class="row"><dt>Sense of purpose</dt><dd><?= num($vitals['sense_of_purpose'] ?? null) ?></dd></div>
          <div class="row"><dt>Raw / expressed anger</dt>
            <dd><?= num($profile['raw_anger'] ?? null, 0) ?> / <?= num($profile['expressed_anger'] ?? null, 0) ?></dd></div>
          <div class="row"><dt>Suppression debt</dt><dd><?= num($profile['suppression_debt'] ?? null, 2) ?></dd></div>
          <div class="row"><dt>Social deficit</dt><dd><?= num($vitals['social_deficit'] ?? null, 2) ?></dd></div>
          <div class="row"><dt>Days without contact</dt><dd><?= h($vitals['days_without_social_action'] ?? '—') ?></dd></div>
        </dl>
      </section>

      <section class="panel">
        <div class="panel-head"><span class="panel-no">08</span>
          <span class="panel-title">Relational topology</span>
          <span class="panel-note"><?= h($edgeTotal) ?> edges</span></div>
        <dl class="rows">
          <div class="row"><dt>Social bonds</dt><dd class="ok"><?= h(count($edges['social'])) ?></dd></div>
          <div class="row"><dt>Antagonisms</dt><dd class="em"><?= h(count($edges['anger'])) ?></dd></div>
          <div class="row"><dt>Desires</dt><dd><?= h(count($edges['desire'])) ?></dd></div>
          <div class="row"><dt>Pair bonds</dt><dd class="vi"><?= h(count($edges['couple'])) ?></dd></div>
          <div class="row"><dt>Models of others</dt><dd class="vi"><?= h(count($edges['model'])) ?></dd></div>
          <div class="row"><dt>Encounters logged</dt><dd><?= h($vitals['meeting_count'] ?? '—') ?></dd></div>
        </dl>
        <?php foreach ($edges['couple'] as $c): ?>
          <hr class="rule" style="margin:9px 0 7px">
          <div class="micro">Pair bond · specimen E<?= h(pad($c['to_id'])) ?></div>
          <dl class="rows">
            <div class="row"><dt>Commitment</dt><dd><?= num($c['commitment'], 0) ?></dd></div>
            <div class="row"><dt>Satisfaction</dt><dd><?= num($c['satisfaction'], 0) ?></dd></div>
            <div class="row"><dt>Trust / suspicion</dt>
              <dd><?= num($c['trust'], 0) ?> / <?= num($c['suspicion'], 0) ?></dd></div>
            <div class="row"><dt>Duration</dt><dd><?= h($c['days_together']) ?> d</dd></div>
          </dl>
        <?php endforeach; ?>
      </section>

    </div>
  </div>

  <!-- ── lower register: micro-type apparatus ──────────────────────────── -->
  <div class="body-grid" style="margin-top:1px">
    <div class="col">
      <section class="panel">
        <div class="panel-head"><span class="panel-no">10</span>
          <span class="panel-title">Marker strip</span>
          <span class="panel-note">SHA-256 · seed + id</span></div>
        <p class="strip"><?php
          // Rendered as bases so it reads as a sequence, but it is a
          // provenance fingerprint, not sequence data — the panel note says
          // exactly what it is rather than letting it imply biology it has not
          // got. Two worlds, two strips; the same specimen, always this one.
          $i = 0;
          foreach (str_split($markers, 4) as $quad) {
              $cls = ($i % 7 === 0) ? 'i' : (($i % 5 === 0) ? 'u' : null);
              echo $cls ? "<$cls>" . h($quad) . "</$cls> " : h($quad) . ' ';
              $i++;
          }
        ?></p>
        <div class="micro" style="margin-top:8px">
          Provenance fingerprint · not a genome · derived from world seed <?= h(substr((string)$seed, 0, 10)) ?>…
        </div>
      </section>
    </div>

    <div class="col col-plate">
      <section class="panel">
        <div class="panel-head"><span class="panel-no">11</span>
          <span class="panel-title">Observation register · perceived others</span>
          <span class="panel-note">as recorded by the specimen</span></div>
        <div class="microtype"><?php
          // Deliberately 7px: at a glance this is the texture of a dense
          // laboratory table. Every column is a real reading off
          // sim.entity_mental_model, so it survives being read closely.
          printf("%-8s %6s %6s %6s %6s %6s %6s %6s %-9s\n",
                 'TARGET', 'TRUST', 'PRED', 'INTENT', 'CONF', 'EST.HAP', 'EST.ANG', 'EST.STR', 'OUTCOME');
          echo str_repeat('·', 73), "\n";   // exactly the width of the format above
          foreach (array_slice($edges['model'], 0, 26) as $m) {
              printf("E%-7s %6.2f %6.2f %6.2f %6.2f %6.1f %6.1f %6.1f %-9s%s\n",
                  pad($m['to_id']),
                  (float)$m['trust_level'], (float)$m['predictability'],
                  (float)$m['perceived_intentionality'], (float)$m['confidence'],
                  (float)$m['estimated_happiness'], (float)$m['estimated_anger'],
                  (float)$m['estimated_stress'],
                  // ASCII placeholder, not an em-dash: printf pads by BYTES,
                  // and a 3-byte glyph counted as one column would knock this
                  // row out of alignment with every other.
                  substr((string)($m['last_interaction_outcome'] ?: '-'), 0, 9),
                  !empty($m['faked_by_them']) ? '  [DECEPTION SUSPECTED]' : '');
          }
          if (!$edges['model']) echo "no models of other minds on record\n";
        ?></div>
      </section>
    </div>

    <div class="col">
      <section class="panel">
        <div class="panel-head"><span class="panel-no">12</span>
          <span class="panel-title">Adjacent plates</span>
          <span class="panel-note">same collection</span></div>
        <?php if ($roster): ?>
          <div class="roster">
            <?php foreach ($roster as $r): ?>
              <a href="?id=<?= (int)$r['sim_id'] ?>">
                <?= h($r['name'] ?: 'unnamed') ?>
                <b>E<?= h(pad($r['sim_id'])) ?> · <?= num($r['age'], 0) ?>y</b>
              </a>
            <?php endforeach; ?>
          </div>
        <?php else: ?>
          <div class="micro">no other living specimens on record</div>
        <?php endif; ?>
      </section>
    </div>
  </div>
<?php endif; ?>

  <!-- ── colophon ───────────────────────────────────────────────────── -->
  <footer class="colophon">
    <div>World<b><?= h($world['label'] ?? '—') ?></b></div>
    <div>Seed<b><?= h($seed !== '' ? $seed : '—') ?></b></div>
    <div>Archive day<b><?= h($live['last_day'] ?? '—') ?></b></div>
    <div>Living population<b><?= h($live['population'] ?? '—') ?></b></div>
    <div>Interred<b><?= h($live['dead'] ?? '—') ?></b></div>
    <div>Last accession<b><?= h($live['last_push_at'] ?? 'never') ?></b></div>
    <div>Session<b><?= h(substr(session_id(), 0, 12) ?: '—') ?></b></div>
    <div>
      Plate scale
      <div class="scalebar"><i></i><span>10 mm</span></div>
    </div>
  </footer>
  <div class="micro" style="margin-top:14px;line-height:1.9">
    ASHB2 · Artificial Simulation of Human Behavior · the world is advanced by the C++ engine and
    written to this archive by scripts/db_spool_loader.py. This page observes; it does not steer.
  </div>
</div>

</body>
</html>
