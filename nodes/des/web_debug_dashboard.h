#pragma once

#include <string>

namespace cvedix_nodes {

inline const std::string WEB_DEBUG_DASHBOARD_HTML = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>OmniCore OSD Web Debug</title>
<style>
  :root {
    --bg: #000;
    --panel: #050505;
    --border: #303030;
    --border-strong: #777;
    --text: #fff;
    --muted: #9a9a9a;
  }

  * {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
  }

  body {
    min-height: 100vh;
    background: var(--bg);
    color: var(--text);
    font-family: Arial, Helvetica, sans-serif;
    overflow-x: hidden;
  }

  .shell {
    min-height: 100vh;
    display: grid;
    grid-template-rows: auto 1fr auto;
  }

  .topbar {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 16px;
    padding: 14px 18px;
    background: #000;
    border-bottom: 1px solid var(--border);
  }

  .brand {
    display: flex;
    align-items: center;
    gap: 10px;
    min-width: 0;
  }

  .mark {
    width: 28px;
    height: 28px;
    display: grid;
    place-items: center;
    background: #fff;
    color: #000;
    font-size: 14px;
    font-weight: 700;
  }

  h1 {
    font-size: 17px;
    font-weight: 600;
    line-height: 1.2;
    white-space: nowrap;
  }

  .connection {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 5px 10px;
    border: 1px solid var(--border-strong);
    color: var(--text);
    font-size: 12px;
    white-space: nowrap;
  }

  .connection-dot {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    background: #fff;
  }

  .viewer {
    min-height: 0;
    padding: 16px;
  }

  .viewer-frame {
    height: 100%;
    min-height: calc(100vh - 174px);
    display: grid;
    grid-template-rows: auto 1fr;
    border: 1px solid var(--border);
    background: var(--panel);
  }

  .viewer-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    padding: 10px 12px;
    border-bottom: 1px solid var(--border);
    color: var(--muted);
    font-size: 12px;
    font-weight: 700;
    letter-spacing: 0.04em;
    text-transform: uppercase;
  }

  .controls {
    display: flex;
    align-items: center;
    gap: 8px;
  }

  button {
    min-width: 82px;
    height: 30px;
    border: 1px solid var(--border-strong);
    background: #000;
    color: #fff;
    font: inherit;
    font-size: 12px;
    cursor: pointer;
  }

  button:hover,
  button.active {
    background: #fff;
    color: #000;
  }

  .stream-wrap {
    position: relative;
    min-height: 0;
    display: grid;
    place-items: center;
    background: #000;
  }

  .stream-wrap img {
    width: 100%;
    height: 100%;
    object-fit: contain;
  }

  .placeholder {
    position: absolute;
    inset: 0;
    display: grid;
    place-items: center;
    color: var(--muted);
    font-size: 14px;
    text-align: center;
    background: #000;
  }

  .placeholder.hidden {
    display: none;
  }

  .stats {
    display: grid;
    grid-template-columns: repeat(5, minmax(0, 1fr));
    gap: 1px;
    padding: 0 16px 16px;
    background: #000;
  }

  .stat {
    min-width: 0;
    padding: 12px;
    border: 1px solid var(--border);
    background: var(--panel);
  }

  .label {
    margin-bottom: 5px;
    color: var(--muted);
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.04em;
    text-transform: uppercase;
  }

  .value {
    color: #fff;
    font-size: 22px;
    font-weight: 700;
    line-height: 1;
    white-space: nowrap;
  }

  @media (max-width: 760px) {
    .topbar {
      align-items: flex-start;
      flex-direction: column;
    }

    h1 {
      white-space: normal;
    }

    .viewer {
      padding: 10px;
    }

    .viewer-frame {
      min-height: auto;
    }

    .stream-wrap {
      aspect-ratio: 16 / 9;
    }

    .stats {
      grid-template-columns: repeat(2, minmax(0, 1fr));
      padding: 0 10px 10px;
    }
  }
</style>
</head>
<body>
  <main class="shell">
    <header class="topbar">
      <div class="brand">
        <div class="mark">O</div>
        <h1>OmniCore OSD Web Debug</h1>
      </div>
      <div class="connection" id="connection-status">
        <span class="connection-dot"></span>
        <span>Connecting</span>
      </div>
    </header>

    <section class="viewer">
      <div class="viewer-frame">
        <div class="viewer-header">
          <span>OSD Web Debug</span>
          <div class="controls">
            <button id="btn-pause-osd" onclick="toggleStream()">Pause</button>
            <button onclick="goFullscreen()">Fullscreen</button>
          </div>
        </div>
        <div class="stream-wrap" id="osd-container">
          <img id="osd-stream" src="/stream/osd" alt="OSD Web Debug stream">
          <div class="placeholder" id="osd-placeholder">Connecting to OSD stream...</div>
        </div>
      </div>
    </section>

    <section class="stats">
      <div class="stat">
        <div class="label">FPS</div>
        <div class="value" id="stat-fps">--</div>
      </div>
      <div class="stat">
        <div class="label">Latency</div>
        <div class="value" id="stat-latency">--</div>
      </div>
      <div class="stat">
        <div class="label">Objects</div>
        <div class="value" id="stat-objects">--</div>
      </div>
      <div class="stat">
        <div class="label">Queue</div>
        <div class="value" id="stat-queue">--</div>
      </div>
      <div class="stat">
        <div class="label">Uptime</div>
        <div class="value" id="stat-uptime">--</div>
      </div>
    </section>
  </main>

<script>
  let streamActive = true;

  const osdStream = document.getElementById('osd-stream');
  const placeholder = document.getElementById('osd-placeholder');
  const pauseButton = document.getElementById('btn-pause-osd');

  osdStream.onload = function() {
    osdStream.style.display = 'block';
    placeholder.classList.add('hidden');
  };

  osdStream.onerror = function() {
    osdStream.style.display = 'none';
    placeholder.classList.remove('hidden');
  };

  function toggleStream() {
    streamActive = !streamActive;
    if (streamActive) {
      osdStream.src = '/stream/osd?t=' + Date.now();
      pauseButton.textContent = 'Pause';
      pauseButton.classList.remove('active');
    } else {
      osdStream.src = '';
      pauseButton.textContent = 'Resume';
      pauseButton.classList.add('active');
    }
  }

  function goFullscreen() {
    const el = document.getElementById('osd-container');
    if (el.requestFullscreen) el.requestFullscreen();
    else if (el.webkitRequestFullscreen) el.webkitRequestFullscreen();
  }

  function formatUptime(sec) {
    const h = Math.floor(sec / 3600);
    const m = Math.floor((sec % 3600) / 60);
    const s = Math.floor(sec % 60);
    return (h > 0 ? h + 'h ' : '') + m + 'm ' + s + 's';
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

      status.querySelector('span:last-child').textContent = 'Connected';
      status.style.borderColor = '#777';
    } catch (_) {
      status.querySelector('span:last-child').textContent = 'Disconnected';
      status.style.borderColor = '#fff';
    }
  }

  setInterval(pollStats, 1000);
  pollStats();
</script>
</body>
</html>
)HTML";

} // namespace cvedix_nodes
