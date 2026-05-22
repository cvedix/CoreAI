#pragma once

#include <string>

namespace cvedix_nodes {

inline const std::string WEB_DEBUG_DASHBOARD_HTML = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>OmniCore Web Debug Dashboard</title>
<meta name="description" content="OmniCore real-time pipeline debug dashboard with OSD video stream and analysis board visualization">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap" rel="stylesheet">
<style>
  :root {
    --bg-primary: #000000;
    --bg-secondary: #111111;
    --bg-card: #111111;
    --bg-card-hover: #222222;
    --border: rgba(255,255,255,0.15);
    --border-strong: rgba(255,255,255,0.3);
    --text: #ffffff;
    --text-muted: #aaaaaa;
    --text-dim: #777777;
    --accent: #ffffff;
    --accent-glow: rgba(255,255,255,0.25);
    --accent2: #dddddd;
    --success: #ffffff;
    --success-glow: rgba(255,255,255,0.25);
    --warning: #cccccc;
    --danger: #999999;
    --gradient-accent: linear-gradient(135deg, #ffffff, #dddddd, #bbbbbb);
    --gradient-card: linear-gradient(145deg, rgba(255,255,255,0.05) 0%, transparent 50%);
    --radius: 12px;
    --radius-sm: 8px;
    --radius-xs: 6px;
    --shadow-sm: 0 1px 3px rgba(0,0,0,0.3);
    --shadow-md: 0 4px 12px rgba(0,0,0,0.4);
    --shadow-lg: 0 8px 32px rgba(0,0,0,0.5);
    --shadow-glow: 0 0 20px var(--accent-glow);
    --transition: 0.2s cubic-bezier(0.4, 0, 0.2, 1);
  }

  * {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
  }

  body {
    min-height: 100vh;
    background: var(--bg-primary);
    color: var(--text);
    font-family: 'Inter', -apple-system, BlinkMacSystemFont, sans-serif;
    overflow-x: hidden;
    -webkit-font-smoothing: antialiased;
  }

  /* ── Scrollbar ── */
  ::-webkit-scrollbar { width: 6px; }
  ::-webkit-scrollbar-track { background: transparent; }
  ::-webkit-scrollbar-thumb { background: var(--border-strong); border-radius: 3px; }

  /* ── Shell Layout ── */
  .shell {
    min-height: 100vh;
    display: flex;
    flex-direction: column;
  }

  /* ── Topbar ── */
  .topbar {
    position: sticky;
    top: 0;
    z-index: 100;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 16px;
    padding: 12px 20px;
    background: rgba(10,10,15,0.85);
    backdrop-filter: blur(16px) saturate(1.4);
    -webkit-backdrop-filter: blur(16px) saturate(1.4);
    border-bottom: 1px solid var(--border);
  }

  .brand {
    display: flex;
    align-items: center;
    gap: 12px;
    min-width: 0;
  }

  .mark {
    width: 32px;
    height: 32px;
    display: grid;
    place-items: center;
    background: var(--gradient-accent);
    color: #fff;
    font-size: 14px;
    font-weight: 800;
    border-radius: var(--radius-xs);
    box-shadow: var(--shadow-glow);
  }

  h1 {
    font-size: 16px;
    font-weight: 700;
    line-height: 1.2;
    white-space: nowrap;
    background: var(--gradient-accent);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
  }

  .topbar-right {
    display: flex;
    align-items: center;
    gap: 12px;
  }

  .connection {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 6px 12px;
    border: 1px solid var(--border-strong);
    border-radius: var(--radius-xs);
    color: var(--text-muted);
    font-size: 12px;
    font-weight: 500;
    white-space: nowrap;
    transition: var(--transition);
  }

  .connection.connected {
    border-color: var(--success);
    color: var(--success);
  }

  .connection-dot {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    background: var(--text-muted);
    transition: var(--transition);
  }

  .connection.connected .connection-dot {
    background: var(--success);
    box-shadow: 0 0 8px var(--success-glow);
    animation: pulse-dot 2s ease-in-out infinite;
  }

  @keyframes pulse-dot {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.5; }
  }

  /* ── Main Content ── */
  .content {
    flex: 1;
    padding: 16px;
    display: flex;
    flex-direction: column;
    gap: 16px;
  }

  /* ── Panels Grid ── */
  .panels {
    display: grid;
    grid-template-columns: 1fr 1fr;
    grid-template-rows: 1fr;
    gap: 16px;
    flex: 1;
    min-height: 400px;
  }

  .right-column {
    display: flex;
    flex-direction: column;
    gap: 16px;
    min-height: 0;
  }

  .right-column .panel:first-child {
    flex: 1;
    min-height: 200px;
  }

  .right-column .panel:last-child {
    flex: 0 0 auto;
    max-height: 220px;
  }

  /* ── Result Panel ── */
  .result-grid {
    display: flex;
    gap: 8px;
    padding: 10px;
    overflow-x: auto;
    flex-wrap: nowrap;
    min-height: 100px;
    align-items: flex-start;
  }

  .result-card {
    flex: 0 0 auto;
    width: 110px;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
    padding: 6px;
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    background: rgba(255,255,255,0.03);
    transition: border-color var(--transition);
  }

  .result-card:hover {
    border-color: var(--border-strong);
  }

  .result-card.identified {
    border-color: rgba(0,200,100,0.4);
  }

  .result-card.unknown {
    border-color: rgba(255,140,0,0.3);
  }

  .result-card img {
    width: 72px;
    height: 72px;
    object-fit: cover;
    border-radius: var(--radius-xs);
    border: 1px solid var(--border);
  }

  .result-label {
    font-size: 10px;
    color: var(--text-muted);
    text-align: center;
    word-break: normal;
    overflow-wrap: break-word;
    line-height: 1.3;
    width: 100%;
  }

  .result-empty {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 100%;
    min-height: 80px;
    color: var(--text-dim);
    font-size: 12px;
  }

  .badge-result {
    background: rgba(0,200,100,0.15);
    color: #4ade80;
    border: 1px solid rgba(0,200,100,0.3);
  }

  /* ── Panel (shared) ── */
  .panel {
    display: flex;
    flex-direction: column;
    border: 1px solid var(--border);
    border-radius: var(--radius);
    background: var(--bg-card);
    background-image: var(--gradient-card);
    overflow: hidden;
    transition: border-color var(--transition), box-shadow var(--transition);
    min-height: 0;
  }

  .panel:hover {
    border-color: var(--border-strong);
    box-shadow: var(--shadow-md);
  }

  .panel-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    padding: 10px 14px;
    border-bottom: 1px solid var(--border);
    background: rgba(0,0,0,0.2);
    flex-shrink: 0;
  }

  .panel-title {
    display: flex;
    align-items: center;
    gap: 8px;
    font-size: 12px;
    font-weight: 600;
    letter-spacing: 0.04em;
    text-transform: uppercase;
    color: var(--text-muted);
  }

  .panel-title .badge {
    display: inline-flex;
    align-items: center;
    padding: 2px 6px;
    font-size: 10px;
    font-weight: 600;
    border-radius: 4px;
    text-transform: uppercase;
    letter-spacing: 0.06em;
  }

  .badge-live {
    background: rgba(255,255,255,0.15);
    color: var(--success);
    border: 1px solid rgba(255,255,255,0.3);
  }

  .badge-board {
    background: rgba(255,255,255,0.15);
    color: var(--accent);
    border: 1px solid rgba(255,255,255,0.3);
  }

  .controls {
    display: flex;
    align-items: center;
    gap: 6px;
  }

  /* ── Buttons ── */
  button {
    min-width: 76px;
    height: 28px;
    border: 1px solid var(--border-strong);
    border-radius: var(--radius-xs);
    background: rgba(255,255,255,0.04);
    color: var(--text-muted);
    font-family: inherit;
    font-size: 11px;
    font-weight: 600;
    cursor: pointer;
    transition: var(--transition);
    letter-spacing: 0.02em;
  }

  button:hover {
    background: rgba(255,255,255,0.08);
    color: var(--text);
    border-color: rgba(255,255,255,0.2);
  }

  button.active {
    background: var(--accent);
    color: #fff;
    border-color: var(--accent);
    box-shadow: 0 0 12px var(--accent-glow);
  }

  /* ── Stream Wrapper ── */
  .stream-wrap {
    position: relative;
    flex: 1;
    min-height: 300px;
    display: flex;
    align-items: center;
    justify-content: center;
    background: #000;
    overflow: hidden;
  }

  .stream-wrap img {
    width: 100%;
    height: auto;
    max-height: 100%;
    object-fit: contain;
    display: block;
  }

  .placeholder {
    position: absolute;
    inset: 0;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 12px;
    color: var(--text-dim);
    font-size: 13px;
    text-align: center;
    background: rgba(0,0,0,0.6);
    backdrop-filter: blur(4px);
    transition: opacity var(--transition);
  }

  .placeholder.hidden {
    opacity: 0;
    pointer-events: none;
    visibility: hidden;
    display: none;
  }

  .placeholder-spinner {
    width: 28px;
    height: 28px;
    border: 2px solid var(--border-strong);
    border-top-color: var(--accent);
    border-radius: 50%;
    animation: spin 0.8s linear infinite;
  }

  @keyframes spin {
    to { transform: rotate(360deg); }
  }

  /* ── Stats Bar ── */
  .stats {
    display: grid;
    grid-template-columns: repeat(6, 1fr);
    gap: 12px;
    flex-shrink: 0;
  }

  .stat {
    padding: 14px 16px;
    border: 1px solid var(--border);
    border-radius: var(--radius);
    background: var(--bg-card);
    background-image: var(--gradient-card);
    transition: border-color var(--transition), transform var(--transition);
    position: relative;
    overflow: hidden;
  }

  .stat:hover {
    border-color: var(--border-strong);
    transform: translateY(-1px);
  }

  .stat::before {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    height: 2px;
    border-radius: 2px 2px 0 0;
    opacity: 0.6;
  }

  .stat:nth-child(1)::before { background: var(--accent); }
  .stat:nth-child(2)::before { background: var(--warning); }
  .stat:nth-child(3)::before { background: var(--success); }
  .stat:nth-child(4)::before { background: #ffffff; }
  .stat:nth-child(5)::before { background: var(--accent2); }
  .stat:nth-child(6)::before { background: var(--danger); }

  .stat-label {
    margin-bottom: 6px;
    color: var(--text-dim);
    font-size: 10px;
    font-weight: 600;
    letter-spacing: 0.06em;
    text-transform: uppercase;
  }

  .stat-value {
    color: var(--text);
    font-size: 22px;
    font-weight: 700;
    line-height: 1;
    white-space: nowrap;
    font-variant-numeric: tabular-nums;
  }

  .stat:nth-child(1) .stat-value { color: var(--accent); }
  .stat:nth-child(2) .stat-value { color: var(--warning); }
  .stat:nth-child(3) .stat-value { color: var(--success); }
  .stat:nth-child(4) .stat-value { color: #ffffff; }
  .stat:nth-child(5) .stat-value { color: var(--accent2); }

  /* ── Tab Buttons for Panel View ── */
  .panel-tabs {
    display: none;
  }

  /* ── Responsive ── */
  @media (max-width: 900px) {
    .panels {
      grid-template-columns: 1fr;
    }

    .panel-tabs {
      display: flex;
      gap: 8px;
      padding: 0;
    }

    .panel-tabs button {
      flex: 1;
      height: 36px;
      font-size: 12px;
    }

    .panel.mobile-hidden {
      display: none;
    }

    .stats {
      grid-template-columns: repeat(3, 1fr);
      gap: 8px;
    }

    .content {
      padding: 10px;
      gap: 10px;
    }
  }

  @media (max-width: 480px) {
    .stats {
      grid-template-columns: repeat(2, 1fr);
    }

    .topbar {
      flex-direction: column;
      align-items: flex-start;
      gap: 8px;
    }
  }

  /* ── Fullscreen mode ── */
  .panel:fullscreen,
  .panel:-webkit-full-screen {
    background: #000;
    border: none;
    border-radius: 0;
  }

  .panel:fullscreen .panel-header,
  .panel:-webkit-full-screen .panel-header {
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    z-index: 10;
    background: rgba(0,0,0,0.7);
    backdrop-filter: blur(8px);
    opacity: 0;
    transition: opacity 0.3s;
  }

  .panel:fullscreen:hover .panel-header,
  .panel:-webkit-full-screen:hover .panel-header {
    opacity: 1;
  }

  /* ── Event Log ── */
  .event-ticker {
    padding: 6px 14px;
    border-top: 1px solid var(--border);
    background: rgba(0,0,0,0.3);
    font-size: 11px;
    color: var(--text-dim);
    display: flex;
    align-items: center;
    gap: 8px;
    overflow: hidden;
    flex-shrink: 0;
  }

  .event-ticker .dot {
    width: 5px;
    height: 5px;
    border-radius: 50%;
    background: var(--accent);
    flex-shrink: 0;
    animation: pulse-dot 1.5s ease-in-out infinite;
  }

  .event-ticker .msg {
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
</style>
</head>
<body>
  <main class="shell" id="app">
    <!-- Topbar -->
    <header class="topbar">
      <div class="brand">
        <div class="mark">O</div>
        
      </div>
      <div class="topbar-right">
        <div class="panel-tabs" id="panel-tabs">
          <button class="active" id="tab-osd" onclick="switchTab('osd')">OSD Stream</button>
          <button id="tab-board" onclick="switchTab('board')">Board</button>
        </div>
        <div class="connection" id="connection-status">
          <span class="connection-dot"></span>
          <span>Connecting…</span>
        </div>
      </div>
    </header>

    <!-- Panels -->
    <section class="content">
      <div class="panels">
        <!-- OSD Panel -->
        <div class="panel" id="osd-panel">
          <div class="panel-header">
            <div class="panel-title">
              <span>OSD Video Stream</span>
              <span class="badge badge-live">● LIVE</span>
            </div>
            <div class="controls">
              <button id="btn-pause-osd" onclick="toggleOsdStream()">Pause</button>
              <button onclick="goFullscreen('osd-panel')">Fullscreen</button>
            </div>
          </div>
          <div class="stream-wrap" id="osd-container">
            <img id="osd-stream" alt="OSD video stream">
            <div class="placeholder" id="osd-placeholder">
              <div class="placeholder-spinner"></div>
              <span>Connecting to OSD stream…</span>
            </div>
          </div>
        </div>

        <!-- Right Column: Board + Result -->
        <div class="right-column">
          <!-- Board Panel -->
          <div class="panel" id="board-panel">
            <div class="panel-header">
              <div class="panel-title">
                <span>Analysis Board</span>
                <span class="badge badge-board">PIPELINE</span>
              </div>
              <div class="controls">
                <button id="btn-pause-board" onclick="toggleBoardStream()">Pause</button>
                <button onclick="goFullscreen('board-panel')">Fullscreen</button>
              </div>
            </div>
            <div class="stream-wrap" id="board-container">
              <img id="board-stream" alt="Analysis board pipeline">
              <div class="placeholder" id="board-placeholder">
                <div class="placeholder-spinner"></div>
                <span>Connecting to Analysis Board…</span>
              </div>
            </div>
          </div>

          <!-- Result Panel -->
          <div class="panel" id="result-panel">
            <div class="panel-header">
              <div class="panel-title">
                <span>Result</span>
                <span class="badge badge-result">FACES</span>
                <span id="result-count" style="font-size:10px;color:var(--text-dim)"></span>
              </div>
            </div>
            <div class="result-grid" id="result-grid">
              <div class="result-empty">Waiting for face detections…</div>
            </div>
          </div>
        </div>
      </div>

      <!-- Stats -->
      <section class="stats" id="stats-bar">
        <div class="stat">
          <div class="stat-label">FPS</div>
          <div class="stat-value" id="stat-fps">--</div>
        </div>
        <div class="stat">
          <div class="stat-label">Latency</div>
          <div class="stat-value" id="stat-latency">--</div>
        </div>
        <div class="stat">
          <div class="stat-label">Objects</div>
          <div class="stat-value" id="stat-objects">--</div>
        </div>
        <div class="stat">
          <div class="stat-label">Queue</div>
          <div class="stat-value" id="stat-queue">--</div>
        </div>
        <div class="stat">
          <div class="stat-label">Uptime</div>
          <div class="stat-value" id="stat-uptime">--</div>
        </div>
        <div class="stat">
          <div class="stat-label">Events</div>
          <div class="stat-value" id="stat-events">0</div>
        </div>
      </section>
    </section>

    <!-- Event ticker -->
    <footer class="event-ticker" id="event-ticker">
      <span class="dot"></span>
      <span class="msg" id="event-msg">Waiting for detection events…</span>
    </footer>
  </main>

<script>
  /* ═══════════════════════════════════════
   * Snapshot-based streaming (cross-browser)
   * Uses fetch() to pull individual JPEG frames
   * ═══════════════════════════════════════ */
  let osdActive = true;
  let boardActive = true;
  let eventCount = 0;

  const osdStream = document.getElementById('osd-stream');
  const osdPlaceholder = document.getElementById('osd-placeholder');
  const boardStream = document.getElementById('board-stream');
  const boardPlaceholder = document.getElementById('board-placeholder');

  // Remove MJPEG src — we'll drive updates via JS
  osdStream.removeAttribute('src');
  boardStream.removeAttribute('src');

  // Snapshot fetch loop for a given img element
  function startSnapshotLoop(img, placeholder, snapshotUrl, intervalMs, isActiveFunc) {
    let prevUrl = null;
    let running = true;
    let firstFrame = false;

    async function fetchFrame() {
      if (!running || !isActiveFunc()) {
        setTimeout(fetchFrame, intervalMs);
        return;
      }
      try {
        const resp = await fetch(snapshotUrl + '?t=' + Date.now());
        if (resp.ok && resp.headers.get('content-type')?.includes('image')) {
          const blob = await resp.blob();
          const url = URL.createObjectURL(blob);
          img.onload = function() {
            if (prevUrl) URL.revokeObjectURL(prevUrl);
            prevUrl = url;
          };
          img.src = url;
          if (!firstFrame) {
            firstFrame = true;
            img.style.display = 'block';
            placeholder.classList.add('hidden');
          }
        }
      } catch(e) {
        // Server might be restarting
      }
      setTimeout(fetchFrame, intervalMs);
    }

    fetchFrame();
    return { stop: function() { running = false; }, start: function() { running = true; } };
  }

  // Start loops
  const osdLoop = startSnapshotLoop(osdStream, osdPlaceholder, '/snapshot/osd', 66, () => osdActive);
  const boardLoop = startSnapshotLoop(boardStream, boardPlaceholder, '/snapshot/board', 500, () => boardActive);

  // ── Result panel: poll face crops ──
  let resultActive = true;
  async function pollResult() {
    if (!resultActive) { setTimeout(pollResult, 1000); return; }
    try {
      const res = await fetch('/api/faces', { cache: 'no-store' });
      if (res.ok) {
        const faces = await res.json();
        const grid = document.getElementById('result-grid');
        const counter = document.getElementById('result-count');
        if (faces && faces.length > 0) {
          const validFaces = faces.filter(f => f.label && !f.label.startsWith('['));
          const displayFaces = validFaces.slice(-10);
          counter.textContent = '(' + displayFaces.length + ' shown / ' + validFaces.length + ' total)';
          let html = '';
          for (const f of displayFaces) {
            const cls = f.score > 0 ? 'identified' : 'unknown';
            html += '<div class="result-card ' + cls + '">';
            html += '<img src="/snapshot/face/' + f.id + '?t=' + Date.now() + '" alt="face">';
            html += '<div class="result-label">' + (f.label || 'Unknown') + '</div>';
            html += '</div>';
          }
          grid.innerHTML = html;
        } else {
          counter.textContent = '';
          grid.innerHTML = '<div class="result-empty">No faces detected</div>';
        }
      }
    } catch(_) {}
    setTimeout(pollResult, 500);
  }
  pollResult();

  function toggleOsdStream() {
    osdActive = !osdActive;
    const btn = document.getElementById('btn-pause-osd');
    btn.textContent = osdActive ? 'Pause' : 'Resume';
    osdActive ? btn.classList.remove('active') : btn.classList.add('active');
  }

  function toggleBoardStream() {
    boardActive = !boardActive;
    const btn = document.getElementById('btn-pause-board');
    btn.textContent = boardActive ? 'Pause' : 'Resume';
    boardActive ? btn.classList.remove('active') : btn.classList.add('active');
  }

  function goFullscreen(panelId) {
    const el = document.getElementById(panelId);
    if (el.requestFullscreen) el.requestFullscreen();
    else if (el.webkitRequestFullscreen) el.webkitRequestFullscreen();
  }

  /* ═══════════════════════════════════════
   * Mobile tab switch
   * ═══════════════════════════════════════ */
  function switchTab(tab) {
    const osdPanel = document.getElementById('osd-panel');
    const boardPanel = document.getElementById('board-panel');
    const tabOsd = document.getElementById('tab-osd');
    const tabBoard = document.getElementById('tab-board');

    if (tab === 'osd') {
      osdPanel.classList.remove('mobile-hidden');
      boardPanel.classList.add('mobile-hidden');
      tabOsd.classList.add('active');
      tabBoard.classList.remove('active');
    } else {
      osdPanel.classList.add('mobile-hidden');
      boardPanel.classList.remove('mobile-hidden');
      tabOsd.classList.remove('active');
      tabBoard.classList.add('active');
    }
  }

  /* ═══════════════════════════════════════
   * Stats polling
   * ═══════════════════════════════════════ */
  function formatUptime(sec) {
    const h = Math.floor(sec / 3600);
    const m = Math.floor((sec % 3600) / 60);
    const s = Math.floor(sec % 60);
    if (h > 0) return h + 'h ' + m + 'm';
    return m + 'm ' + s + 's';
  }

  async function pollStats() {
    const status = document.getElementById('connection-status');
    try {
      const res = await fetch('/api/stats', { cache: 'no-store' });
      const data = await res.json();

      document.getElementById('stat-fps').textContent = (data.fps || 0).toFixed(1);
      document.getElementById('stat-latency').textContent = (data.latency_ms || 0) + 'ms';
      document.getElementById('stat-objects').textContent = data.object_count || 0;
      document.getElementById('stat-queue').textContent = data.queue_size || 0;
      document.getElementById('stat-uptime').textContent = formatUptime(data.uptime_sec || 0);

      status.classList.add('connected');
      status.querySelector('span:last-child').textContent = 'Connected';
    } catch (_) {
      status.classList.remove('connected');
      status.querySelector('span:last-child').textContent = 'Disconnected';
    }
  }

  setInterval(pollStats, 1000);
  pollStats();

  /* ═══════════════════════════════════════
   * SSE Events
   * ═══════════════════════════════════════ */
  function connectSSE() {
    const es = new EventSource('/events');
    const ticker = document.getElementById('event-msg');
    const counter = document.getElementById('stat-events');

    es.onmessage = function(e) {
      try {
        const data = JSON.parse(e.data);
        eventCount++;
        counter.textContent = eventCount;
        const ts = new Date(data.timestamp).toLocaleTimeString();
        ticker.textContent = '[' + ts + '] ch' + data.channel + ' — ' + data.targets_count + ' target(s) detected';
      } catch (_) {}
    };

    es.onerror = function() {
      es.close();
      ticker.textContent = 'SSE disconnected — reconnecting…';
      setTimeout(connectSSE, 3000);
    };
  }

  connectSSE();
</script>
</body>
</html>
)HTML";

} // namespace cvedix_nodes
