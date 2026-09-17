// HeraMind transform: normalise CVEDIX PPE violation events into dashboard metrics.
//
// Input is the JSON array cvedix_ppe_event_node publishes on the MQTT topic:
// one `event-ppe-violation` item, one `attribute` item per reported fact, and —
// when a person crop was encoded — a `crop` item carrying the base64 JPEG.
// Output is the flat metric object HeraMind stores for the device.

const raw = input_raw._raw
  || (input_raw.values && input_raw.values._raw)
  || input;

let payload = raw;
if (typeof payload === 'string') {
  try {
    payload = JSON.parse(payload);
  } catch (_) {
    payload = [];
  }
}

const records = Array.isArray(payload)
  ? payload
  : Array.isArray(payload && payload.events)
    ? payload.events
    : Array.isArray(payload && payload.data)
      ? payload.data
      : payload && typeof payload === 'object'
        ? [payload]
        : [];

function asNumber(value, fallback = 0) {
  const number = Number(value);
  return Number.isFinite(number) ? number : fallback;
}

function cleanString(value, fallback = '') {
  if (value === null || value === undefined) return fallback;
  const text = String(value).trim();
  return text || fallback;
}

// Attribute values travel as strings ("true"/"false") and as numbers; accept
// both so a producer change does not silently flip a safety metric.
function asBoolean(value) {
  if (typeof value === 'boolean') return value;
  const text = cleanString(value).toLowerCase();
  if (text === 'true' || text === '1' || text === 'yes') return true;
  if (text === 'false' || text === '0' || text === 'no') return false;
  return false;
}

function normalizeImage(value) {
  if (typeof value === 'string' && value.trim()) {
    const text = value.trim();
    if (/^\/9j\//.test(text)) return `data:image/jpeg;base64,${text}`;
    if (/^iVBOR/.test(text)) return `data:image/png;base64,${text}`;
    if (/^(data:image\/|https?:\/\/|\/)/i.test(text)) return text;
    return text;
  }
  if (!value || typeof value !== 'object') return '';

  for (const key of ['url', 'src', 'image_url', 'imageUrl', 'image', 'data', 'base64', 'value', 'path']) {
    const nested = value[key];
    const image = normalizeImage(nested);
    if (image) return image;
  }
  return '';
}

function getLocation(item) {
  const location = item && item.location && typeof item.location === 'object'
    ? item.location
    : {};
  return {
    x: asNumber(location.x),
    y: asNumber(location.y),
    width: asNumber(location.width),
    height: asNumber(location.height)
  };
}

// A violation is anything the runtime tagged as an event: the payload only ever
// carries this array on the unsafe-person branch.
const violations = records.filter(item =>
  item &&
  typeof item === 'object' &&
  (String(item.$id || '').startsWith('event-') ||
   cleanString(item.object_class).toLowerCase() === 'person')
);

const eventIds = new Set(violations.map(item => cleanString(item.event_id)).filter(Boolean));

const attributes = {};
for (const item of records) {
  if (!item || typeof item !== 'object' || item.$id !== 'attribute') continue;
  const name = cleanString(item.name);
  if (!name) continue;
  attributes[name] = item.value;
}

const crops = records.filter(item =>
  item &&
  typeof item === 'object' &&
  item.$id === 'crop' &&
  normalizeImage(item.image || item)
);

// Prefer a crop that belongs to this event, then the most confident one.
crops.sort((left, right) => {
  const leftMatches = eventIds.has(cleanString(left.ref_event_id)) ? 1 : 0;
  const rightMatches = eventIds.has(cleanString(right.ref_event_id)) ? 1 : 0;
  if (leftMatches !== rightMatches) return rightMatches - leftMatches;
  return asNumber(right.confidence) - asNumber(left.confidence);
});

const primaryEvent = violations[0] || records.find(item => item && typeof item === 'object') || {};
const bestCrop = crops[0] || null;
const metadataSource = primaryEvent || bestCrop || {};
const cropImage = bestCrop ? normalizeImage(bestCrop.image || bestCrop) : '';
const location = getLocation(primaryEvent.location ? primaryEvent : bestCrop || {});

const missing = Array.isArray(primaryEvent.missing_ppe)
  ? primaryEvent.missing_ppe.map(entry => cleanString(entry)).filter(Boolean)
  : [];

// The event item carries the verdict; fall back to the attributes when a
// consumer forwards only the attribute items.
const hasHelmet = primaryEvent.has_helmet !== undefined
  ? Boolean(primaryEvent.has_helmet)
  : asBoolean(attributes.has_helmet);
const hasVest = primaryEvent.has_vest !== undefined
  ? Boolean(primaryEvent.has_vest)
  : asBoolean(attributes.has_vest);

const result = {
  device_name: 'CVEDIX PPE Camera',
  device_model: 'Core Runtime',
  device_vendor: 'CVEDIX',
  event_type: cleanString(primaryEvent.$id || bestCrop?.$id, 'unknown'),
  event_id: cleanString(primaryEvent.event_id || bestCrop?.ref_event_id),
  instance_id: cleanString(metadataSource.instance_id),
  tracking_id: cleanString(metadataSource.ref_tracking_id),
  event_timestamp_ms: asNumber(metadataSource.event_timestamp_ms),
  system_timestamp: asNumber(metadataSource.system_timestamp),
  event_time: cleanString(metadataSource.system_datetime),

  ppe_status: cleanString(primaryEvent.status, cleanString(attributes.ppe_status, 'unknown')),
  has_helmet: hasHelmet,
  has_vest: hasVest,
  missing_ppe: missing.join(','),
  missing_ppe_count: asNumber(primaryEvent.missing_ppe_count, missing.length),
  person_confidence: asNumber(primaryEvent.confidence, asNumber(attributes.person_confidence)),
  track_id: asNumber(attributes.track_id),

  crop_count: crops.length,
  attribute_count: Object.keys(attributes).length,
  has_crop: Boolean(cropImage),
  bbox_x: location.x,
  bbox_y: location.y,
  bbox_width: location.width,
  bbox_height: location.height
};

if (violations.length > 0) {
  result.ppe_violation_seen = true;
  result.ppe_violation_seen_numeric = 1;
  result.ppe_violation_count = violations.length;
}

if (bestCrop) {
  result.crop_confidence = asNumber(bestCrop.confidence);
  result.crop_timestamp_ms = asNumber(bestCrop.crop_timestamp_ms);
  result.crop_event_id = cleanString(bestCrop.ref_event_id);
}

if (cropImage) {
  result.crop_image = cropImage;
}

return result;
