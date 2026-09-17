#!/usr/bin/env bash
#
# Register the CVEDIX PPE violation pipeline with a HeraMind instance:
# device type, device, MQTT broker, transform and dashboard.
#
# Mirrors HeraMind/examples/heracam-rv1126b/setup.sh. The MQTT settings default
# to the same HERAMIND_* variables ppe_video_sample reads, so one set of exports
# configures both the camera and this registration.
#
# Usage:
#   export HERAMIND_API_KEY=...            # or HERAMIND_TOKEN
#   export HERAMIND_MQTT_HOST=192.168.1.239
#   export HERAMIND_MQTT_TOPIC=heramind/ppe/events
#   export HERAMIND_CAMERA_ID=camera-01
#   bash integrations/heramind_ppe/setup.sh
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
API_BASE="${HERAMIND_API_BASE:-http://127.0.0.1:9375/api}"
API_BASE="${API_BASE%/}"

BROKER_ID="${CVEDIX_PPE_BROKER_ID:-cvedix-ppe-broker}"
BROKER_HOST="${HERAMIND_MQTT_HOST:-127.0.0.1}"
BROKER_PORT="${HERAMIND_MQTT_PORT:-1883}"
BROKER_TOPIC="${HERAMIND_MQTT_TOPIC:-heramind/ppe/events}"
# The sample's --camera-id is what HeraMind stores as instance_id, and the
# device_id registered here must match it or the transform scope misses events.
DEVICE_ID="${HERAMIND_CAMERA_ID:-camera-01}"
DEVICE_NAME="${CVEDIX_PPE_DEVICE_NAME:-CVEDIX PPE Camera ${DEVICE_ID}}"
DEVICE_TYPE="cvedix_ppe_violation_analytics"
TRANSFORM_NAME="CVEDIX PPE Violation Normalizer"
DASHBOARD_NAME="CVEDIX PPE - An toàn lao động"

for command_name in curl jq; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "Missing required command: $command_name" >&2
    exit 1
  fi
done

CURL_AUTH=()
if [[ -n "${HERAMIND_API_KEY:-}" ]]; then
  CURL_AUTH=(-H "X-API-Key: ${HERAMIND_API_KEY}")
elif [[ -n "${HERAMIND_TOKEN:-}" ]]; then
  CURL_AUTH=(-H "Authorization: Bearer ${HERAMIND_TOKEN}")
else
  echo "Set HERAMIND_API_KEY or HERAMIND_TOKEN before running this script." >&2
  exit 1
fi

request() {
  local method="$1"
  local path="$2"
  local body_file="${3:-}"
  local response_file
  local status

  response_file="$(mktemp)"
  if [[ -n "$body_file" ]]; then
    status="$(curl -sS -o "$response_file" -w '%{http_code}' \
      -X "$method" \
      "${CURL_AUTH[@]}" \
      -H 'Content-Type: application/json' \
      --data-binary "@${body_file}" \
      "${API_BASE}${path}")"
  else
    status="$(curl -sS -o "$response_file" -w '%{http_code}' \
      -X "$method" \
      "${CURL_AUTH[@]}" \
      "${API_BASE}${path}")"
  fi

  if (( status < 200 || status >= 300 )); then
    echo "HeraMind API ${method} ${path} failed with HTTP ${status}:" >&2
    jq . "$response_file" >&2 2>/dev/null || sed -n '1,120p' "$response_file" >&2
    rm -f "$response_file"
    exit 1
  fi

  cat "$response_file"
  rm -f "$response_file"
}

make_temp_json() {
  mktemp --suffix=.json
}

echo "Checking HeraMind at ${API_BASE}..."
request GET "/settings/timezone" >/dev/null

echo "Registering CVEDIX PPE device type..."
request POST "/device-types" "${SCRIPT_DIR}/device-type.json" >/dev/null

device_payload="$(make_temp_json)"
jq -n \
  --arg device_type "$DEVICE_TYPE" \
  --arg device_id "$DEVICE_ID" \
  --arg name "$DEVICE_NAME" \
  --arg topic "$BROKER_TOPIC" \
  --arg broker_id "$BROKER_ID" \
  --arg broker_host "$BROKER_HOST" \
  --argjson broker_port "$BROKER_PORT" \
  '{
    device_type: $device_type,
    device_id: $device_id,
    name: $name,
    adapter_type: "mqtt",
    connection_config: {
      telemetry_topic: $topic,
      broker_id: $broker_id,
      manufacturer: "CVEDIX",
      model: "Core Runtime",
      device_class: "PPE Safety Camera",
      simulated: false,
      mqtt_host: $broker_host,
      mqtt_port: $broker_port
    }
  }' > "$device_payload"

devices="$(request GET "/devices?limit=1000")"
if jq -e --arg id "$DEVICE_ID" \
  '(.data.devices // .devices // []) | any((.device_id // .id) == $id)' \
  >/dev/null <<<"$devices"; then
  echo "Updating device ${DEVICE_ID}..."
  update_device_payload="$(make_temp_json)"
  jq '{name, adapter_type, connection_config}' "$device_payload" > "$update_device_payload"
  request PUT "/devices/${DEVICE_ID}" "$update_device_payload" >/dev/null
  rm -f "$update_device_payload"
else
  echo "Creating device ${DEVICE_ID}..."
  request POST "/devices" "$device_payload" >/dev/null
fi
rm -f "$device_payload"

broker_payload="$(make_temp_json)"
jq -n \
  --arg id "$BROKER_ID" \
  --arg name "CVEDIX PPE MQTT" \
  --arg broker "$BROKER_HOST" \
  --argjson port "$BROKER_PORT" \
  --arg topic "$BROKER_TOPIC" \
  --arg username "${HERAMIND_MQTT_USERNAME:-}" \
  --arg password "${HERAMIND_MQTT_PASSWORD:-}" \
  '{
    id: $id,
    name: $name,
    broker: $broker,
    port: $port,
    tls: false,
    enabled: true,
    client_id: "heramind-cvedix-ppe",
    subscribe_topics: [$topic]
  }
  + (if $username == "" then {} else {username: $username} end)
  + (if $password == "" then {} else {password: $password} end)' \
  > "$broker_payload"

brokers="$(request GET "/brokers")"
if jq -e --arg id "$BROKER_ID" \
  '(.data.brokers // .brokers // []) | any(.id == $id)' \
  >/dev/null <<<"$brokers"; then
  echo "Updating external MQTT broker ${BROKER_HOST}:${BROKER_PORT}/${BROKER_TOPIC}..."
  request PUT "/brokers/${BROKER_ID}" "$broker_payload" >/dev/null
else
  echo "Creating external MQTT broker ${BROKER_HOST}:${BROKER_PORT}/${BROKER_TOPIC}..."
  request POST "/brokers" "$broker_payload" >/dev/null
fi
rm -f "$broker_payload"

transform_code="$(<"${SCRIPT_DIR}/transform.js")"
transform_payload="$(make_temp_json)"
jq -n \
  --arg name "$TRANSFORM_NAME" \
  --arg device_id "$DEVICE_ID" \
  --arg code "$transform_code" \
  '{
    name: $name,
    description: "Normalize CVEDIX Core Runtime PPE violation events, person crop images and PPE attributes into dashboard metrics.",
    enabled: true,
    type: "transform",
    definition: {
      scope: {device: $device_id},
      intent: "Build PPE safety metrics from CVEDIX Core Runtime MQTT events",
      js_code: $code,
      output_prefix: "cvedix_ppe",
      complexity: 2
    }
  }' > "$transform_payload"

automations="$(request GET "/automations?type=transform&limit=1000")"
transform_id="$(jq -r --arg name "$TRANSFORM_NAME" \
  '(.data.automations // .automations // [])[]? | select(.name == $name) | .id' \
  <<<"$automations" | head -n 1)"

if [[ -n "$transform_id" ]]; then
  echo "Updating transform ${transform_id}..."
  request PUT "/automations/${transform_id}" "$transform_payload" >/dev/null
else
  echo "Creating CVEDIX PPE transform..."
  transform_response="$(request POST "/automations" "$transform_payload")"
  transform_id="$(jq -r '.data.automation.id // .automation.id // empty' <<<"$transform_response")"
fi
rm -f "$transform_payload"

if [[ -z "$transform_id" ]]; then
  echo "Could not determine the CVEDIX PPE transform ID." >&2
  exit 1
fi

transform_source() {
  local metric="$1"
  local aggregate="${2:-latest}"
  local time_range="${3:-1}"
  local limit="${4:-100}"
  jq -n \
    --arg transform_id "$transform_id" \
    --arg metric "cvedix_ppe.${metric}" \
    --arg aggregate "$aggregate" \
    --argjson time_range "$time_range" \
    --argjson limit "$limit" \
    '{
      type: "transform",
      sourceId: ("transform:" + $transform_id),
      transformId: $transform_id,
      metricId: $metric,
      timeRange: $time_range,
      limit: $limit,
      aggregateExt: $aggregate,
      source: "transform",
      id: $transform_id,
      field: $metric,
      mode: "timeseries"
    }'
}

mqtt_message_count_source() {
  jq -n \
    --arg device_id "$DEVICE_ID" \
    '{
      type: "telemetry",
      sourceId: $device_id,
      metricId: "_raw",
      timeRange: 24,
      limit: 1,
      aggregateExt: "count",
      source: "device",
      id: $device_id,
      field: "_raw",
      mode: "timeseries"
    }'
}

violation_trend_source() {
  jq -n \
    --arg device_id "$DEVICE_ID" \
    '{
      type: "telemetry",
      sourceId: $device_id,
      metricId: "cvedix_ppe.ppe_violation_seen_numeric",
      timeRange: 6,
      limit: 3000,
      aggregateExt: "raw",
      source: "device",
      id: $device_id,
      field: "cvedix_ppe.ppe_violation_seen_numeric",
      mode: "timeseries",
      transform: "raw",
      params: {includeRawPoints: true},
      timeWindow: {type: "last_6hours"}
    }'
}

crop_image_source() {
  jq -n \
    --arg transform_id "$transform_id" \
    '{
      type: "transform",
      sourceId: ("transform:" + $transform_id),
      transformId: $transform_id,
      metricId: "cvedix_ppe.crop_image",
      timeRange: 48,
      limit: 200,
      aggregateExt: "raw",
      source: "transform",
      id: $transform_id,
      field: "cvedix_ppe.crop_image",
      mode: "timeseries",
      transform: "raw",
      params: {includeRawPoints: true, isImage: true}
    }'
}

raw_event_source() {
  jq -n \
    --arg device_id "$DEVICE_ID" \
    '{
      type: "telemetry",
      sourceId: $device_id,
      metricId: "_raw",
      timeRange: 48,
      limit: 200,
      aggregateExt: "raw",
      source: "device",
      id: $device_id,
      field: "_raw",
      mode: "timeseries",
      transform: "raw",
      params: {includeRawPoints: true}
    }'
}

component() {
  local id="$1"
  local type="$2"
  local title="$3"
  local x="$4"
  local y="$5"
  local w="$6"
  local h="$7"
  local data_source="${8:-null}"
  local config="${9-}"
  if [[ -z "$config" ]]; then
    config='{}'
  fi

  jq -n \
    --arg id "$id" \
    --arg type "$type" \
    --arg title "$title" \
    --argjson x "$x" \
    --argjson y "$y" \
    --argjson w "$w" \
    --argjson h "$h" \
    --argjson data_source "$data_source" \
    --argjson config "$config" \
    '{
      id: $id,
      type: $type,
      title: $title,
      position: {x: $x, y: $y, w: $w, h: $h},
      data_source: $data_source,
      config: $config
    }'
}

components="$(jq -n '[]')"
device_info_content="$(printf '# CVEDIX PPE Camera\n\n- **Thiết bị:** %s\n- **MQTT:** %s:%s · topic `%s`\n- **Event chính:** `event-ppe-violation`, `crop`, `attribute`\n- **Attribute:** `has_helmet`, `has_vest`, `missing_helmet`, `missing_vest`, `ppe_status`, `person_confidence`, `track_id`\n- **Toạ độ:** `location` chuẩn hoá [0,1] theo khung hình gốc' "$DEVICE_ID" "$BROKER_HOST" "$BROKER_PORT" "$BROKER_TOPIC")"
device_markdown="$(jq -n --arg content "$device_info_content" '{content: $content, variant: "default"}')"

components="$(jq --argjson item "$(component cvedix-ppe-info markdown-display "Thông tin thiết bị" 0 0 6 3 null "$device_markdown")" '. + [$item]' <<<"$components")"
components="$(jq --argjson item "$(component cvedix-ppe-total value-card "Vi phạm PPE · 24 giờ" 6 0 3 3 "$(transform_source ppe_violation_seen count 24 1)" '{"size":"lg","variant":"danger","showTrend":true,"icon":"alert-triangle"}')" '. + [$item]' <<<"$components")"
components="$(jq --argjson item "$(component cvedix-ppe-mqtt-count value-card "MQTT messages · 24 giờ" 9 0 3 3 "$(mqtt_message_count_source)" '{"size":"lg","variant":"default","showTrend":false,"icon":"radio"}')" '. + [$item]' <<<"$components")"

crop_config='{"fit":"contain","rounded":true,"zoomable":true,"downloadable":true,"showTitle":true}'
crop_history_config='{"fit":"contain","rounded":true,"showTimestamp":true,"showIndex":true,"limit":200,"timeRange":48}'
components="$(jq --argjson item "$(component cvedix-ppe-crop-latest image-display "Ảnh crop người vi phạm mới nhất" 0 3 5 5 "$(crop_image_source)" "$crop_config")" '. + [$item]' <<<"$components")"
components="$(jq --argjson item "$(component cvedix-ppe-trend line-chart "Số vi phạm mỗi phút · 6 giờ" 5 3 7 5 "$(violation_trend_source)" '{"showLegend":false,"showGrid":true,"showTooltip":true,"smooth":false,"fillArea":true,"size":"lg","dataMapping":{"timeAggregate":"1m","aggregate":"sum","fillMissingBuckets":true}}')" '. + [$item]' <<<"$components")"

for spec in \
  "ppe_status|Trạng thái PPE|0|8|3|2|shield-alert" \
  "missing_ppe|PPE còn thiếu|3|8|3|2|list-x" \
  "has_helmet|Có mũ bảo hộ|6|8|3|2|hard-hat" \
  "has_vest|Có áo phản quang|9|8|3|2|vest" \
  "person_confidence|Độ tin cậy người|0|10|3|2|scan-search" \
  "event_time|Thời gian event|3|10|3|2|clock"; do
  IFS='|' read -r metric title x y w h icon <<<"$spec"
  components="$(jq --argjson item "$(component "cvedix-ppe-${metric}" value-card "$title" "$x" "$y" "$w" "$h" "$(transform_source "$metric")" "{\"size\":\"md\",\"variant\":\"default\",\"showTrend\":false,\"icon\":\"${icon}\"}")" '. + [$item]' <<<"$components")"
done

event_list_config='{"limit":200,"timeRange":48,"pageSize":10,"showSearch":true,"showFilters":true,"showImage":true}'
components="$(jq --argjson item "$(component cvedix-ppe-event-list event-list "Danh sách người vi phạm và attribute · 48 giờ" 0 12 12 7 "$(raw_event_source)" "$event_list_config")" '. + [$item]' <<<"$components")"
components="$(jq --argjson item "$(component cvedix-ppe-crop-history image-history "Lịch sử ảnh crop · 48 giờ" 0 19 12 5 "$(crop_image_source)" "$crop_history_config")" '. + [$item]' <<<"$components")"

dashboard_payload="$(make_temp_json)"
jq -n \
  --arg name "$DASHBOARD_NAME" \
  --argjson components "$components" \
  '{
    name: $name,
    layout: {
      columns: 12,
      rows: "auto",
      breakpoints: {lg: 1200, md: 996, sm: 768, xs: 480}
    },
    components: $components
  }' > "$dashboard_payload"

dashboards="$(request GET "/dashboards")"
dashboard_id="$(jq -r --arg name "$DASHBOARD_NAME" \
  '(.data.dashboards // .dashboards // [])[]? | select(.name == $name) | .id' \
  <<<"$dashboards" | head -n 1)"

if [[ -n "$dashboard_id" ]]; then
  echo "Updating dashboard ${dashboard_id}..."
  request PUT "/dashboards/${dashboard_id}" "$dashboard_payload" >/dev/null
else
  echo "Creating CVEDIX PPE dashboard..."
  dashboard_response="$(request POST "/dashboards" "$dashboard_payload")"
  dashboard_id="$(jq -r '.data.id // .id // empty' <<<"$dashboard_response")"
fi
rm -f "$dashboard_payload"

echo
echo "CVEDIX PPE setup complete."
echo "Device:    ${DEVICE_ID}"
echo "Broker:    mqtt://${BROKER_HOST}:${BROKER_PORT}/${BROKER_TOPIC}"
echo "Transform: ${transform_id}"
echo "Dashboard: ${dashboard_id:-$DASHBOARD_NAME}"
echo "Web UI:    ${API_BASE%/api}"
