# Tài liệu nội bộ — Core Runtime SDK

> Điểm vào duy nhất cho toàn bộ tài liệu kỹ thuật của dự án. Bắt đầu từ đây.

## Bắt đầu

| Tài liệu | Dành cho | Nội dung |
|----------|----------|----------|
| [ONBOARDING.md](ONBOARDING.md) | Dev mới | Setup → build → chạy sample đầu tiên trong 30 phút |
| [DEVELOPMENT.md](DEVELOPMENT.md) | Mọi dev | Môi trường, build targets, CMake options, troubleshooting |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Mọi dev | Kiến trúc pipeline, taxonomy node (SRC/MID/DES), data flow |
| [NODES_AND_SAMPLES.md](NODES_AND_SAMPLES.md) | Mọi dev | API reference 80+ nodes + danh sách samples |

## Hướng dẫn tính năng — [guides/](guides/)

| Tài liệu | Nội dung |
|----------|----------|
| [BA_CROSSLINE_USAGE.md](guides/BA_CROSSLINE_USAGE.md) | Cấu hình crossline counting (đếm vượt vạch) |
| [BA_NODE_EVENT_FORMAT.md](guides/BA_NODE_EVENT_FORMAT.md) | Định dạng event JSON/XML của behavior analysis |
| [BA_EVENT_EXTRACTION_INTEGRATION.md](guides/BA_EVENT_EXTRACTION_INTEGRATION.md) | Tích hợp BA event extraction |
| [VLM_OBJECT_FEATURE_NODE.md](guides/VLM_OBJECT_FEATURE_NODE.md) | Trích xuất đặc trưng object bằng VLM |
| [RAPIDMEDIA_VLM_FEATURE_NODE.md](guides/RAPIDMEDIA_VLM_FEATURE_NODE.md) | Tích hợp VLM với RapidMedia (customer-specific) |
| [FACE_RECOGNIZER_SEETAFACE6.md](guides/FACE_RECOGNIZER_SEETAFACE6.md) | Setup face recognizer SeetaFace6 |

## Hạ tầng & CI — [ci/](ci/)

| Tài liệu | Nội dung |
|----------|----------|
| [GPU_CI_SETUP_GUIDE.md](ci/GPU_CI_SETUP_GUIDE.md) | Hướng dẫn setup GPU CI runner (tham khảo — CI chưa triển khai) |

## SDK phân phối cho người dùng — [../sdk/](../sdk/)

Submodule trỏ tới [github.com/cvedix/SDK](https://github.com/cvedix/SDK) — gói headers + prebuilt libs + bindings giao cho khách hàng.

| Tài liệu | Nội dung |
|----------|----------|
| [../sdk/README.md](../sdk/README.md) | Tổng quan SDK phân phối |
| [../sdk/bindings/README.md](../sdk/bindings/README.md) | Kiến trúc bindings C / Java / C# trên C API |
| [../capi/include/cvedix_c_api.h](../capi/include/cvedix_c_api.h) | C ABI ổn định (nguồn trong repo này, build option `CVEDIX_BUILD_CAPI`) |

Quy trình đồng bộ: `make build` → `bash sdk/scripts/package_sdk.sh $PWD/sdk` → commit & push submodule `sdk/`.

## Báo cáo — [reports/](reports/)

| Tài liệu | Nội dung |
|----------|----------|
| [FACE_RECOGNITION_BENCHMARK_REPORT.txt](reports/FACE_RECOGNITION_BENCHMARK_REPORT.txt) | Benchmark face recognition |
| [../benchmarks/reports/llm_benchmark_report.csv](../benchmarks/reports/llm_benchmark_report.csv) | Dữ liệu benchmark LLM |

## Tài liệu lịch sử — [internal/](internal/)

Tài liệu tham khảo, không còn được cập nhật thường xuyên:

| Tài liệu | Nội dung |
|----------|----------|
| [UPDATE_PLAN.md](internal/UPDATE_PLAN.md) | Kế hoạch update cũ (lịch sử) |
| [RECOMMENDATIONS.md](internal/RECOMMENDATIONS.md) | Khuyến nghị cải thiện DX (roadmap tham khảo) |
| [cvedix_runtime_sdk_integration_plan.md](internal/cvedix_runtime_sdk_integration_plan.md) | Kế hoạch tích hợp runtime SDK (lịch sử) |

## Quy ước tài liệu

- Tài liệu tính năng mới đặt trong `guides/`, tên file dạng `TÊN_TÍNH_NĂNG.md`
- Báo cáo benchmark/đo đạc đặt trong `reports/`
- Kế hoạch/plan đã hoàn thành chuyển vào `internal/`
- Mỗi tài liệu mới phải được thêm vào mục lục này
- Link nội bộ dùng **đường dẫn tương đối** — không dùng đường dẫn tuyệt đối (`file:///...`)
