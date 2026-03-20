import csv
import os
import threading
import json
import re
from datetime import datetime
from ollama import chat
from ollama import ChatResponse

# Base folder where all simulation logs and output should live (src/data)
BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

prompt = """
Context:
- This is a multi-agent simulation modeling human-like entities with needs, emotions, relationships, and behaviors
- Entities have attributes: health, happiness, stress, loneliness, personality traits (extraversion, agreeableness, conscientiousness, neuroticism, openness)
- Simulation includes: disease spread, social interactions, reproduction, movement, aging, and death
- Time progresses in "days" with entities making decisions every few frames
Analysis Requirements:
1. Executive Summary (2-3 paragraphs)
2. Population Analysis
3. Disease and Health Dynamics
4. Social Dynamics
5. Behavioral Patterns
6. Key Events Timeline
7. Conclusions and Insights
Report Style:
- Write in formal scientific/academic tone
- Use clear headings and subheadings
- Focus on patterns, trends, and insights rather than listing every individual event
"""

HTML_SHELL = """<!DOCTYPE html>
<html lang=\"en\">
<head>
  <meta charset=\"UTF-8\">
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">
  <title>ASHB2 — Simulation Report</title>
  <link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">
  <link href=\"https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700&family=Roboto+Slab:wght@300;400;600&display=swap\" rel=\"stylesheet\">
  <script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>
  <script src=\"https://unpkg.com/vis-network/standalone/umd/vis-network.min.js\"></script>

  <style>
    :root {
      --bg: #070a11;
      --panel: rgba(255, 255, 255, 0.06);
      --panel-strong: rgba(255, 255, 255, 0.1);
      --text: #e7edf8;
      --muted: rgba(231, 237, 248, 0.7);
      --accent: #5bd3ff;
      --accent2: #7c57ff;
      --border: rgba(255, 255, 255, 0.12);
      --shadow: 0 18px 55px rgba(0, 0, 0, 0.35);
      --radius: 16px;
      --code: #0f172a;
    }

    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: 'Roboto Slab', serif;
      background: radial-gradient(circle at 30% 20%, rgba(91, 211, 255, 0.12), transparent 60%),
                  radial-gradient(circle at 70% 80%, rgba(124, 87, 255, 0.18), transparent 55%),
                  var(--bg);
      color: var(--text);
      line-height: 1.6;
    }

    header {
      padding: 2.5rem 2rem 1rem;
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      gap: 1rem;
    }

    .brand {
      font-size: 2rem;
      font-weight: 700;
      letter-spacing: 0.05em;
      text-transform: uppercase;
      line-height: 1;
    }

    .brand span {
      display: block;
      font-size: 0.9rem;
      font-weight: 400;
      letter-spacing: 0.15em;
      margin-top: 0.25rem;
      color: var(--muted);
    }

    .meta {
      display: flex;
      flex-wrap: wrap;
      gap: 1rem;
      font-size: 0.85rem;
      color: var(--muted);
      align-items: center;
    }

    .meta div {
      background: rgba(255,255,255,0.08);
      padding: 0.55rem 0.8rem;
      border-radius: 999px;
      border: 1px solid rgba(255,255,255,0.12);
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, 'Liberation Mono', 'Courier New', monospace;
    }

    .actions {
      display: flex;
      gap: 0.6rem;
    }

    .btn {
      border: 1px solid rgba(255,255,255,0.18);
      background: rgba(255,255,255,0.05);
      color: var(--text);
      cursor: pointer;
      border-radius: 999px;
      padding: 0.6rem 1rem;
      font-weight: 600;
      transition: transform 0.12s ease, background 0.12s ease, border-color 0.12s ease;
    }

    .btn:hover {
      background: rgba(255,255,255,0.12);
      border-color: rgba(91, 211, 255, 0.8);
      transform: translateY(-1px);
    }

    main {
      padding: 1rem 2rem 3rem;
      max-width: 1460px;
      margin: 0 auto;
      display: grid;
      grid-template-columns: 1.3fr 0.7fr;
      gap: 1.75rem;
    }

    @media (max-width: 1100px) {
      main { grid-template-columns: 1fr; }
    }

    .panel {
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      box-shadow: var(--shadow);
      padding: 1.5rem;
      overflow: hidden;
    }

    .panel h2 {
      font-size: 1.25rem;
      margin: 0 0 1rem;
      letter-spacing: 0.02em;
      color: var(--accent);
    }

    .panel h3 {
      margin: 1.25rem 0 0.75rem;
      font-size: 1.05rem;
      color: var(--muted);
    }

    .report-body {
      color: rgba(231, 237, 248, 0.92);
      font-size: 0.95rem;
    }

    .logs {
      display: grid;
      grid-template-columns: repeat(1, minmax(0, 1fr));
      gap: 1rem;
    }

    .log-block {
      background: var(--panel-strong);
      border: 1px solid rgba(255,255,255,0.1);
      border-radius: var(--radius);
      overflow: hidden;
      height: 320px;
      display: flex;
      flex-direction: column;
    }

    .log-block .log-header {
      padding: 0.85rem 1rem;
      font-weight: 700;
      text-transform: uppercase;
      font-size: 0.75rem;
      letter-spacing: 0.12em;
      border-bottom: 1px solid rgba(255,255,255,0.12);
      background: rgba(0, 0, 0, 0.2);
    }

    .log-block pre {
      padding: 1rem;
      margin: 0;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, 'Liberation Mono', 'Courier New', monospace;
      font-size: 0.75rem;
      line-height: 1.3;
      overflow: auto;
      height: 100%;
      white-space: pre-wrap;
      word-break: break-word;
      color: rgba(231, 237, 248, 0.9);
      background: transparent;
    }

    .grid-2 {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 1rem;
    }

    @media (max-width: 850px) {
      .grid-2 { grid-template-columns: 1fr; }
    }

    .chart-row {
      display: grid;
      grid-template-columns: 1fr;
      gap: 1rem;
    }

    .chart-container {
      padding: 1rem;
      border-radius: var(--radius);
      background: rgba(255,255,255,0.07);
      border: 1px solid rgba(255,255,255,0.1);
      box-shadow: 0 14px 38px rgba(0,0,0,0.28);
    }

    .chart-container h3 {
      margin: 0 0 0.75rem;
      font-size: 1rem;
      color: var(--accent);
    }

    .network {
      height: 420px;
      border-radius: var(--radius);
      border: 1px solid rgba(255,255,255,0.14);
      background: rgba(0,0,0,0.15);
    }
  </style>
</head>
<body>
<header>
  <div class="brand">ASHB2 <span>Simulation Report</span></div>
  <div class="meta">
    <div>Started: {SIM_START}</div>
    <div>Ended: {SIM_END}</div>
    <div>Duration: {SIM_DURATION}</div>
    <div>Ticks: {SIM_TICKS}</div>
  </div>
  <div class="actions">
    <button id="download-report" class="btn">Download HTML</button>
    <button id="download-data" class="btn">Export JSON</button>
  </div>
</header>

<main>
  <section class="panel">
    <h2>AI Summary</h2>
    <div class="report-body">{AI_SUMMARY}</div>
  </section>

  <section class="panel">
    <h2>Simulation Metrics</h2>
    <div class="chart-row" id="charts-slot"></div>
  </section>

  <section class="panel">
    <h2>Action Frequency</h2>
    <canvas id="actionChart" style="width:100%;height:260px"></canvas>
  </section>

  <section class="panel">
    <h2>Relationship Network</h2>
    <div id="relationship-network" class="network"></div>
  </section>

  <section class="panel">
    <h2>Raw Logs</h2>
    <div class="logs">
      <div class="log-block"><div class="log-header">cmd_log.txt</div><pre>{CMD_LOG}</pre></div>
      <div class="log-block"><div class="log-header">actions_log.txt</div><pre>{ACTIONS_LOG}</pre></div>
      <div class="log-block"><div class="log-header">diseases_log.txt</div><pre>{DISEASES_LOG}</pre></div>
      <div class="log-block"><div class="log-header">deaths_log.txt</div><pre>{DEATHS_LOG}</pre></div>
      <div class="log-block"><div class="log-header">movements_log.txt</div><pre>{MOVEMENTS_LOG}</pre></div>
      <div class="log-block"><div class="log-header">births_log.txt</div><pre>{BIRTHS_LOG}</pre></div>
      <div class="log-block"><div class="log-header">events_log.txt</div><pre>{EVENTS_LOG}</pre></div>
      <div class="log-block"><div class="log-header">relationships_log.txt</div><pre>{RELATIONSHIPS_LOG}</pre></div>
    </div>
  </section>
</main>

<script>
  const reportData = {REPORT_JSON};
  const ticks = {CHART_TICKS};
  const attributesData = {CHART_DATA};
  const actionCounts = {ACTION_COUNTS};
  const relationshipNodes = {RELATION_NODES};
  const relationshipEdges = {RELATION_EDGES};

  function downloadBlob(blob, filename) {
    const link = document.createElement('a');
    link.href = URL.createObjectURL(blob);
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    setTimeout(() => {
      URL.revokeObjectURL(link.href);
      link.remove();
    }, 200);
  }

  document.getElementById('download-report').addEventListener('click', () => {
    const html = document.documentElement.outerHTML;
    downloadBlob(new Blob([html], { type: 'text/html' }), 'ashb2-report.html');
  });

  document.getElementById('download-data').addEventListener('click', () => {
    downloadBlob(new Blob([JSON.stringify(reportData, null, 2)], { type: 'application/json' }), 'ashb2-data.json');
  });

  // Build per-attribute charts
  const chartsSlot = document.getElementById('charts-slot');
  const colorPalette = ['#5bd3ff', '#7c57ff', '#ff6bbb', '#1ce1b7', '#ffbd59', '#a7ff39', '#f33f7a', '#3f8cff'];
  let colorIndex = 0;

  for (const [attrName, agentsArray] of Object.entries(attributesData)) {
    const container = document.createElement('div');
    container.className = 'chart-container';
    container.innerHTML = `<h3>${attrName} Over Time</h3><canvas id="chart-${attrName}"></canvas>`;
    chartsSlot.appendChild(container);

    const datasets = agentsArray.map((agentData, index) => {
      const hue = (colorIndex * 40 + index * 23) % 360;
      return {
        label: `Agent ${index}`,
        data: agentData,
        borderColor: `hsl(${hue}, 85%, 60%)`,
        backgroundColor: `hsla(${hue}, 85%, 60%, 0.2)`,
        borderWidth: 1,
        pointRadius: 0,
        tension: 0.22,
      };
    });

    new Chart(document.getElementById(`chart-${attrName}`), {
      type: 'line',
      data: { labels: ticks, datasets },
      options: {
        responsive: true,
        plugins: { legend: { display: false }, tooltip: { mode: 'index', intersect: false } },
        scales: {
          x: { display: true, title: { display: true, text: 'Tick' } },
          y: { display: true, title: { display: true, text: attrName } }
        }
      }
    });

    colorIndex++;
  }

  // Action frequency chart
  (function renderActionChart() {
    const ctx = document.getElementById('actionChart').getContext('2d');
    const labels = Object.keys(actionCounts).sort((a, b) => actionCounts[b] - actionCounts[a]);
    const data = labels.map(l => actionCounts[l]);

    new Chart(ctx, {
      type: 'bar',
      data: {
        labels,
        datasets: [{
          label: 'Action Count',
          data,
          backgroundColor: labels.map((_, idx) => colorPalette[idx % colorPalette.length]),
        }]
      },
      options: {
        responsive: true,
        plugins: { legend: { display: false }, tooltip: { mode: 'index', intersect: false } },
        scales: {
          x: { ticks: { color: '#ccd6f6' } },
          y: { ticks: { color: '#ccd6f6' } }
        }
      }
    });
  })();

  // Relationship network
  (function renderNetwork() {
    const container = document.getElementById('relationship-network');
    const data = {
      nodes: new vis.DataSet(relationshipNodes),
      edges: new vis.DataSet(relationshipEdges)
    };
    const options = {
      physics: { stabilization: true, barnesHut: { gravitationalConstant: -8000 } },
      edges: { smooth: { enabled: true, type: 'cubicBezier' }, font: { align: 'top' } },
      nodes: { font: { color: 'rgba(231,237,248,0.88)' }, color: { border: '#5bd3ff', background: 'rgba(91, 211, 255, 0.18)' } }
    };
    new vis.Network(container, data, options);
  })();
</script>
</body>
</html>
"""

# Shared dictionary to store results from threads
report_data = {
    "ai_summary": "<i>No analysis generated.</i>",
    "cmd_log": "",
    "actions_log": "",
    "diseases_log": "",
    "deaths_log": "",
    "movements_log": "",
    "births_log": "",
    "events_log": "",
    "relationships_log": "",
    "sim_start": "",
    "sim_end": "",
    "sim_duration": "",
    "sim_ticks": "",
    "chart_ticks": "[]",
    "chart_data": "{}",
    "action_counts": "{}",
    "relationship_nodes": "[]",
    "relationship_edges": "[]",
    "report_json": "{}"
}

def safe_read(filepath):
    """Safely reads a file (relative to BASE_DIR) and returns its content or an error message."""
    full_path = filepath if os.path.isabs(filepath) else os.path.join(BASE_DIR, filepath)
    try:
        with open(full_path, "r", encoding="utf-8") as f:
            return f.read()
    except Exception as e:
        return f"[File not found or unreadable: {full_path}]"

def log_logs():
    cmd_content = safe_read("../cmd_log.txt")
    report_data["cmd_log"] = cmd_content

    try:
        response: ChatResponse = chat(model='qwen2.5:14b', messages=[
            {
                'role': 'user',
                'content': prompt + "\n\n" + cmd_content,
            },
        ])
        ai_text = str(response.message.content)

        # Convert Markdown headings to HTML
        ai_text = re.sub(r'^### (.+)$', r'<h3>\1</h3>', ai_text, flags=re.MULTILINE)
        ai_text = re.sub(r'^## (.+)$',  r'<h2>\1</h2>', ai_text, flags=re.MULTILINE)
        ai_text = re.sub(r'^# (.+)$',   r'<h2>\1</h2>', ai_text, flags=re.MULTILINE)

        # Wrap bare lines in <p>
        paragraphs = []
        for block in re.split(r'\n{2,}', ai_text):
            block = block.strip()
            if block and not block.startswith('<h'):
                block = f'<p>{block}</p>'
            paragraphs.append(block)

        report_data["ai_summary"] = '\n'.join(paragraphs)
    except Exception as e:
        report_data["ai_summary"] = f"<p>Error generating AI summary: {e}</p>"

def _parse_timestamp_range(log_text: str):
    """Returns (start_datetime, end_datetime) parsed from log timestamps."""
    timestamps = []
    for m in re.finditer(r'^\[([0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2})\]', log_text, flags=re.MULTILINE):
        try:
            timestamps.append(datetime.strptime(m.group(1), "%Y-%m-%d %H:%M:%S"))
        except Exception:
            continue
    if not timestamps:
        return None, None
    return min(timestamps), max(timestamps)


def _format_duration(delta):
    if delta is None:
        return ""
    days = delta.days
    hours, rem = divmod(delta.seconds, 3600)
    minutes, seconds = divmod(rem, 60)
    parts = []
    if days:
        parts.append(f"{days}d")
    if hours:
        parts.append(f"{hours}h")
    if minutes:
        parts.append(f"{minutes}m")
    if seconds or not parts:
        parts.append(f"{seconds}s")
    return ' '.join(parts)


def log_all():
    """Handles reading all secondary logs and parsing CSVs for charts"""
    report_data["deaths_log"] = safe_read("deaths_log.txt")
    report_data["diseases_log"] = safe_read("diseases_log.txt")
    report_data["actions_log"] = safe_read("actions_log.txt")
    report_data["movements_log"] = safe_read("movements_log.txt")
    report_data["births_log"] = safe_read("births_log.txt")
    report_data["events_log"] = safe_read("events_log.txt")
    report_data["relationships_log"] = safe_read("relationships_log.txt")

    # Determine simulation start/end times from events log (fallbacks to other logs)
    start, end = _parse_timestamp_range(report_data["events_log"])
    if start is None or end is None:
        start, end = _parse_timestamp_range(report_data["deaths_log"])
    if start is None or end is None:
        start, end = _parse_timestamp_range(report_data["cmd_log"])

    report_data["sim_start"] = start.strftime("%Y-%m-%d %H:%M:%S") if start else "Unknown"
    report_data["sim_end"] = end.strftime("%Y-%m-%d %H:%M:%S") if end else "Unknown"
    report_data["sim_duration"] = _format_duration(end - start) if start and end else "Unknown"

    # Parse action counts from per-agent action files
    action_counts = {}
    try:
        for i in range(1000):
            filepath = os.path.join(BASE_DIR, f'act_{i}.csv')
            if not os.path.exists(filepath):
                break
            with open(filepath, encoding="utf-8") as f:
                line = f.readline().strip()
                # Find action names preceding ',category:'
                for action in re.findall(r'([A-Za-z0-9 _\-]+)(?=,category:)', line):
                    action = action.strip()
                    if not action:
                        continue
                    action_counts[action] = action_counts.get(action, 0) + 1
    except Exception as e:
        print(f"Error parsing action CSVs: {e}")

    # Parse relationship links for network visualization
    nodes = {}
    edges = []
    for line in report_data["relationships_log"].splitlines():
        m = re.match(r'^\[([^\]]+)\]\s*Relationship:\s*(.+?)\s*\((\d+)\)\s*and\s*(.+?)\s*\((\d+)\)\s*-\s*([^\-\n]+)(?:-\s*(.*))?$', line)
        if not m:
            continue
        timestamp, name1, id1, name2, id2, reltype, details = m.groups()
        id1 = int(id1)
        id2 = int(id2)
        nodes[id1] = name1.strip()
        nodes[id2] = name2.strip()
        title = f"{reltype.strip()}\n{timestamp}"
        if details:
            title += f"\n{details.strip()}"
        edges.append({
            "from": id1,
            "to": id2,
            "label": reltype.strip(),
            "title": title,
        })

    # Data structure for charts: { "AttributeName": [ [agent0_values], [agent1_values], ... ] }
    attr_names = ['Antibody', 'Boredom', 'Anger', 'Happiness', 'Health', 'Hygiene', 'Loneliness', 'Mental Health', 'Stress']
    entities_data = {name: {} for name in attr_names}
    ticks = []

    try:
        for i in range(1000):
            filepath = os.path.join(BASE_DIR, f'{i}.csv')
            if not os.path.exists(filepath):
                break  # Stop looking if we reach a file that doesn't exist

            ticks.append(i)
            with open(filepath, newline='', encoding="utf-8") as csvfile:
                reader = csv.reader(csvfile)
                for row_idx, row in enumerate(reader):
                    # Ensure lists exist for this agent (row_idx)
                    if row_idx not in entities_data['Antibody']:
                        for name in attr_names:
                            entities_data[name][row_idx] = []

                    for col_idx, name in enumerate(attr_names):
                        if col_idx < len(row):
                            try:
                                val = float(row[col_idx])
                            except ValueError:
                                val = 0.0
                            entities_data[name][row_idx].append(val)
    except Exception as e:
        print(f"Error parsing CSVs: {e}")

    # Convert dictionaries of agents into flat arrays for JS injection
    clean_chart_data = {}
    for name in attr_names:
        clean_chart_data[name] = [entities_data[name][agent_id] for agent_id in sorted(entities_data[name].keys())]

    report_data["chart_ticks"] = json.dumps(ticks)
    report_data["chart_data"] = json.dumps(clean_chart_data)
    report_data["action_counts"] = json.dumps(action_counts)
    report_data["relationship_nodes"] = json.dumps([{"id": k, "label": v} for k,v in nodes.items()])
    report_data["relationship_edges"] = json.dumps(edges)
    report_data["sim_ticks"] = str(len(ticks))

    # Export a JSON snapshot for easy saving
    report_data["report_json"] = json.dumps({
        "start": report_data["sim_start"],
        "end": report_data["sim_end"],
        "duration": report_data["sim_duration"],
        "ticks": report_data["sim_ticks"],
        "actionCounts": action_counts,
        "logs": {
            "cmd": report_data["cmd_log"],
            "actions": report_data["actions_log"],
            "diseases": report_data["diseases_log"],
            "deaths": report_data["deaths_log"],
            "movements": report_data["movements_log"],
            "births": report_data["births_log"],
            "events": report_data["events_log"],
            "relationships": report_data["relationships_log"]
        }
    }, ensure_ascii=False, indent=2)

t1 = threading.Thread(target=log_all)
t2 = threading.Thread(target=log_logs)
t1.start()
t2.start()

t1.join()
t2.join()
if t2.is_alive():
    report_data["ai_summary"] = "<p><em>AI analysis unavailable (request timed out).</em></p>"

# Assemble Final HTML
final_html = HTML_SHELL
final_html = final_html.replace('{AI_SUMMARY}', report_data["ai_summary"])
final_html = final_html.replace('{CMD_LOG}', report_data["cmd_log"])
final_html = final_html.replace('{ACTIONS_LOG}', report_data["actions_log"])
final_html = final_html.replace('{DISEASES_LOG}', report_data["diseases_log"])
final_html = final_html.replace('{DEATHS_LOG}', report_data["deaths_log"])
final_html = final_html.replace('{MOVEMENTS_LOG}', report_data["movements_log"])
final_html = final_html.replace('{BIRTHS_LOG}', report_data["births_log"])
final_html = final_html.replace('{EVENTS_LOG}', report_data["events_log"])
final_html = final_html.replace('{RELATIONSHIPS_LOG}', report_data["relationships_log"])
final_html = final_html.replace('{SIM_START}', report_data["sim_start"])
final_html = final_html.replace('{SIM_END}', report_data["sim_end"])
final_html = final_html.replace('{SIM_DURATION}', report_data["sim_duration"])
final_html = final_html.replace('{SIM_TICKS}', report_data["sim_ticks"])
final_html = final_html.replace('{CHART_TICKS}', report_data["chart_ticks"])
final_html = final_html.replace('{CHART_DATA}', report_data["chart_data"])
final_html = final_html.replace('{ACTION_COUNTS}', report_data["action_counts"])
final_html = final_html.replace('{RELATION_NODES}', report_data["relationship_nodes"])
final_html = final_html.replace('{RELATION_EDGES}', report_data["relationship_edges"])
final_html = final_html.replace('{REPORT_JSON}', report_data["report_json"])
final_html = final_html.replace('{TIMESTAMP}', datetime.now().strftime('%Y-%m-%d %H:%M'))

# Write Output (relative to this script folder)
output_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "report.html"))
with open(output_path, "w", encoding="utf-8") as out:
    out.write(final_html)

print(f"Report generated successfully at {output_path}")
