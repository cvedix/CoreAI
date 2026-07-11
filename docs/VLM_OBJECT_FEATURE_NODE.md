# VLM Object Feature Node

## Muc tieu

`cvedix_vlm_feature_node` trich xuat dac trung semantic cua doi tuong trong anh crop tu detector/tracker. Node duoc thiet ke de uu tien mo rong da model VLM (Ollama/OpenAI-compatible), bao gom nhung khong gioi han:

- Qwen3-VL
- LLaVA family
- Gemma vision models
- Cac model noi bo thong qua endpoint tuong thich

Node ghi ket qua ve chinh `targets` trong `frame_meta`:

- `targets[i]->embeddings`: vector dac trung 128 chieu (L2-normalized)
- `targets[i]->secondary_labels`: nhan dang `vlm:<category>:<attributes>`
- `targets[i]->secondary_scores`: do tin cay semantic (`0.8` cho LLM, `0.2` cho fallback)

## Pipeline khuyen nghi

```text
src -> detector -> vlm_feature -> (milvus_vector_search | broker | osd)
```

Node nay chay sau detector de co bounding box va crop theo tung target.

## Constructor

```cpp
auto vlm_feature = std::make_shared<cvedix_nodes::cvedix_vlm_feature_node>(
    "vlm_feature_0",
    "qwen3-vl:latest",          // model_name
    "http://127.0.0.1:11434",   // api_base_url (Ollama)
    "",                         // api_key
    llmlib::LLMBackendType::Ollama,
    8,                            // run_every_n_frames
    4,                            // max_targets_per_frame
    0.03f,                        // min_box_width_ratio
    0.03f,                        // min_box_height_ratio
    10                            // request_timeout_sec
);
```

## Fallback

Neu model timeout hoac phan hoi khong hop le, node tu fallback bang dac trung histogram + gradient, dam bao pipeline khong bi gian doan.

Khi fallback, secondary label co hau to `:fallback`.

## Build

Yeu cau `CVEDIX_WITH_LLM=ON`.

Vi du:

```bash
cmake -S . -B build -DCVEDIX_WITH_CUDA=ON -DCVEDIX_WITH_LLM=ON
cmake --build build -j
```

## Sample

Sample su dung ten generic:

- executable: `vlm_object_feature_sample`
- source: `samples/vlm_object_feature_sample.cpp`

Sample hau xu ly event da co san (sau khi webhook da luu snapshot):

- executable: `event_snapshot_vlm_enrichment_sample`
- source: `samples/event_snapshot_vlm_enrichment_sample.cpp`

Run:

```bash
./build/bin/vlm_object_feature_sample \
  --video /path/to/video.mp4 \
  --model qwen3-vl:latest \
  --api http://127.0.0.1:11434

./build/bin/event_snapshot_vlm_enrichment_sample \
  --input /path/to/event_snapshots \
  --model qwen3-vl:latest \
  --api http://127.0.0.1:11434 \
  --output /tmp/event_vlm.jsonl
```

## Luu y hieu nang

- Gioi han `max_targets_per_frame` de tranh tang latency dot bien.
- Tang `run_every_n_frames` neu nguon video FPS cao.
- Dung node nay nhu semantic feature extractor, khong thay detector.
