# RapidMedia VLM Feature Node

## Muc tieu

`cvedix_rapidmedia_vlm_feature_node` la node mo rong tu `cvedix_vlm_feature_node` de toi uu cho bai toan event-driven cua RapidMedia:

- Giu nguyen duong xu ly VLM/embedding da co (tu node cha).
- Them lop pre-filter de loai bo target it gia tri truoc khi goi VLM.
- Sap xep uu tien target theo nhan, kich thuoc bbox, track_id va confidence.

Node nay phu hop khi can gioi han VLM budget trong moi frame nhung van dam bao target quan trong duoc xu ly truoc.

## Ke thua va tuong thich

- Base class: `cvedix_vlm_feature_node`
- Tuong thich output: ghi vao `targets[i]->embeddings` va `targets[i]->secondary_labels`
- Co the thay the truc tiep node cu trong pipeline ma khong doi schema output

## Constructor mac dinh cho RapidMedia

- model: `qwen3-vl:2b`
- api_base_url: `http://127.0.0.1:8080` (phu hop llama.cpp server OpenAI-compatible)
- backend: `OpenAI`
- run_every_n_frames: `4`
- max_targets_per_frame: `4`

## Bien moi truong toi uu

- `ASS_RAPIDMEDIA_VLM_REQUIRE_TRACK` (default: true)
  - Bat: chi xu ly target da co `track_id`
- `ASS_RAPIDMEDIA_VLM_MIN_AREA_RATIO` (default: 0.005)
  - Nguong dien tich toi thieu theo ty le bbox/frame
- `ASS_RAPIDMEDIA_VLM_PREFILTER_MAX` (default: 12)
  - So target toi da sau pre-filter truoc khi giao cho node cha
- `ASS_RAPIDMEDIA_VLM_ALLOW_LABELS`
  - Danh sach label cho phep, cach nhau boi `,` `;` `|`
- `ASS_RAPIDMEDIA_VLM_DENY_LABELS`
  - Danh sach label loai tru
- `ASS_RAPIDMEDIA_VLM_PRIORITY_LABELS`
  - Danh sach label uu tien trong buoc sap xep

## Metrics giam sat node

Node da duoc bo sung metrics runtime de giam sat tai he thong RapidMedia qua log collector hien co.

- `ASS_RAPIDMEDIA_VLM_METRICS_ENABLED` (default: true)
  - Bat/tat xuat metrics runtime
- `ASS_RAPIDMEDIA_VLM_METRICS_INTERVAL_SEC` (default: 30)
  - Chu ky xuat log metrics (giay)
- `ASS_RAPIDMEDIA_VLM_METRICS_PREFIX` (default: `rapidmedia_vlm`)
  - Prefix trong dong log de de loc/scrape

Log metrics duoc xuat dang key-value, vi du:

```text
[rapidmedia_vlm] frames_total=120 frames_skipped_empty=12 frames_skipped_invalid=0 targets_input_total=356 targets_kept_total=181 targets_selected_total=120 keep_ratio=0.5084 select_ratio=0.3371 base_node_calls=108 base_node_exceptions=0 prefilter_time_us_total=96432 base_time_us_total=1550420 prefilter_avg_us=803.6 base_avg_us=14355.7
```

Y nghia nhanh:

- `frames_total`, `frames_skipped_*`: theo doi luong frame vao va frame bo qua.
- `targets_input_total`, `targets_kept_total`, `targets_selected_total`: theo doi hieu qua pre-filter.
- `keep_ratio`, `select_ratio`: ty le giu/sap xu ly de can bang chat luong va chi phi VLM.
- `base_node_calls`, `base_node_exceptions`: suc khoe duong xu ly node cha.
- `prefilter_avg_us`, `base_avg_us`: do tre pre-filter va goi node VLM cha.

## Vi du su dung

```cpp
#include "cvedix/nodes/infers/cvedix_rapidmedia_vlm_feature_node.h"

auto rapidmedia_vlm = std::make_shared<cvedix_nodes::cvedix_rapidmedia_vlm_feature_node>(
    "rapidmedia_vlm_feature_0",
    "qwen3-vl:2b",
    "http://127.0.0.1:8080",
    "",
    llmlib::LLMBackendType::OpenAI,
    4,
    4,
    0.02f,
    0.02f,
    12
);
```

Pipeline goi y:

```text
src -> detector -> sort_track -> rapidmedia_vlm_feature -> broker/osd
```

## Ghi chu

- Build yeu cau `CVEDIX_WITH_LLM=ON`.
- Neu pipeline can fallback descriptor trong tinh huong backend loi, tiep tuc su dung co che fallback cua node cha.
