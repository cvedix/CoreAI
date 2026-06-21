#pragma once

#include <string>

namespace cvedix_nodes {

inline const std::string WEB_DEBUG_DASHBOARD_HTML = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>RapidMedia — AI Vision Dashboard</title>
<meta name="description" content="RapidMedia real-time AI vision pipeline dashboard with OSD stream, analysis board and system metrics">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700;800;900&family=JetBrains+Mono:wght@400;500;600&display=swap" rel="stylesheet">
<style>
  :root {
    --bg-primary: #0a0a0f;
    --bg-secondary: #12121a;
    --bg-card: rgba(18,18,28,0.85);
    --bg-card-solid: #14141f;
    --border: rgba(255,255,255,0.08);
    --border-hover: rgba(255,255,255,0.16);
    --border-accent: rgba(99,102,241,0.4);
    --text: #f0f0f5;
    --text-muted: #8b8ba0;
    --text-dim: #55556a;
    --accent: #6366f1;
    --accent-light: #818cf8;
    --accent-glow: rgba(99,102,241,0.35);
    --cyan: #22d3ee;
    --cyan-glow: rgba(34,211,238,0.3);
    --emerald: #34d399;
    --emerald-glow: rgba(52,211,153,0.3);
    --amber: #fbbf24;
    --amber-glow: rgba(251,191,36,0.3);
    --rose: #fb7185;
    --rose-glow: rgba(251,113,133,0.3);
    --violet: #a78bfa;
    --violet-glow: rgba(167,139,250,0.3);
    --sky: #38bdf8;
    --sky-glow: rgba(56,189,248,0.3);
    --gradient-hero: linear-gradient(135deg, #6366f1 0%, #8b5cf6 50%, #a78bfa 100%);
    --gradient-cyan: linear-gradient(135deg, #06b6d4, #22d3ee);
    --gradient-emerald: linear-gradient(135deg, #10b981, #34d399);
    --gradient-amber: linear-gradient(135deg, #f59e0b, #fbbf24);
    --gradient-rose: linear-gradient(135deg, #f43f5e, #fb7185);
    --gradient-sky: linear-gradient(135deg, #0ea5e9, #38bdf8);
    --gradient-violet: linear-gradient(135deg, #7c3aed, #a78bfa);
    --radius: 16px;
    --radius-sm: 10px;
    --radius-xs: 6px;
    --shadow-card: 0 4px 24px rgba(0,0,0,0.4), 0 0 0 1px var(--border);
    --shadow-glow-accent: 0 0 30px var(--accent-glow), 0 0 60px rgba(99,102,241,0.1);
    --transition: 0.25s cubic-bezier(0.4, 0, 0.2, 1);
    --glass: rgba(18,18,28,0.6);
  }

  * { box-sizing: border-box; margin: 0; padding: 0; }

  body {
    min-height: 100vh;
    background: var(--bg-primary);
    background-image:
      radial-gradient(ellipse 80% 60% at 50% 0%, rgba(99,102,241,0.08) 0%, transparent 50%),
      radial-gradient(ellipse 60% 50% at 80% 100%, rgba(34,211,238,0.05) 0%, transparent 50%);
    color: var(--text);
    font-family: 'Inter', -apple-system, BlinkMacSystemFont, sans-serif;
    overflow-x: hidden;
    -webkit-font-smoothing: antialiased;
  }

  ::-webkit-scrollbar { width: 5px; }
  ::-webkit-scrollbar-track { background: transparent; }
  ::-webkit-scrollbar-thumb { background: var(--border-hover); border-radius: 3px; }

  /* ── Shell ── */
  .shell { min-height: 100vh; display: flex; flex-direction: column; }

  /* ── Topbar ── */
  .topbar {
    position: sticky; top: 0; z-index: 100;
    display: flex; align-items: center; justify-content: space-between;
    gap: 16px; padding: 10px 24px;
    background: rgba(10,10,15,0.75);
    backdrop-filter: blur(24px) saturate(1.6);
    -webkit-backdrop-filter: blur(24px) saturate(1.6);
    border-bottom: 1px solid var(--border);
  }

  .brand { display: flex; align-items: center; gap: 14px; min-width: 0; }

  .mark {
    width: 36px; height: 36px;
    display: grid; place-items: center;
    background: var(--gradient-hero);
    color: #fff; font-size: 15px; font-weight: 900;
    border-radius: var(--radius-xs);
    box-shadow: var(--shadow-glow-accent);
    letter-spacing: -0.02em;
  }

  .brand-text { display: flex; flex-direction: column; gap: 1px; }

  .brand-title {
    font-size: 15px; font-weight: 700; line-height: 1.2;
    background: var(--gradient-hero);
    -webkit-background-clip: text; -webkit-text-fill-color: transparent;
    background-clip: text;
  }

  .brand-sub {
    font-size: 10px; font-weight: 500; color: var(--text-dim);
    letter-spacing: 0.08em; text-transform: uppercase;
  }

  .topbar-right { display: flex; align-items: center; gap: 14px; }

  /* ── System Info Chips ── */
  .sys-chips { display: flex; gap: 8px; }

  .sys-chip {
    display: flex; align-items: center; gap: 5px;
    padding: 4px 10px; border-radius: 20px;
    background: rgba(255,255,255,0.04);
    border: 1px solid var(--border);
    font-size: 10px; font-weight: 600; color: var(--text-muted);
    font-family: 'JetBrains Mono', monospace;
    letter-spacing: 0.02em;
    transition: var(--transition);
  }

  .sys-chip:hover { border-color: var(--border-hover); background: rgba(255,255,255,0.06); }
  .sys-chip .icon { font-size: 12px; }
  .sys-chip.gpu { color: var(--emerald); border-color: rgba(52,211,153,0.2); }
  .sys-chip.cpu { color: var(--cyan); border-color: rgba(34,211,238,0.2); }
  .sys-chip.mem { color: var(--violet); border-color: rgba(167,139,250,0.2); }

  .connection {
    display: flex; align-items: center; gap: 8px;
    padding: 6px 14px; border: 1px solid var(--border);
    border-radius: 20px; color: var(--text-dim);
    font-size: 11px; font-weight: 600; white-space: nowrap;
    transition: var(--transition);
  }

  .connection.connected { border-color: var(--emerald); color: var(--emerald); }

  .connection-dot {
    width: 7px; height: 7px; border-radius: 50%;
    background: var(--text-dim); transition: var(--transition);
  }

  .connection.connected .connection-dot {
    background: var(--emerald);
    box-shadow: 0 0 8px var(--emerald-glow);
    animation: pulse-dot 2s ease-in-out infinite;
  }

  @keyframes pulse-dot { 0%,100%{opacity:1} 50%{opacity:0.4} }
  /* ── Collapsible Board Section ── */
  .board-section {
    overflow: hidden;
    transition: max-height 0.4s cubic-bezier(0.4, 0, 0.2, 1), opacity 0.3s ease;
    max-height: 400px;
    opacity: 1;
  }

  .board-section.collapsed {
    max-height: 0;
    opacity: 0;
  }

  .board-toggle {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    width: 100%;
    padding: 8px 16px;
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    background: rgba(99,102,241,0.06);
    color: var(--accent-light);
    font-family: 'Inter', sans-serif;
    font-size: 12px;
    font-weight: 600;
    cursor: pointer;
    transition: var(--transition);
    letter-spacing: 0.04em;
    min-width: unset;
    height: 36px;
  }

  .board-toggle:hover {
    background: rgba(99,102,241,0.12);
    border-color: var(--border-accent);
    color: #fff;
  }

  .board-toggle .arrow {
    display: inline-block;
    transition: transform 0.3s ease;
    font-size: 10px;
  }

  .board-toggle.active .arrow {
    transform: rotate(180deg);
  }

  .board-panel-inner {
    border: 1px solid var(--border);
    border-radius: var(--radius);
    background: var(--bg-card);
    backdrop-filter: blur(12px);
    overflow: hidden;
    margin-top: 8px;
  }

  .board-panel-inner .panel-header {
    display: flex; align-items: center; justify-content: space-between;
    gap: 12px; padding: 8px 16px;
    border-bottom: 1px solid var(--border);
    background: rgba(0,0,0,0.25);
  }

  .badge-board {
    background: rgba(99,102,241,0.12); color: var(--accent-light);
    border: 1px solid rgba(99,102,241,0.25);
  }

  .board-stream-wrap {
    position: relative;
    display: flex; align-items: center; justify-content: center;
    background: #000;
    min-height: 220px;
    max-height: 340px;
    overflow: hidden;
  }

  .board-stream-wrap img {
    width: 100%; height: auto; max-height: 340px;
    object-fit: contain; display: block;
    image-rendering: -webkit-optimize-contrast;
  }
  /* ── Main Content ── */
  .content { flex: 1; padding: 16px 24px; display: flex; flex-direction: column; gap: 16px; }

  /* ── Panels Grid — 1 column, board below ── */
  .panels-row {
    display: grid;
    grid-template-columns: 1fr;
    gap: 16px;
    flex: 1;
    min-height: 350px;
  }



  /* ── Panel ── */
  .panel {
    display: flex; flex-direction: column;
    border: 1px solid var(--border);
    border-radius: var(--radius);
    background: var(--bg-card);
    backdrop-filter: blur(12px);
    overflow: hidden;
    transition: border-color var(--transition), box-shadow var(--transition);
    min-height: 0;
  }

  .panel:hover { border-color: var(--border-hover); box-shadow: var(--shadow-card); }

  .panel-header {
    display: flex; align-items: center; justify-content: space-between;
    gap: 12px; padding: 10px 16px;
    border-bottom: 1px solid var(--border);
    background: rgba(0,0,0,0.25);
    flex-shrink: 0;
  }

  .panel-title {
    display: flex; align-items: center; gap: 10px;
    font-size: 12px; font-weight: 600;
    letter-spacing: 0.04em; text-transform: uppercase; color: var(--text-muted);
  }

  .badge {
    display: inline-flex; align-items: center;
    padding: 2px 8px; font-size: 9px; font-weight: 700;
    border-radius: 4px; text-transform: uppercase; letter-spacing: 0.08em;
  }

  .badge-live {
    background: rgba(52,211,153,0.12); color: var(--emerald);
    border: 1px solid rgba(52,211,153,0.25);
    animation: badge-pulse 2.5s ease-in-out infinite;
  }

  @keyframes badge-pulse { 0%,100%{opacity:1} 50%{opacity:0.6} }



  .controls { display: flex; align-items: center; gap: 6px; }

  button {
    min-width: 72px; height: 28px;
    border: 1px solid var(--border-hover); border-radius: 6px;
    background: rgba(255,255,255,0.04); color: var(--text-muted);
    font-family: 'Inter', sans-serif; font-size: 11px; font-weight: 600;
    cursor: pointer; transition: var(--transition);
  }

  button:hover { background: rgba(255,255,255,0.08); color: var(--text); }

  button.active {
    background: var(--accent); color: #fff;
    border-color: var(--accent); box-shadow: 0 0 16px var(--accent-glow);
  }

  /* ── Stream ── */
  .stream-wrap {
    position: relative; flex: 1; min-height: 280px;
    display: flex; align-items: center; justify-content: center;
    background: #000; overflow: hidden;
  }



  .stream-wrap img {
    width: 100%; height: auto; max-height: 100%;
    object-fit: contain; display: block;
  }

  .placeholder {
    position: absolute; inset: 0;
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    gap: 14px; color: var(--text-dim); font-size: 12px; font-weight: 500;
    background: rgba(0,0,0,0.7); backdrop-filter: blur(6px);
    transition: opacity var(--transition);
  }

  .placeholder.hidden { opacity:0; pointer-events:none; visibility:hidden; display:none; }

  .placeholder-spinner {
    width: 30px; height: 30px;
    border: 2px solid var(--border-hover); border-top-color: var(--accent);
    border-radius: 50%; animation: spin 0.8s linear infinite;
  }

  @keyframes spin { to{transform:rotate(360deg)} }

  /* ── Stats Grid ── */
  .stats {
    display: grid;
    grid-template-columns: repeat(6, 1fr);
    gap: 12px; flex-shrink: 0;
  }

  .stat {
    padding: 16px 18px; border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    background: var(--bg-card);
    backdrop-filter: blur(8px);
    transition: border-color var(--transition), transform var(--transition), box-shadow var(--transition);
    position: relative; overflow: hidden;
  }

  .stat:hover {
    border-color: var(--border-hover);
    transform: translateY(-2px);
    box-shadow: 0 8px 24px rgba(0,0,0,0.3);
  }

  .stat::before {
    content: ''; position: absolute; top: 0; left: 0; right: 0;
    height: 2px; border-radius: 2px 2px 0 0;
  }

  .stat::after {
    content: ''; position: absolute; top: 0; left: 0; right: 0; bottom: 0;
    opacity: 0; transition: opacity var(--transition);
    pointer-events: none; border-radius: inherit;
  }

  .stat:hover::after { opacity: 1; }

  .stat:nth-child(1)::before { background: var(--gradient-emerald); }
  .stat:nth-child(1)::after  { background: radial-gradient(circle at top, rgba(52,211,153,0.06), transparent 70%); }
  .stat:nth-child(2)::before { background: var(--gradient-amber); }
  .stat:nth-child(2)::after  { background: radial-gradient(circle at top, rgba(251,191,36,0.06), transparent 70%); }
  .stat:nth-child(3)::before { background: var(--gradient-cyan); }
  .stat:nth-child(3)::after  { background: radial-gradient(circle at top, rgba(34,211,238,0.06), transparent 70%); }
  .stat:nth-child(4)::before { background: var(--gradient-violet); }
  .stat:nth-child(4)::after  { background: radial-gradient(circle at top, rgba(167,139,250,0.06), transparent 70%); }
  .stat:nth-child(5)::before { background: var(--gradient-sky); }
  .stat:nth-child(5)::after  { background: radial-gradient(circle at top, rgba(56,189,248,0.06), transparent 70%); }
  .stat:nth-child(6)::before { background: var(--gradient-rose); }
  .stat:nth-child(6)::after  { background: radial-gradient(circle at top, rgba(251,113,133,0.06), transparent 70%); }

  .stat-icon {
    font-size: 16px; margin-bottom: 8px; display: block;
    filter: drop-shadow(0 0 6px currentColor);
  }

  .stat:nth-child(1) .stat-icon { color: var(--emerald); }
  .stat:nth-child(2) .stat-icon { color: var(--amber); }
  .stat:nth-child(3) .stat-icon { color: var(--cyan); }
  .stat:nth-child(4) .stat-icon { color: var(--violet); }
  .stat:nth-child(5) .stat-icon { color: var(--sky); }
  .stat:nth-child(6) .stat-icon { color: var(--rose); }

  .stat-label {
    margin-bottom: 6px; color: var(--text-dim);
    font-size: 10px; font-weight: 600;
    letter-spacing: 0.06em; text-transform: uppercase;
  }

  .stat-value {
    font-size: 24px; font-weight: 800; line-height: 1;
    white-space: nowrap; font-variant-numeric: tabular-nums;
    font-family: 'JetBrains Mono', 'Inter', monospace;
  }

  .stat:nth-child(1) .stat-value { color: var(--emerald); }
  .stat:nth-child(2) .stat-value { color: var(--amber); }
  .stat:nth-child(3) .stat-value { color: var(--cyan); }
  .stat:nth-child(4) .stat-value { color: var(--violet); }
  .stat:nth-child(5) .stat-value { color: var(--sky); }
  .stat:nth-child(6) .stat-value { color: var(--rose); }

  .stat-unit {
    font-size: 11px; font-weight: 500; color: var(--text-dim);
    margin-left: 2px;
  }

  /* ── Tab Buttons (Mobile) ── */
  .panel-tabs { display: none; }

  /* ── Event Log ── */
  .event-ticker {
    padding: 8px 20px;
    border-top: 1px solid var(--border);
    background: rgba(0,0,0,0.3);
    backdrop-filter: blur(8px);
    font-size: 11px; color: var(--text-dim);
    display: flex; align-items: center; gap: 10px;
    overflow: hidden; flex-shrink: 0;
    font-family: 'JetBrains Mono', monospace;
  }

  .event-ticker .dot {
    width: 6px; height: 6px; border-radius: 50%;
    background: var(--accent); flex-shrink: 0;
    animation: pulse-dot 1.5s ease-in-out infinite;
    box-shadow: 0 0 8px var(--accent-glow);
  }

  .event-ticker .msg {
    white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
  }

  /* ── Fullscreen ── */
  .panel:fullscreen, .panel:-webkit-full-screen { background: #000; border: none; border-radius: 0; }

  .panel:fullscreen .panel-header, .panel:-webkit-full-screen .panel-header {
    position: absolute; top: 0; left: 0; right: 0; z-index: 10;
    background: rgba(0,0,0,0.7); backdrop-filter: blur(8px);
    opacity: 0; transition: opacity 0.3s;
  }

  .panel:fullscreen:hover .panel-header, .panel:-webkit-full-screen:hover .panel-header { opacity: 1; }

  /* ── Responsive ── */
  @media (max-width: 900px) {
    .panels-row { grid-template-columns: 1fr; }
    .panel-tabs { display: flex; gap: 8px; }
    .panel-tabs button { flex: 1; height: 36px; font-size: 12px; }
    .panel.mobile-hidden { display: none; }
    .stats { grid-template-columns: repeat(3, 1fr); gap: 8px; }
    .content { padding: 10px; gap: 10px; }
    .sys-chips { display: none; }
  }

  @media (max-width: 480px) {
    .stats { grid-template-columns: repeat(2, 1fr); }
    .topbar { padding: 10px 14px; }
  }
</style>
</head>
<body>
  <main class="shell" id="app">
    <!-- Topbar -->
    <header class="topbar">
      <div class="brand">
        <div class="mark">R</div>
        <div class="brand-text">
          <div class="brand-title">RapidMedia</div>
          <div class="brand-sub">AI Vision Pipeline</div>
        </div>
      </div>
      <div class="topbar-right">
        <div class="sys-chips">
          <div class="sys-chip gpu"><span class="icon">⬢</span> <span id="chip-gpu">RTX 3080</span></div>
          <div class="sys-chip cpu"><span class="icon">◈</span> <span id="chip-cpu">--</span></div>
          <div class="sys-chip mem"><span class="icon">◆</span> <span id="chip-mem">--</span></div>
        </div>
        <div class="panel-tabs" id="panel-tabs">
          <button class="active" id="tab-osd" onclick="switchTab('osd')">OSD</button>
        </div>
        <div class="connection" id="connection-status">
          <span class="connection-dot"></span>
          <span>Connecting…</span>
        </div>
      </div>
    </header>

    <!-- Content -->
    <section class="content">
      <!-- Video Streams Row -->
      <div class="panels-row">
        <!-- OSD Panel -->
        <div class="panel" id="osd-panel">
          <div class="panel-header">
            <div class="panel-title">
              <span>AI Detection</span>
              <span class="badge badge-live">● LIVE</span>
            </div>
            <div class="controls">
              <button id="btn-pause-osd" onclick="toggleOsdStream()">Pause</button>
              <button onclick="goFullscreen('osd-panel')">⛶</button>
            </div>
          </div>
          <div class="stream-wrap" id="osd-container">
            <img id="osd-stream" alt="OSD video stream">
            <div class="placeholder" id="osd-placeholder">
              <div class="placeholder-spinner"></div>
              <span>Connecting to AI Detection stream…</span>
            </div>
          </div>
        </div>
      </div>

      <!-- Analysis Board (Collapsible) -->
      <button class="board-toggle active" id="board-toggle" onclick="toggleBoard()">
        <span class="arrow">▼</span> Pipeline Analysis Board
      </button>
      <div class="board-section" id="board-section">
        <div class="board-panel-inner">
          <div class="panel-header">
            <div class="panel-title">
              <span>Pipeline Topology</span>
              <span class="badge badge-board">LIVE</span>
            </div>
            <div class="controls">
              <button id="btn-pause-board" onclick="toggleBoardStream()">Pause</button>
              <button onclick="goFullscreenBoard()">⛶</button>
            </div>
          </div>
          <div class="board-stream-wrap" id="board-container">
            <img id="board-stream" alt="Pipeline analysis board">
            <div class="placeholder" id="board-placeholder">
              <div class="placeholder-spinner"></div>
              <span>Loading Pipeline Board…</span>
            </div>
          </div>
        </div>
      </div>

      <!-- Stats -->
      <section class="stats" id="stats-bar">
        <div class="stat">
          <span class="stat-icon">▶</span>
          <div class="stat-label">FPS</div>
          <div class="stat-value"><span id="stat-fps">--</span></div>
        </div>
        <div class="stat">
          <span class="stat-icon">⏱</span>
          <div class="stat-label">Latency</div>
          <div class="stat-value"><span id="stat-latency">--</span><span class="stat-unit">ms</span></div>
        </div>
        <div class="stat">
          <span class="stat-icon">◎</span>
          <div class="stat-label">Objects</div>
          <div class="stat-value"><span id="stat-objects">--</span></div>
        </div>
        <div class="stat">
          <span class="stat-icon">≡</span>
          <div class="stat-label">Queue</div>
          <div class="stat-value"><span id="stat-queue">--</span></div>
        </div>
        <div class="stat">
          <span class="stat-icon">◷</span>
          <div class="stat-label">Uptime</div>
          <div class="stat-value" id="stat-uptime">--</div>
        </div>
        <div class="stat">
          <span class="stat-icon">⚡</span>
          <div class="stat-label">Events</div>
          <div class="stat-value"><span id="stat-events">0</span></div>
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
   * ═══════════════════════════════════════ */
  let osdActive = true;
  let boardActive = true;
  let boardVisible = true;
  let eventCount = 0;


  const osdStream = document.getElementById('osd-stream');
  const osdPlaceholder = document.getElementById('osd-placeholder');
  const boardStream = document.getElementById('board-stream');
  const boardPlaceholder = document.getElementById('board-placeholder');


  osdStream.removeAttribute('src');
  boardStream.removeAttribute('src');

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
      } catch(e) {}
      setTimeout(fetchFrame, intervalMs);
    }

    fetchFrame();
    return { stop: function() { running = false; }, start: function() { running = true; } };
  }

  const osdLoop = startSnapshotLoop(osdStream, osdPlaceholder, '/snapshot/osd', 250, () => osdActive);
  const boardLoop = startSnapshotLoop(boardStream, boardPlaceholder, '/snapshot/board', 400, () => boardActive && boardVisible);

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

  function toggleBoard() {
    const section = document.getElementById('board-section');
    const btn = document.getElementById('board-toggle');
    boardVisible = !boardVisible;
    section.classList.toggle('collapsed');
    btn.classList.toggle('active');
  }

  function goFullscreenBoard() {
    const el = document.getElementById('board-container');
    if (el.requestFullscreen) el.requestFullscreen();
    else if (el.webkitRequestFullscreen) el.webkitRequestFullscreen();
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
    // Single tab behavior
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
      document.getElementById('stat-latency').textContent = (data.latency_ms || 0);
      document.getElementById('stat-objects').textContent = data.object_count || 0;
      document.getElementById('stat-queue').textContent = data.queue_size || 0;
      document.getElementById('stat-uptime').textContent = formatUptime(data.uptime_sec || 0);

      // Update system chips if available
      if (data.gpu_name) document.getElementById('chip-gpu').textContent = data.gpu_name;
      if (data.gpu_util !== undefined) document.getElementById('chip-gpu').textContent = (data.gpu_name || 'GPU') + ' ' + data.gpu_util + '%';
      if (data.cpu_percent !== undefined) document.getElementById('chip-cpu').textContent = 'CPU ' + data.cpu_percent + '%';
      if (data.mem_used_gb !== undefined) document.getElementById('chip-mem').textContent = data.mem_used_gb + ' GB';

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
