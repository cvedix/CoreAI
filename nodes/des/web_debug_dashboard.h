#pragma once

#include <string>

namespace cvedix_nodes {

inline const std::string WEB_DEBUG_DASHBOARD_HTML = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>OmniCore Debug Dashboard</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap');

  :root {
    --bg-primary: #0a0e1a;
    --bg-secondary: #111827;
    --bg-card: rgba(17, 24, 39, 0.8);
    --border: rgba(75, 85, 99, 0.4);
    --accent: #00d4ff;
    --accent-glow: rgba(0, 212, 255, 0.15);
    --text-primary: #f3f4f6;
    --text-secondary: #9ca3af;
    --text-muted: #6b7280;
    --success: #10b981;
    --warning: #f59e0b;
    --danger: #ef4444;
    --purple: #8b5cf6;
  }

  * { margin: 0; padding: 0; box-sizing: border-box; }

  body {
    font-family: 'Inter', -apple-system, BlinkMacSystemFont, sans-serif;
    background: var(--bg-primary);
    color: var(--text-primary);
    min-height: 100vh;
    overflow-x: hidden;
  }

  /* Header */
  .header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 12px 24px;
    background: var(--bg-secondary);
    border-bottom: 1px solid var(--border);
    backdrop-filter: blur(12px);
    position: sticky;
    top: 0;
    z-index: 100;
  }

  .header-left {
    display: flex;
    align-items: center;
    gap: 12px;
  }

  .logo {
    width: 32px;
    height: 32px;
    border-radius: 8px;
    background: linear-gradient(135deg, var(--accent), var(--purple));
    display: flex;
    align-items: center;
    justify-content: center;
    font-weight: 700;
    font-size: 14px;
    color: white;
  }

  .header h1 {
    font-size: 18px;
    font-weight: 600;
    background: linear-gradient(90deg, var(--accent), var(--purple));
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
  }

  .header-right {
    display: flex;
    align-items: center;
    gap: 16px;
  }

  .status-badge {
    display: flex;
    align-items: center;
    gap: 6px;
    padding: 4px 12px;
    border-radius: 20px;
    font-size: 12px;
    font-weight: 500;
    background: rgba(16, 185, 129, 0.15);
    color: var(--success);
    border: 1px solid rgba(16, 185, 129, 0.3);
  }

  .status-dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--success);
    animation: pulse 2s infinite;
  }

  @keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.4; }
  }

  /* Main layout */
  .main {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 16px;
    padding: 16px 24px;
  }

  @media (max-width: 1024px) {
    .main { grid-template-columns: 1fr; }
  }

  /* Cards */
  .card {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: 12px;
    overflow: hidden;
    backdrop-filter: blur(12px);
    transition: border-color 0.3s;
  }

  .card:hover {
    border-color: rgba(0, 212, 255, 0.3);
  }

  .card-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 12px 16px;
    border-bottom: 1px solid var(--border);
    background: rgba(0, 0, 0, 0.2);
  }

  .card-title {
    display: flex;
    align-items: center;
    gap: 8px;
    font-size: 13px;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    color: var(--text-secondary);
  }

  .card-title .icon { font-size: 16px; }

  .card-body {
    position: relative;
    background: #000;
    aspect-ratio: 16/9;
    display: flex;
    align-items: center;
    justify-content: center;
  }

  .card-body img {
    width: 100%;
    height: 100%;
    object-fit: contain;
  }

  .stream-placeholder {
    color: var(--text-muted);
    font-size: 14px;
    text-align: center;
  }

  .stream-placeholder .spinner {
    width: 32px;
    height: 32px;
    border: 3px solid var(--border);
    border-top-color: var(--accent);
    border-radius: 50%;
    animation: spin 1s linear infinite;
    margin: 0 auto 12px;
  }

  @keyframes spin {
    to { transform: rotate(360deg); }
  }

  /* Controls */
  .card-controls {
    display: flex;
    align-items: center;
    gap: 8px;
  }

  .btn {
    padding: 4px 10px;
    border: 1px solid var(--border);
    border-radius: 6px;
    background: transparent;
    color: var(--text-secondary);
    font-size: 11px;
    font-family: inherit;
    cursor: pointer;
    transition: all 0.2s;
  }

  .btn:hover {
    border-color: var(--accent);
    color: var(--accent);
    background: var(--accent-glow);
  }

  .btn.active {
    border-color: var(--accent);
    color: var(--accent);
    background: var(--accent-glow);
  }

  /* Stats bar */
  .stats-bar {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
    gap: 12px;
    padding: 0 24px 16px;
  }

  .stat-card {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 14px 16px;
    backdrop-filter: blur(12px);
    transition: border-color 0.3s;
  }

  .stat-card:hover { border-color: rgba(0, 212, 255, 0.3); }

  .stat-label {
    font-size: 11px;
    font-weight: 500;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    color: var(--text-muted);
    margin-bottom: 4px;
  }

  .stat-value {
    font-size: 24px;
    font-weight: 700;
    color: var(--text-primary);
  }

  .stat-value.accent { color: var(--accent); }
  .stat-value.success { color: var(--success); }
  .stat-value.warning { color: var(--warning); }
  .stat-value.purple { color: var(--purple); }

  /* Event log */
  .event-log-container {
    grid-column: 1 / -1;
    margin: 0 24px 24px;
  }

  .event-log {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: 12px;
    overflow: hidden;
    backdrop-filter: blur(12px);
  }

  .event-log-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 12px 16px;
    border-bottom: 1px solid var(--border);
    background: rgba(0, 0, 0, 0.2);
  }

  .event-log-title {
    font-size: 13px;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    color: var(--text-secondary);
  }

  .event-counter {
    font-size: 11px;
    padding: 2px 8px;
    border-radius: 10px;
    background: var(--accent-glow);
    color: var(--accent);
    border: 1px solid rgba(0, 212, 255, 0.3);
  }

  .event-log-body {
    max-height: 240px;
    overflow-y: auto;
    padding: 8px;
    font-family: 'SF Mono', 'Fira Code', monospace;
    font-size: 12px;
    line-height: 1.6;
  }

  .event-log-body::-webkit-scrollbar { width: 6px; }
  .event-log-body::-webkit-scrollbar-track { background: transparent; }
  .event-log-body::-webkit-scrollbar-thumb {
    background: var(--border);
    border-radius: 3px;
  }

  .event-entry {
    padding: 4px 8px;
    border-radius: 4px;
    margin-bottom: 2px;
    color: var(--text-secondary);
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    animation: fadeIn 0.3s ease;
  }

  .event-entry:hover {
    background: rgba(255,255,255,0.05);
    white-space: normal;
    word-break: break-all;
  }

  @keyframes fadeIn {
    from { opacity: 0; transform: translateY(-4px); }
    to { opacity: 1; transform: translateY(0); }
  }

  .event-time { color: var(--text-muted); }
  .event-type { color: var(--accent); font-weight: 600; }

  .no-events {
    color: var(--text-muted);
    text-align: center;
    padding: 24px;
    font-size: 13px;
  }

  /* Fullscreen button */
  .fullscreen-btn {
    position: absolute;
    top: 8px;
    right: 8px;
    width: 28px;
    height: 28px;
    border-radius: 6px;
    background: rgba(0,0,0,0.6);
    border: 1px solid rgba(255,255,255,0.2);
    color: white;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 14px;
    opacity: 0;
    transition: opacity 0.2s;
    z-index: 10;
  }

  .card-body:hover .fullscreen-btn { opacity: 1; }
  .fullscreen-btn:hover { background: rgba(0,212,255,0.3); border-color: var(--accent); }

  /* Quality slider */
  .quality-control {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 11px;
    color: var(--text-muted);
  }

  .quality-control input[type="range"] {
    width: 60px;
    height: 3px;
    -webkit-appearance: none;
    background: var(--border);
    border-radius: 2px;
    outline: none;
  }

  .quality-control input[type="range"]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 12px;
    height: 12px;
    border-radius: 50%;
    background: var(--accent);
    cursor: pointer;
  }
</style>
</head>
<body>
  <!-- Header -->
  <header class="header">
    <div class="header-left">
      <div class="logo">O</div>
      <h1>OmniCore Debug Dashboard</h1>
    </div>
    <div class="header-right">
      <div class="quality-control">
        <span>Quality</span>
        <input type="range" id="quality" min="20" max="95" value="75">
        <span id="quality-val">75%</span>
      </div>
      <div class="status-badge" id="connection-status">
        <span class="status-dot"></span>
        <span>Connected</span>
      </div>
    </div>
  </header>

  <!-- Video panels -->
  <div class="main">
    <!-- OSD Panel -->
    <div class="card">
      <div class="card-header">
        <div class="card-title">
          <span class="icon">📹</span> OSD Video Stream
        </div>
        <div class="card-controls">
          <button class="btn" id="btn-pause-osd" onclick="toggleStream('osd')">⏸ Pause</button>
        </div>
      </div>
      <div class="card-body" id="osd-container">
        <img id="osd-stream" src="/stream/osd" alt="OSD Stream"
             onerror="this.style.display='none';document.getElementById('osd-placeholder').style.display='block'"
             onload="this.style.display='block';document.getElementById('osd-placeholder').style.display='none'">
        <div class="stream-placeholder" id="osd-placeholder">
          <div class="spinner"></div>
          Connecting to OSD stream...
        </div>
        <button class="fullscreen-btn" onclick="goFullscreen('osd-container')">⛶</button>
      </div>
    </div>

    <!-- Analysis Board Panel -->
    <div class="card">
      <div class="card-header">
        <div class="card-title">
          <span class="icon">📊</span> Analysis Board
        </div>
        <div class="card-controls">
          <button class="btn" id="btn-pause-board" onclick="toggleStream('board')">⏸ Pause</button>
        </div>
      </div>
      <div class="card-body" id="board-container">
        <img id="board-stream" src="/stream/board" alt="Analysis Board"
             onerror="this.style.display='none';document.getElementById('board-placeholder').style.display='block'"
             onload="this.style.display='block';document.getElementById('board-placeholder').style.display='none'">
        <div class="stream-placeholder" id="board-placeholder">
          <div class="spinner"></div>
          Connecting to Analysis Board...
        </div>
        <button class="fullscreen-btn" onclick="goFullscreen('board-container')">⛶</button>
      </div>
    </div>
  </div>

  <!-- Stats bar -->
  <div class="stats-bar">
    <div class="stat-card">
      <div class="stat-label">FPS (in)</div>
      <div class="stat-value accent" id="stat-fps">--</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Latency</div>
      <div class="stat-value success" id="stat-latency">--</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Objects</div>
      <div class="stat-value purple" id="stat-objects">--</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Queue</div>
      <div class="stat-value warning" id="stat-queue">--</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Uptime</div>
      <div class="stat-value" id="stat-uptime">--</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Events</div>
      <div class="stat-value accent" id="stat-events">0</div>
    </div>
  </div>

  <!-- Event log -->
  <div class="event-log-container">
    <div class="event-log">
      <div class="event-log-header">
        <div class="event-log-title">🔔 Detection Events (SSE)</div>
        <div>
          <button class="btn" onclick="clearLog()">Clear</button>
          <span class="event-counter" id="event-count-badge">0 events</span>
        </div>
      </div>
      <div class="event-log-body" id="event-log">
        <div class="no-events">Waiting for events...</div>
      </div>
    </div>
  </div>

<script>
  // --- Stream toggle ---
  const streamState = { osd: true, board: true };

  function toggleStream(type) {
    const img = document.getElementById(type + '-stream');
    const btn = document.getElementById('btn-pause-' + type);
    streamState[type] = !streamState[type];
    if (streamState[type]) {
      img.src = '/stream/' + type + '?t=' + Date.now();
      btn.textContent = '⏸ Pause';
      btn.classList.remove('active');
    } else {
      img.src = '';
      btn.textContent = '▶ Resume';
      btn.classList.add('active');
    }
  }

  // --- Fullscreen ---
  function goFullscreen(id) {
    const el = document.getElementById(id);
    if (el.requestFullscreen) el.requestFullscreen();
    else if (el.webkitRequestFullscreen) el.webkitRequestFullscreen();
  }

  // --- Quality ---
  const qualitySlider = document.getElementById('quality');
  const qualityVal = document.getElementById('quality-val');
  qualitySlider.addEventListener('input', function() {
    qualityVal.textContent = this.value + '%';
  });

  // --- Stats polling ---
  let eventCount = 0;

  async function pollStats() {
    try {
      const res = await fetch('/api/stats');
      const data = await res.json();
      document.getElementById('stat-fps').textContent = (data.fps || 0).toFixed(1);
      document.getElementById('stat-latency').textContent = (data.latency_ms || 0) + 'ms';
      document.getElementById('stat-objects').textContent = data.object_count || 0;
      document.getElementById('stat-queue').textContent = data.queue_size || 0;
      document.getElementById('stat-uptime').textContent = formatUptime(data.uptime_sec || 0);

      const statusEl = document.getElementById('connection-status');
      statusEl.querySelector('span:last-child').textContent = 'Connected';
      statusEl.style.color = '';
    } catch (e) {
      const statusEl = document.getElementById('connection-status');
      statusEl.querySelector('span:last-child').textContent = 'Disconnected';
      statusEl.style.color = 'var(--danger)';
    }
  }

  function formatUptime(sec) {
    const h = Math.floor(sec / 3600);
    const m = Math.floor((sec % 3600) / 60);
    const s = Math.floor(sec % 60);
    return (h > 0 ? h + 'h ' : '') + m + 'm ' + s + 's';
  }

  setInterval(pollStats, 1000);
  pollStats();

  // --- SSE Events ---
  const eventLog = document.getElementById('event-log');
  const eventCountBadge = document.getElementById('event-count-badge');
  const statEvents = document.getElementById('stat-events');

  function connectSSE() {
    const source = new EventSource('/events');
    source.onmessage = function(e) {
      eventCount++;
      statEvents.textContent = eventCount;
      eventCountBadge.textContent = eventCount + ' events';

      // Remove placeholder
      const noEvents = eventLog.querySelector('.no-events');
      if (noEvents) noEvents.remove();

      const now = new Date().toLocaleTimeString();
      const entry = document.createElement('div');
      entry.className = 'event-entry';

      let display = e.data;
      try {
        const obj = JSON.parse(e.data);
        display = JSON.stringify(obj);
      } catch(_) {}

      entry.innerHTML = '<span class="event-time">[' + now + ']</span> ' +
                         '<span class="event-type">EVENT</span> ' + display;
      eventLog.insertBefore(entry, eventLog.firstChild);

      // Keep max 200 entries
      while (eventLog.children.length > 200) {
        eventLog.removeChild(eventLog.lastChild);
      }
    };

    source.onerror = function() {
      setTimeout(connectSSE, 3000);
    };
  }

  connectSSE();

  function clearLog() {
    eventLog.innerHTML = '<div class="no-events">Log cleared. Waiting for events...</div>';
    eventCount = 0;
    statEvents.textContent = '0';
    eventCountBadge.textContent = '0 events';
  }
</script>
</body>
</html>
)HTML";

} // namespace cvedix_nodes
