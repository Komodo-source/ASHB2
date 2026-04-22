import csv
import ftplib
import json
import os
import re
import threading
from datetime import datetime

from ollama import ChatResponse, chat

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

HTML_SHELL = r"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ASHB2 — Simulation Report</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600&family=Roboto+Mono:wght@400;500;700&display=swap" rel="stylesheet">
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <script src="https://unpkg.com/vis-network/standalone/umd/vis-network.min.js"></script>
  <link rel="stylesheet" href="style.css">
</head>
<body>
<div class="app-shell">

  <header>
    <div class="header-left">
      <div class="brand">ASHB2</div>
      <div class="brand-sub">Anthropological Simulation Data</div>
    </div>
    <div class="meta-grid">
      <div class="meta-item"><span class="meta-label">Start</span><span class="mono">{SIM_START}</span></div>
      <div class="meta-item"><span class="meta-label">End</span><span class="mono">{SIM_END}</span></div>
      <div class="meta-item"><span class="meta-label">Duration</span><span class="mono">{SIM_DURATION}</span></div>
      <div class="meta-item"><span class="meta-label">Ticks</span><span class="mono">{SIM_TICKS}</span></div>
    </div>
    <button id="btn-export" class="btn">Export Data</button>
  </header>

  <nav class="tab-nav">
    <button class="tab-btn active" data-tab="overview">Overview</button>
    <button class="tab-btn" data-tab="global-stats">Telemetry</button>
    <button class="tab-btn" data-tab="entities">Agents</button>
    <button class="tab-btn" data-tab="network">Network</button>
    <button class="tab-btn" data-tab="logs">Logs</button>
  </nav>

  <div id="tab-overview" class="tab-page visible">
    <div class="overview-grid">
      <div class="panel">
        <div class="panel-title">Analysis Report</div>
        <div class="report-body">{AI_SUMMARY}</div>
      </div>
      <div class="gap-col">
        <div class="panel">
          <div class="panel-title">Action Frequency</div>
          <canvas id="actionChart" style="max-height:220px"></canvas>
        </div>
        <div class="panel">
          <div class="panel-title">Population Status</div>
          <canvas id="popChart" style="max-height:180px"></canvas>
        </div>
      </div>
    </div>
  </div>

  <div id="tab-global-stats" class="tab-page">
    <div class="stats-grid" id="global-charts-slot"></div>
  </div>

  <div id="tab-entities" class="tab-page">
    <div class="entity-layout">
      <div class="entity-sidebar">
        <div class="sidebar-hdr">
          <div class="sidebar-hdr-title">Agent Directory</div>
          <span class="entity-count-pill" id="entity-count-badge">0</span>
        </div>
        <div class="entity-search-wrap">
          <input class="entity-search" id="entity-search" type="text" placeholder="Search ID or Name...">
        </div>
        <div class="entity-list" id="entity-items"></div>
      </div>
      <div id="entity-detail-area">
        <div class="entity-empty">
          <p>Select an agent from the sidebar to view details.</p>
        </div>
      </div>
    </div>
  </div>

  <div id="tab-network" class="tab-page">
    <div class="panel">
      <div class="panel-title">Social Network Graph</div>
      <div id="relationship-network" class="network-canvas"></div>
    </div>
  </div>

  <div id="tab-logs" class="tab-page">
    <div class="logs-grid">

      <div class="log-block">
        <div class="log-block-hdr">
          <div class="log-block-title"><span class="log-type-dot" style="background:var(--text-dim)"></span>Events</div>
          <span class="log-count-badge" id="lc-events">0</span>
        </div>
        <div class="log-search-wrap"><input class="log-search" placeholder="Filter..." oninput="filterLog('log-events',this.value)"></div>
        <div class="log-entries" id="log-events"></div>
      </div>

      <div class="log-block">
        <div class="log-block-hdr">
          <div class="log-block-title"><span class="log-type-dot" style="background:var(--primary)"></span>Actions</div>
          <span class="log-count-badge" id="lc-actions">0</span>
        </div>
        <div class="log-search-wrap"><input class="log-search" placeholder="Filter..." oninput="filterLog('log-actions',this.value)"></div>
        <div class="log-entries" id="log-actions"></div>
      </div>

      <div class="log-block">
        <div class="log-block-hdr">
          <div class="log-block-title"><span class="log-type-dot" style="background:var(--red)"></span>Deaths</div>
          <span class="log-count-badge" id="lc-deaths">0</span>
        </div>
        <div class="log-search-wrap"><input class="log-search" placeholder="Filter..." oninput="filterLog('log-deaths',this.value)"></div>
        <div class="log-entries" id="log-deaths"></div>
      </div>

      <div class="log-block">
        <div class="log-block-hdr">
          <div class="log-block-title"><span class="log-type-dot" style="background:var(--yellow)"></span>Diseases</div>
          <span class="log-count-badge" id="lc-diseases">0</span>
        </div>
        <div class="log-search-wrap"><input class="log-search" placeholder="Filter..." oninput="filterLog('log-diseases',this.value)"></div>
        <div class="log-entries" id="log-diseases"></div>
      </div>

      <div class="log-block">
        <div class="log-block-hdr">
          <div class="log-block-title"><span class="log-type-dot" style="background:var(--green)"></span>Births</div>
          <span class="log-count-badge" id="lc-births">0</span>
        </div>
        <div class="log-search-wrap"><input class="log-search" placeholder="Filter..." oninput="filterLog('log-births',this.value)"></div>
        <div class="log-entries" id="log-births"></div>
      </div>

      <div class="log-block">
        <div class="log-block-hdr">
          <div class="log-block-title"><span class="log-type-dot" style="background:var(--text-faint)"></span>Movements</div>
          <span class="log-count-badge" id="lc-movements">0</span>
        </div>
        <div class="log-search-wrap"><input class="log-search" placeholder="Filter..." oninput="filterLog('log-movements',this.value)"></div>
        <div class="log-entries" id="log-movements"></div>
      </div>

      <div class="log-block">
        <div class="log-block-hdr">
          <div class="log-block-title"><span class="log-type-dot" style="background:var(--purple)"></span>Relationships</div>
          <span class="log-count-badge" id="lc-relationships">0</span>
        </div>
        <div class="log-search-wrap"><input class="log-search" placeholder="Filter..." oninput="filterLog('log-relationships',this.value)"></div>
        <div class="log-entries" id="log-relationships"></div>
      </div>

      <div class="log-block">
        <div class="log-block-hdr">
          <div class="log-block-title"><span class="log-type-dot" style="background:var(--border-light)"></span>System</div>
          <span class="log-count-badge" id="lc-cmd">0</span>
        </div>
        <div class="log-search-wrap"><input class="log-search" placeholder="Filter..." oninput="filterLog('log-cmd',this.value)"></div>
        <div class="log-entries" id="log-cmd"></div>
      </div>

    </div>
  </div>

</div>

<script>
const reportData     = {REPORT_JSON};
const ticks          = {CHART_TICKS};
const attributesData = {CHART_DATA};
const actionCounts   = {ACTION_COUNTS};
const relNodes       = {RELATION_NODES};
const relEdges       = {RELATION_EDGES};

const ATTR_NAMES = ['Antibody','Boredom','Anger','Happiness','Health','Hygiene','Loneliness','Mental Health','Stress'];
const ATTR_COLOR = {
  'Antibody':'#34d399','Boredom':'#fbbf24','Anger':'#f87171','Happiness':'#38bdf8',
  'Health':'#818cf8','Hygiene':'#94a3b8','Loneliness':'#c084fc','Mental Health':'#f472b6','Stress':'#fb923c',
};
const BAD_HIGH = new Set(['Stress','Loneliness','Boredom','Anger']);
const NUM_ENTITIES = (attributesData['Health']||[]).length;
const entityNameMap = {};
relNodes.forEach(n => { entityNameMap[n.id] = n.label; });

document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-page').forEach(p => p.classList.remove('visible'));
    btn.classList.add('active');
    document.getElementById('tab-' + btn.dataset.tab).classList.add('visible');
    if (btn.dataset.tab === 'network') renderNetwork();
  });
});
document.getElementById('btn-export').addEventListener('click', () => {
  const blob = new Blob([JSON.stringify(reportData,null,2)],{type:'application/json'});
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob); a.download = 'ashb2-data.json';
  document.body.appendChild(a); a.click();
  setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 200);
});

// Chart.js clean styling
Chart.defaults.color          = '#94a3b8';
Chart.defaults.borderColor    = '#1e293b';
Chart.defaults.font.family    = "'Inter', sans-serif";
Chart.defaults.font.size      = 11;

(function() {
  const labels  = Object.keys(actionCounts).sort((a,b) => actionCounts[b]-actionCounts[a]);
  const palette = ['#38bdf8','#818cf8','#34d399','#fbbf24','#f87171','#c084fc','#94a3b8','#f472b6','#fb923c'];
  new Chart(document.getElementById('actionChart'), {
    type:'bar',
    data:{ labels, datasets:[{
      label:'Count', data:labels.map(l=>actionCounts[l]),
      backgroundColor:labels.map((_,i)=>palette[i%palette.length]),
      borderRadius: 4, borderSkipped:false
    }]},
    options:{
      responsive:true, plugins:{legend:{display:false}},
      scales:{
        x:{ticks:{maxRotation:40},grid:{display:false}},
        y:{grid:{color:'#1e293b'}}
      }
    }
  });
})();

(function() {
  const alive      = (attributesData['Health']||[]).length;
  const deathCount = (reportData.logs.deaths||'').split('\n').filter(l=>l.trim()).length;
  const birthCount = (reportData.logs.births||'').split('\n').filter(l=>l.trim()).length;
  new Chart(document.getElementById('popChart'), {
    type:'doughnut',
    data:{
      labels:['Alive','Deaths','Births'],
      datasets:[{
        data:[alive,deathCount,birthCount],
        backgroundColor:['#34d399','#f87171','#38bdf8'],
        borderWidth:0, hoverOffset:4,
      }]
    },
    options:{
      responsive:true, cutout:'75%',
      plugins:{legend:{position:'right',labels:{padding:15,boxWidth:10,usePointStyle:true}}}
    }
  });
})();

(function() {
  const slot = document.getElementById('global-charts-slot');
  ATTR_NAMES.forEach(attrName => {
    const agentsArray = attributesData[attrName]||[];
    const color = ATTR_COLOR[attrName];
    const wrap = document.createElement('div');
    wrap.className = 'chart-wrap';
    const cid = 'gc-'+attrName.replace(/\s/g,'_');
    wrap.innerHTML = `
      <div class="chart-label">
        <span class="cdot" style="background:${color}"></span>${attrName}
      </div>
      <canvas id="${cid}" style="max-height:200px"></canvas>`;
    slot.appendChild(wrap);
    const datasets = agentsArray.map((vals,idx) => {
      const hue = (ATTR_NAMES.indexOf(attrName)*40 + idx*23)%360;
      return { label:entityNameMap[idx]||`Agent ${idx}`, data:vals,
        borderColor:`hsl(${hue},70%,60%)`, backgroundColor:`hsla(${hue},70%,60%,0.1)`,
        borderWidth:1.5, pointRadius:0, tension:0.2 };
    });
    new Chart(document.getElementById(cid), {
      type:'line', data:{labels:ticks,datasets},
      options:{
        responsive:true, animation:false,
        plugins:{
          legend:{display:agentsArray.length<=8,labels:{boxWidth:8,padding:10}},
          tooltip:{mode:'index',intersect:false,backgroundColor:'#1e293b',titleColor:'#f1f5f9',bodyColor:'#cbd5e1'}
        },
        scales:{
          x:{title:{display:true,text:'Tick'},grid:{color:'#1e293b'}},
          y:{grid:{color:'#1e293b'}}
        }
      }
    });
  });
})();

const entityItemsEl    = document.getElementById('entity-items');
const entityDetailEl   = document.getElementById('entity-detail-area');
const entityCountBadge = document.getElementById('entity-count-badge');
const entityChartRefs  = {};
const AVATAR_COLORS    = ['#38bdf8','#818cf8','#34d399','#fbbf24','#f87171','#c084fc','#94a3b8','#f472b6','#fb923c'];
function avatarColor(idx) { return AVATAR_COLORS[idx%AVATAR_COLORS.length]; }

function buildEntityList(filter) {
  entityItemsEl.innerHTML = ''; let shown = 0;
  for (let i=0;i<NUM_ENTITIES;i++) {
    const name = entityNameMap[i]||`Agent ${i}`;
    if (filter && !name.toLowerCase().includes(filter.toLowerCase()) && !String(i).includes(filter)) continue;
    shown++;
    const clr  = avatarColor(i);
    const item = document.createElement('div');
    item.className = 'entity-item'; item.dataset.idx = i;
    item.innerHTML = `
      <div class="avatar" style="color:${clr};border-color:${clr}">${name[0].toUpperCase()}</div>
      <div>
        <div class="entity-item-name">${name}</div>
        <div class="entity-item-id">ID: ${i}</div>
      </div>`;
    item.addEventListener('click', () => {
      document.querySelectorAll('.entity-item').forEach(e=>e.classList.remove('selected'));
      item.classList.add('selected'); renderEntityDetail(i);
    });
    entityItemsEl.appendChild(item);
  }
  entityCountBadge.textContent = shown;
}
buildEntityList('');
document.getElementById('entity-search').addEventListener('input', e=>buildEntityList(e.target.value));

function getLastVal(attrName,idx) {
  const arr = (attributesData[attrName]||[])[idx];
  return (!arr||!arr.length)?0:arr[arr.length-1];
}
function statBarColor(attrName,val) {
  if (BAD_HIGH.has(attrName)) { if(val>70)return'#f87171';if(val>40)return'#fbbf24';return'#34d399'; }
  if(val<30)return'#f87171';if(val<60)return'#fbbf24';return'#34d399';
}

function renderEntityDetail(idx) {
  const name = entityNameMap[idx]||`Agent ${idx}`;
  const clr  = avatarColor(idx);
  if (entityChartRefs[idx]) entityChartRefs[idx].forEach(c=>c.destroy());
  entityChartRefs[idx] = [];

  const statBarsHtml = ATTR_NAMES.map(attr => {
    const val=getLastVal(attr,idx), pct=Math.min(100,Math.max(0,val)).toFixed(1);
    const barClr=statBarColor(attr,val);
    return `<div class="stat-bar-row">
      <div class="stat-bar-meta">
        <span class="stat-bar-name">${attr}</span>
        <span class="stat-bar-val" style="color:${barClr}">${parseFloat(val).toFixed(1)}</span>
      </div>
      <div class="stat-bar-track">
        <div class="stat-bar-fill" style="width:${pct}%;background:${barClr}"></div>
      </div>
    </div>`;
  }).join('');

  const chartIds   = ATTR_NAMES.map(a=>`ec-${idx}-${a.replace(/\s/g,'_')}`);
  const chartsHtml = ATTR_NAMES.map((attr,ai) => {
    const clr2=ATTR_COLOR[attr];
    return `<div class="chart-wrap">
      <div class="chart-label"><span class="cdot" style="background:${clr2}"></span>${attr}</div>
      <canvas id="${chartIds[ai]}" style="max-height:120px"></canvas>
    </div>`;
  }).join('');

  const entityActions = getEntityActions(idx,name);
  const entityLogs    = filterEntityLogs(idx,name);
  const partners = [];
  relEdges.forEach(e => {
    if(e.from===idx) partners.push({id:e.to,  name:entityNameMap[e.to]  ||`Agent ${e.to}`,  label:e.label});
    if(e.to  ===idx) partners.push({id:e.from,name:entityNameMap[e.from]||`Agent ${e.from}`,label:e.label});
  });
  const linksHtml = partners.length
    ? partners.map(p => {
        const pc=avatarColor(p.id);
        return `<div style="display:flex;align-items:center;gap:0.8rem;padding:0.6rem 0;border-bottom:1px solid var(--border)">
          <div class="avatar" style="color:${pc};border-color:${pc};width:28px;height:28px;font-size:0.75rem">${p.name[0].toUpperCase()}</div>
          <span style="font-size:0.85rem;font-weight:500;color:var(--text)">${p.name}</span>
          <span class="badge" style="margin-left:auto;background:var(--bg);border:1px solid var(--border);color:var(--text-dim)">${p.label}</span>
        </div>`;
      }).join('')
    : `<p style="color:var(--text-faint);font-size:0.8rem">No relationships recorded.</p>`;

  entityDetailEl.innerHTML = `<div class="entity-detail">
    <div class="entity-hero">
      <div class="entity-hero-avatar" style="color:${clr};border-color:${clr}">${name[0].toUpperCase()}</div>
      <div>
        <div class="entity-hero-name">${name}</div>
        <div class="entity-hero-sub">ID: ${idx} &nbsp;|&nbsp; Active Ticks: ${ticks.length}</div>
        <div class="badge-row">
          <span class="badge alive">Alive</span>
          <span class="badge info">${ticks.length} Ticks</span>
          ${partners.length?`<span class="badge info">${partners.length} Link${partners.length>1?'s':''}</span>`:''}
        </div>
      </div>
    </div>
    <div class="panel">
      <div class="panel-title">Current Stats</div>
      <div class="stat-bars-grid">${statBarsHtml}</div>
    </div>
    <div class="panel">
      <div class="panel-title">History</div>
      <div class="entity-charts-grid">${chartsHtml}</div>
    </div>
    <div class="panel">
      <div class="panel-title">Relationships</div>
      ${linksHtml}
    </div>
    ${entityActions?`<div class="panel"><div class="panel-title">Actions Performed</div><canvas id="eac-${idx}" style="max-height:200px"></canvas></div>`:''}
    <div class="panel">
      <div class="panel-title">Log History</div>
      <div class="log-entries" style="max-height:350px;background:var(--bg);border-radius:6px;border:1px solid var(--border)">
        ${entityLogs||'<div style="color:var(--text-faint);padding:1rem;font-size:0.8rem">No log entries found.</div>'}
      </div>
    </div>
  </div>`;

  requestAnimationFrame(() => {
    ATTR_NAMES.forEach((attr,ai) => {
      const canvas=document.getElementById(chartIds[ai]);
      if(!canvas)return;
      const vals=(attributesData[attr]||[])[idx]||[];
      const clr2=ATTR_COLOR[attr];
      const c=new Chart(canvas,{
        type:'line',
        data:{labels:ticks,datasets:[{label:attr,data:vals,borderColor:clr2,backgroundColor:clr2+'20',borderWidth:1.5,pointRadius:0,tension:0.2,fill:true}]},
        options:{responsive:true,animation:false,
          plugins:{legend:{display:false},tooltip:{mode:'index',intersect:false,backgroundColor:'#1e293b'}},
          scales:{x:{display:false},y:{grid:{color:'#1e293b'}}}}
      });
      entityChartRefs[idx].push(c);
    });
    if (entityActions) {
      const eacCanvas=document.getElementById(`eac-${idx}`);
      if(eacCanvas){
        const palette=['#38bdf8','#818cf8','#34d399','#fbbf24','#f87171','#c084fc','#94a3b8','#f472b6','#fb923c'];
        const labels=Object.keys(entityActions).sort((a,b)=>entityActions[b]-entityActions[a]);
        const c=new Chart(eacCanvas,{
          type:'bar',
          data:{labels,datasets:[{label:'Count',data:labels.map(l=>entityActions[l]),
            backgroundColor:labels.map((_,i)=>palette[i%palette.length]),
            borderRadius:4,borderSkipped:false}]},
          options:{responsive:true,plugins:{legend:{display:false}},
            scales:{x:{ticks:{maxRotation:35},grid:{display:false}},
                    y:{grid:{color:'#1e293b'}}}}
        });
        entityChartRefs[idx].push(c);
      }
    }
  });
}

function getEntityActions(idx,name) {
  const text=reportData.logs.actions||'';
  const nameRe=new RegExp(name.replace(/[.*+?^${}()|[\]\\]/g,'\\$&'),'i');
  const idRe=new RegExp(`\\(${idx}\\)|Agent ${idx}[^0-9]`,'i');
  const counts={};
  text.split('\n').forEach(line=>{
    if(!line.trim()||(!nameRe.test(line)&&!idRe.test(line)))return;
    const m=line.match(/performed[:\s]+([^\-\n,]+)/i);
    if(m){const act=m[1].trim();counts[act]=(counts[act]||0)+1;}
  });
  return Object.keys(counts).length?counts:null;
}
function filterEntityLogs(idx,name) {
  const nameRe=new RegExp(name.replace(/[.*+?^${}()|[\]\\]/g,'\\$&'),'i');
  const idRe=new RegExp(`\\(${idx}\\)|Agent ${idx}[^0-9]`,'i');
  const sources=[
    {text:reportData.logs.actions||'',cls:'ev-action'},
    {text:reportData.logs.deaths||'',cls:'ev-death'},
    {text:reportData.logs.births||'',cls:'ev-birth'},
    {text:reportData.logs.diseases||'',cls:'ev-disease'},
    {text:reportData.logs.relationships||'',cls:'ev-relation'},
    {text:reportData.logs.events||'',cls:'ev-event'},
  ];
  let html='';
  sources.forEach(({text,cls})=>{
    text.split('\n').forEach(line=>{
      if(!line.trim()||(!nameRe.test(line)&&!idRe.test(line)))return;
      html+=renderLogLine(line,cls);
    });
  });
  return html;
}

const LOG_TYPE_LABELS={'ev-death':'Death','ev-birth':'Birth','ev-disease':'Health','ev-relation':'Social','ev-action':'Action','ev-move':'Move','ev-event':'Event','ev-cmd':'System'};
function detectLogClass(line,hint) {
  if(hint)return hint;
  const l=line.toLowerCase();
  if(l.includes('died')||l.includes('death'))return'ev-death';
  if(l.includes('born')||l.includes('birth'))return'ev-birth';
  if(l.includes('contracted')||l.includes('disease')||l.includes('cured')||l.includes('plague'))return'ev-disease';
  if(l.includes('relationship'))return'ev-relation';
  if(l.includes('moved')||l.includes('movement'))return'ev-move';
  if(l.includes('performed')||l.includes('action'))return'ev-action';
  return'ev-event';
}
function renderLogLine(line,hintClass) {
  if(!line.trim())return'';
  const cls=detectLogClass(line,hintClass);
  const typeLabel=LOG_TYPE_LABELS[cls]||'Info';
  const tsMatch=line.match(/^\[([^\]]+)\]\s*(.*)/s);
  const tagHtml=`<span class="le-tag">${typeLabel}</span>`;
  if(tsMatch){
    return `<div class="log-entry ${cls}">
      <div class="log-entry-header">${tagHtml}<span class="le-ts">${tsMatch[1]}</span></div>
      <div class="le-body">${tsMatch[2].replace(/</g,'&lt;').replace(/>/g,'&gt;')}</div>
    </div>`;
  }
  return `<div class="log-entry ${cls}">
    <div class="log-entry-header">${tagHtml}</div>
    <div class="le-body">${line.replace(/</g,'&lt;').replace(/>/g,'&gt;')}</div>
  </div>`;
}
function populateLog(containerId,text,countId,hintClass) {
  const el=document.getElementById(containerId);
  const counter=document.getElementById(countId);
  const lines=text.split('\n').filter(l=>l.trim());
  el.innerHTML=lines.map(l=>renderLogLine(l,hintClass)).join('');
  if(counter)counter.textContent=lines.length;
  el.dataset.raw=text;
}
function filterLog(containerId,query) {
  const el=document.getElementById(containerId);
  const raw=el.dataset.raw||'';
  const q=query.toLowerCase();
  const lines=raw.split('\n').filter(l=>l.trim());
  el.innerHTML=(q?lines.filter(l=>l.toLowerCase().includes(q)):lines).map(l=>renderLogLine(l,'')).join('');
}
populateLog('log-events',       reportData.logs.events       ||'','lc-events',       'ev-event');
populateLog('log-actions',      reportData.logs.actions      ||'','lc-actions',      'ev-action');
populateLog('log-deaths',       reportData.logs.deaths       ||'','lc-deaths',       'ev-death');
populateLog('log-diseases',     reportData.logs.diseases     ||'','lc-diseases',     'ev-disease');
populateLog('log-births',       reportData.logs.births       ||'','lc-births',       'ev-birth');
populateLog('log-movements',    reportData.logs.movements    ||'','lc-movements',    'ev-move');
populateLog('log-relationships',reportData.logs.relationships||'','lc-relationships','ev-relation');
populateLog('log-cmd',          reportData.logs.cmd          ||'','lc-cmd',          '');

let networkRendered=false;
function renderNetwork() {
  if(networkRendered)return;
  networkRendered=true;
  const container=document.getElementById('relationship-network');
  const data={nodes:new vis.DataSet(relNodes),edges:new vis.DataSet(relEdges)};
  const options={
    physics:{stabilization:{iterations:200},barnesHut:{gravitationalConstant:-9000,springLength:140}},
    edges:{
      smooth:{enabled:true,type:'cubicBezier'},
      font:{align:'top',color:'#94a3b8',size:11,face:"'Inter',sans-serif"},
      color:{color:'#334155',highlight:'#38bdf8'},
      width:1.5,
    },
    nodes:{
      shape:'dot',size:12,
      font:{color:'#f1f5f9',size:12,face:"'Inter',sans-serif", strokeWidth: 0},
      color:{
        border:'#38bdf8',background:'rgba(56,189,248,0.1)',
        highlight:{border:'#34d399',background:'rgba(52,211,153,0.2)'},
        hover:    {border:'#818cf8',background:'rgba(129,140,248,0.2)'},
      },
      borderWidth:2
    },
    interaction:{hover:true,tooltipDelay:100},
  };
  new vis.Network(container,data,options);
}
</script>
</body>
</html>
"""

report_data = {
    "ai_summary": "",
    "cmd_log": "",
    "actions_log": "",
    "diseases_log": "",
    "deaths_log": "",
    "movements_log": "",
    "births_log": "",
    "events_log": "",
    "relationships_log": "",
    "sim_start": "UNKNOWN",
    "sim_end": "UNKNOWN",
    "sim_duration": "UNKNOWN",
    "sim_ticks": "0",
    "chart_ticks": "[]",
    "chart_data": "{}",
    "action_counts": "{}",
    "relationship_nodes": "[]",
    "relationship_edges": "[]",
    "report_json": "{}",
}


def safe_read(filepath):
    full_path = (
        filepath if os.path.isabs(filepath) else os.path.join(BASE_DIR, filepath)
    )
    try:
        with open(full_path, "r", encoding="utf-8") as f:
            return f.read()
    except Exception as e:
        return f"[File not found or unreadable: {full_path}]"


def log_logs():
    cmd_content = safe_read("../cmd_log.txt")
    report_data["cmd_log"] = cmd_content
    try:
        response: ChatResponse = chat(
            model="qwen2.5:14b",
            messages=[
                {"role": "user", "content": prompt + "\n\n" + cmd_content},
            ],
        )
        ai_text = str(response.message.content)
        ai_text = re.sub(r"^### (.+)$", r"<h3>\1</h3>", ai_text, flags=re.MULTILINE)
        ai_text = re.sub(r"^## (.+)$", r"<h2>\1</h2>", ai_text, flags=re.MULTILINE)
        ai_text = re.sub(r"^# (.+)$", r"<h2>\1</h2>", ai_text, flags=re.MULTILINE)
        paragraphs = []
        for block in re.split(r"\n{2,}", ai_text):
            block = block.strip()
            if block and not block.startswith("<h"):
                block = f"<p>{block}</p>"
            paragraphs.append(block)
        report_data["ai_summary"] = "\n".join(paragraphs)
    except Exception as e:
        report_data["ai_summary"] = f"<p>Error generating analysis: {e}</p>"


def _parse_timestamp_range(log_text: str):
    timestamps = []
    for m in re.finditer(
        r"^\[([0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2})\]",
        log_text,
        flags=re.MULTILINE,
    ):
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
    return " ".join(parts)


def log_all():
    report_data["deaths_log"] = safe_read("deaths_log.txt")
    report_data["diseases_log"] = safe_read("diseases_log.txt")
    report_data["actions_log"] = safe_read("actions_log.txt")
    report_data["movements_log"] = safe_read("movements_log.txt")
    report_data["births_log"] = safe_read("births_log.txt")
    report_data["events_log"] = safe_read("events_log.txt")
    report_data["relationships_log"] = safe_read("relationships_log.txt")

    start, end = _parse_timestamp_range(report_data["events_log"])
    if start is None or end is None:
        start, end = _parse_timestamp_range(report_data["deaths_log"])
    if start is None or end is None:
        start, end = _parse_timestamp_range(report_data["cmd_log"])

    report_data["sim_start"] = (
        start.strftime("%Y-%m-%d %H:%M:%S") if start else "UNKNOWN"
    )
    report_data["sim_end"] = end.strftime("%Y-%m-%d %H:%M:%S") if end else "UNKNOWN"
    report_data["sim_duration"] = (
        _format_duration(end - start) if (start and end) else "UNKNOWN"
    )

    action_counts = {}
    try:
        for i in range(1000):
            filepath = os.path.join(BASE_DIR, f"act_{i}.csv")
            if not os.path.exists(filepath):
                break
            with open(filepath, encoding="utf-8") as f:
                line = f.readline().strip()
                for action in re.findall(r"([A-Za-z0-9 _\-]+)(?=,category:)", line):
                    action = action.strip()
                    if not action:
                        continue
                    action_counts[action] = action_counts.get(action, 0) + 1
    except Exception as e:
        print(f"Error parsing action CSVs: {e}")

    nodes = {}
    edges = []
    for line in report_data["relationships_log"].splitlines():
        m = re.match(
            r"^\[([^\]]+)\]\s*Relationship:\s*(.+?)\s*\((\d+)\)\s*and\s*(.+?)\s*\((\d+)\)\s*-\s*([^\-\n]+)(?:-\s*(.*))?$",
            line,
        )
        if not m:
            continue
        timestamp, name1, id1, name2, id2, reltype, details = m.groups()
        id1, id2 = int(id1), int(id2)
        nodes[id1] = name1.strip()
        nodes[id2] = name2.strip()
        title = f"{reltype.strip()}\n{timestamp}"
        if details:
            title += f"\n{details.strip()}"
        edges.append({"from": id1, "to": id2, "label": reltype.strip(), "title": title})

    attr_names = [
        "Antibody",
        "Boredom",
        "Anger",
        "Happiness",
        "Health",
        "Hygiene",
        "Loneliness",
        "Mental Health",
        "Stress",
    ]
    entities_data = {name: {} for name in attr_names}
    ticks = []

    try:
        for i in range(1000):
            filepath = os.path.join(BASE_DIR, f"{i}.csv")
            if not os.path.exists(filepath):
                break
            ticks.append(i)
            with open(filepath, newline="", encoding="utf-8") as csvfile:
                reader = csv.reader(csvfile)
                for row_idx, row in enumerate(reader):
                    if row_idx not in entities_data["Antibody"]:
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

    clean_chart_data = {}
    for name in attr_names:
        clean_chart_data[name] = [
            entities_data[name][agent_id]
            for agent_id in sorted(entities_data[name].keys())
        ]

    report_data["chart_ticks"] = json.dumps(ticks)
    report_data["chart_data"] = json.dumps(clean_chart_data)
    report_data["action_counts"] = json.dumps(action_counts)
    report_data["relationship_nodes"] = json.dumps(
        [{"id": k, "label": v} for k, v in nodes.items()]
    )
    report_data["relationship_edges"] = json.dumps(edges)
    report_data["sim_ticks"] = str(len(ticks))

    report_data["report_json"] = json.dumps(
        {
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
                "relationships": report_data["relationships_log"],
            },
        },
        ensure_ascii=False,
        indent=2,
    )


t1 = threading.Thread(target=log_all)
# t2 = threading.Thread(target=log_logs)   # uncomment to enable AI summary
t1.start()
# t2.start()
t1.join()
# t2.join()

final_html = HTML_SHELL
replacements = {
    "{AI_SUMMARY}": report_data["ai_summary"],
    "{SIM_START}": report_data["sim_start"],
    "{SIM_END}": report_data["sim_end"],
    "{SIM_DURATION}": report_data["sim_duration"],
    "{SIM_TICKS}": report_data["sim_ticks"],
    "{CHART_TICKS}": report_data["chart_ticks"],
    "{CHART_DATA}": report_data["chart_data"],
    "{ACTION_COUNTS}": report_data["action_counts"],
    "{RELATION_NODES}": report_data["relationship_nodes"],
    "{RELATION_EDGES}": report_data["relationship_edges"],
    "{REPORT_JSON}": report_data["report_json"],
    "{TIMESTAMP}": datetime.now().strftime("%Y-%m-%d %H:%M"),
}
for placeholder, value in replacements.items():
    final_html = final_html.replace(placeholder, value)


f = ""
with open("./simulation_continuity.txt", "r") as version:
    f = version.readline()

with open("./simulation_continuity.txt", "w") as version:
    version.write(str(int(f) + 1))

print(f"report{f}.html")
output_path = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", f"report{f}.html")
)
with open(output_path, "w", encoding="utf-8") as out:
    out.write(final_html)

print(f"[ASHB2] Report generated → {output_path}")

session = ftplib.FTP("ftpupload.net", "if0_37377007", "bujsYxINZZBY4")
session.cwd("arena.ct.ws")
session.cwd("htdocs")
file = open(
    os.path.abspath(os.path.join(os.path.dirname(__file__), "..", f"report{f}.html")),
    "rb",
)
session.storbinary(f"STOR {f'report{f}.html'}", file)
file.close()
session.quit()
print("file saved to the server")
