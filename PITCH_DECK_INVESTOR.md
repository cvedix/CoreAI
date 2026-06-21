# CVEDIX AI Runtime SDK
## Investor Pitch Deck

---

## Slide 1 — Cover

# CVEDIX
### AI Edge Infrastructure for Vietnam's Smart Transformation

**Investment Opportunity**
AI SDK platform enabling enterprises to deploy computer vision at the edge — 10x faster, 60% cheaper.

> **Vấn đề thị trường:** Việt Nam đang trải qua cuộc cách mạng smart city, smart factory, nhưng infrastructure cho AI vision còn phân mảnh và phụ thuộc nước ngoài.
>
> **Giải pháp:** CVEDIX xây dựng nền tảng AI runtime — "Windows của AI Edge" tại Việt Nam.

---

## Slide 2 — Market Opportunity

### Tại sao NOW? Tại sao Việt Nam?

| Yếu tố | Cơ hội |
|--------|--------|
| **Smart City** | 70+ smart city projects tại VN, thị trường $2.5B đến 2030 |
| **Manufacturing 4.0** | 85% factory đang plan AI vision deployment |
| **AI toàn cầu** | Global AI vision market $62B (2025) → $150B (2030), CAGR 19% |
| **Edge AI** | 75% enterprise data sẽ được process at edge đến 2030 (Gartner) |
| **Local preference** | Government & banking ưu tiên software nội địa, không phụ thuộc nước ngoài |

### Total Addressable Market (TAM)
```
Global AI Edge SDK Market:        $12B (2025)
→ Serviceable Available Market:   $2.5B (Asia-Pacific + Vietnam)
→ Serviceable Obtainable Market:  $150M (5-year target)
```

---

## Slide 3 — Problem

### The AI Vision Deployment Nightmare

| Stakeholder | Vấn đề | Chi phí ẩn |
|-------------|--------|------------|
| **Enterprise** | Build AI pipeline từ scratch mất 6-12 tháng | $500K-$2M development cost |
| **System Integrator** | Mỗi project phải rebuild lại từ đầu | Không scale được, margin thấp |
| **End Customer** | Vendor lock-in, không switch backend được | Dependency vào 1 hardware vendor |

```
Timeline truyền thống:
[Requirements] → [Hardware Procurement] → [Model Optimization] → [Pipeline Build] → [Testing] → [Deploy]
      ⏱️ 6-12 tháng                    ⏱️ 2-4 tháng              ⏱️ 3-6 tháng
```

### Pain points sâu:
1. **Fragmentation**: Mỗi hardware (NVIDIA, Intel, Rockchip) cần code khác nhau
2. **No reuse**: Mỗi project start from zero, không có platform
3. **Talent shortage**: Thiếu engineer biết cả AI + edge deployment
4. **Time-to-market**: 6-12 tháng là quá chậm cho thị trường VN

---

## Slide 4 — Solution

### CVEDIX AI Runtime SDK — "Windows của AI Edge"

**Một SDK để rule tất cả AI vision deployment tại edge.**

```
Trước CVEDIX:                     Sau CVEDIX:
Hardware A → Code A → Test A      Một SDK → Deploy trên mọi hardware
Hardware B → Code B → Test B                 → Giảm 80% dev time
Hardware C → Code C → Test C                 → Giảm 60% infrastructure cost
```

### Key Value Propositions:
- **10x faster deployment**: Từ 6 tháng → 2 tuần
- **60% cheaper**: Không cần cloud, chạy on-premise
- **Zero vendor lock-in**: Switch backend (TensorRT ↔ OpenVINO) không cần change app code
- **Hot-plug analytics**: Thêm tính năng mới mà không restart system

---

## Slide 5 — Product Demo

### Architecture: Plugin-Based, Infinite Scale

```
┌─────────────────────────────────────────────────────────┐
│              Application Layer                           │
│    Smart City · Smart Factory · Smart Retail            │
├─────────────────────────────────────────────────────────┤
│              SDK Core Engine                             │
│    Dynamic Pipeline · Node Management · Data Flow       │
├──────┬──────┬──────┬──────┬──────┬──────┬──────┬───────┤
│Source│Detect│Track│Analyze│Recogn│OSD   │Broker│Output │
├──────┴──────┴──────┴──────┴──────┴──────┴──────┴───────┤
│    Inference: ONNX · TensorRT · OpenVINO · RKNN       │
└─────────────────────────────────────────────────────────┘
```

### Product Metrics:
| Metric | Value |
|--------|-------|
| Nodes available | 80+ |
| Inference backends | 7 |
| Sample programs | 60+ |
| Max channels tested | 32 |
| Latency | < 50ms per frame |
| Platforms | x86, ARM, NPU |

---

## Slide 6 — Business Model

### Multi-Tier SaaS + Perpetual License

```
                    Starter      Professional      Enterprise
                    ───────      ───────────       ──────────
Revenue Streams:
├─ SDK License       $5K/yr       $15K/yr           $50K/yr
├─ Custom Nodes      $10K/proj    $25K/proj         $50K/proj
├─ Support           15%/yr       15%/yr            20%/yr + SLA
├─ Training          $5K          $10K              $20K
└─ White-label       —            —                 Custom
```

### Revenue Projection (Year 1-5):
```
Year 1 (2025):    $200K  — 10 customers, proof of product-market fit
Year 2 (2026):    $800K  — 40 customers, expand to SEA
Year 3 (2027):    $2.5M  — 150 customers, enterprise tier dominant
Year 4 (2028):    $5M    — Regional expansion, platform play
Year 5 (2029):    $10M   — Market leader, acquisition target
```

### LTV/CAC Ratio dự kiến: 8-10x (typical for enterprise SaaS)

---

## Slide 7 — Traction

### Hiện tại (Pre-Revenue)
| Milestone | Status |
|-----------|--------|
| **SDK Core** | ✅ Production-ready (v2025.0.1.3-dev1) |
| **Nodes** | ✅ 80+ nodes implemented |
| **Backends** | ✅ 7 inference backends |
| **Samples** | ✅ 60+ working examples |
| **Documentation** | ✅ Full docs, API reference |
| **Packaging** | ✅ .deb, Docker, systemd |
| **Technical Risk** | ✅ **MITIGATED** |

### Early Signals:
- Internal product với 60+ samples chứng tỏ tính khả thi
- Architecture flexible, dễ dàng add new nodes
- Already supports 7 hardware platforms
- Active development (monthly updates)

### Cần traction trong 12 tháng tới:
- [ ] 10 paying customers (pilots)
- [ ] 2 case studies với measurable ROI
- [ ] 1 white-label partner
- [ ] $200K ARR

---

## Slide 8 — Competitive Landscape

```
                    Easy to Use      Enterprise-Grade
                         │                    │
                         │                    │
    ┌────────────────────┼────────────────────┼────────────────────┐
    │                    │                    │                    │
    │  Open Source       │   ┌──────────┐    │                    │
    │  (YOLO-WebUI,      │   │ CVEDIX   │    │                    │
    │   Darknet)         │   │  AI SDK  │◄───┤  OUR TARGET        │
    │                    │   └──────────┘    │                    │
    ├────────────────────┼────────────────────┼────────────────────┤
    │                    │                    │                    │
    │                    │   ┌──────────┐    │                    │
    │                    │   │  Custom  │    │                    │
    │                    │   │  Builds  │    │                    │
    │                    │   └──────────┘    │                    │
    ├────────────────────┼────────────────────┼────────────────────┤
    │                    │                    │                    │
    │  ┌──────────┐     │   ┌──────────┐    │  ┌──────────┐       │
    │  │  Cheap   │     │   │Commercial│    │  │  Premium │       │
    │  │  VMS     │     │   │  VMS     │    │  │  VMS     │       │
    │  └──────────┘     │   │(Movidius,│    │  │(Ambarella│       │
    │                    │   │  Aptiv)  │    │  │  , FDTI) │       │
    │                    │   └──────────┘    │  └──────────┘       │
    └────────────────────┴────────────────────┴────────────────────┘
```

### Why CVEDIX wins:
| Dimension | Open Source | Custom | Commercial VMS | **CVEDIX** |
|-----------|-------------|--------|----------------|------------|
| Time-to-market | Slow | Very Slow | Medium | **Fast** |
| Cost | Low | High | Very High | **Medium** |
| Flexibility | High | High | Low | **High** |
| Support | None | Self | Vendor | **Local** |
| Local adaptation | No | Yes | No | **Yes** |

---

## Slide 9 — Go-to-Market Strategy

### Phase 1: Land (Months 1-6)
**Target:** System Integrators tại Việt Nam
- Free community edition (limited to 4 channels)
- Partner with 3-5 SIs để build pilot projects
- Conference: VietAI Summit, VnExpress Tech

### Phase 2: Expand (Months 7-18)
**Target:** Enterprise customers (manufacturing, retail)
- Paid licenses cho production deployment
- Case studies từ pilot projects
- Channel program: reseller + technology partner

### Phase 3: Scale (Months 19-36)
**Target:** Regional (SEA + Southeast Asia)
- White-label cho local distributors
- Cloud-edge platform (SaaS model)
- AI model marketplace

### Customer Acquisition:
```
Month 1-6:   3 pilots → 2 paying customers
Month 7-12:  10 customers, $200K ARR
Month 13-24: 40 customers, $800K ARR
Month 25-36: 150 customers, $2.5M ARR
```

---

## Slide 10 — Team

### Core Team (待 thêm information)

| Role |待 thêm | Responsibilities |
|------|--------|------------------|
| **CEO / Tech Lead** |待 thêm | Full-stack AI/ML, SDK architecture |
| **CTO** |待 thêm | Engineering, product development |
| **Sales** |待 thêm | Enterprise sales, partner management |

### Advisory Board (待 thêm)
-待 thêm industry advisors (AI, manufacturing, smart city)

### Why this team?
- Deep technical expertise trong AI, computer vision, edge computing
- Đã build production systems trước đó
- Understanding of Vietnam market dynamics

---

## Slide 11 — Financial Projection

### 5-Year Forecast (USD)

| Metric | Year 1 | Year 2 | Year 3 | Year 4 | Year 5 |
|--------|--------|--------|--------|--------|--------|
| **Customers** | 10 | 40 | 150 | 300 | 500 |
| **Revenue** | $200K | $800K | $2.5M | $5M | $10M |
| **Gross Margin** | 85% | 88% | 90% | 92% | 93% |
| **COGS** | $30K | $100K | $250K | $400K | $700K |
| **Gross Profit** | $170K | $700K | $2.25M | $4.6M | $9.3M |
| **OpEx** | $500K | $800K | $1.5M | $2.5M | $4M |
| - Sales & Marketing | $200K | $350K | $600K | $900K | $1.4M |
| - R&D | $200K | $300K | $500K | $900K | $1.5M |
| - G&A | $100K | $150K | $400K | $700K | $1.1M |
| **EBITDA** | **-$330K** | **-$100K** | **$750K** | **$2.1M** | **$5.3M** |
| **Net Income** | **-$330K** | **-$100K** | **$560K** | **$1.6M** | **$4M** |

### Key Assumptions:
- Average contract value: $20K (Year 1) → $25K (Year 5)
- Churn rate: < 5% (enterprise product = sticky)
- CAC payback: < 12 months
- R&D: 2-3 engineers trong Year 1-2, expand đến 8-10 by Year 3

---

## Slide 12 — The Ask

### Raising: **$500K Pre-Seed**

### Use of Funds:
```
                    ┌──────────────────────────────┐
                    │          $500K               │
                    ├──────────────────────────────┤
                    │  Engineering (60%)   $300K   │
                    │  ├─ 2 senior engineers   $180K/yr │
                    │  ├─ 1 frontend dev       $60K/yr  │
                    │  └─ QA / DevOps          $60K/yr  │
                    │                              │
                    │  Sales & Marketing (25%) $125K │
                    │  ├─ Sales lead             $80K/yr  │
                    │  └─ Marketing / events     $45K/yr  │
                    │                              │
                    │  Operations (10%)    $50K    │
                    │  ├─ Legal / IP             $20K     │
                    │  └─ Office / Admin         $30K     │
                    │                              │
                    │  Reserve (5%)      $25K    │
                    └──────────────────────────────┘
```

### Milestones with $500K:
- **Month 6:** 3 paying customers, $50K ARR
- **Month 12:** 10 paying customers, $200K ARR
- **Month 18:** Product-market fit confirmed, prepare Seed round

### Runway: 18 months

---

## Slide 13 — Exit Strategy

### Potential Exit Paths

| Scenario | Timeline | Valuation | Return Multiple |
|----------|----------|-----------|-----------------|
| **Acquisition by VMS player** | 5-7 years | $50-100M | 10-20x |
| **Acquisition by hardware vendor** | 5-7 years | $30-60M | 6-12x |
| **Acquisition by cloud provider** | 7-10 years | $100-200M | 20-40x |
| **IPO (unlikely but possible)** | 10+ years | $200M+ | 40x+ |

### Strategic Acquirers:
- **NVIDIA** — Want edge AI platform
- **Intel (Movidius)** — OpenVINO ecosystem play
- **Ambarella** — Silicon vendor, want software moat
- **FDTI** — Chinese AI hardware, want SEA expansion
- **VMS vendors** (Milestone, Genetec) — Want modern AI platform

### Base case: $50M acquisition at 10x revenue = **$500K → $5-10M return (10-20x)**

---

## Slide 14 — Risks & Mitigation

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **Competition from NVIDIA/Intel** | Medium | High | Focus on VN market, local support, multi-backend abstraction |
| **Open source alternatives improve** | Medium | Medium | Faster iteration, better DX, enterprise features |
| **Customer acquisition slow** | High | Medium | Free tier, partner channel, SI ecosystem |
| **Key person dependency** | Medium | High | Document everything, hire early, equity alignment |
| **Revenue slower than projected** | High | Medium | Pivot to services revenue if needed, reduce burn |
| **Technology obsolescence** | Low | High | Multi-backend strategy, adapt to new tech |

### Biggest Risk: **No market need**
- Mitigation: Validate với 10+ potential customers trong 3 tháng đầu
- Pivot plan: If B2B slow, switch to B2C / developer tool

---

## Slide 15 — Vision

### 5 năm tới, CVEDIX sẽ là:

```
2025:     AI SDK cho edge vision (Vietnam)
2027:     AI platform cho SEA market
2029:     AI infrastructure layer — "The Windows of Edge AI"
```

### Vision Statement:
> **"Mọi thiết bị edge tại Việt Nam chạy AI vision đều sẽ dùng CVEDIX SDK."**

### The bigger picture:
1. **Start**: AI vision SDK cho edge deployment
2. **Expand**: AI runtime platform cho mọi workloads (NLP, audio, time-series)
3. **Scale**: AI marketplace — developers publish models, customers deploy

---

## Slide 16 — Contact

# Thank You

### CVEDIX — Build Once. Deploy Everywhere.

**Next Steps:**
1. Technical deep-dive session
2. Product demo & POC
3. Term sheet discussion

### Liên hệ:
- **Email**: [待 thêm]
- **Phone**: [待 thêm]
- **Website**: [待 thêm]
- **Address**: [待 thêm]

---

> **DISCLAIMER**: Tài liệu này chứa thông tin nội bộ và forward-looking statements. Các con số financial là projection, không phải đảm bảo kết quả thực tế. Chỉ dùng cho mục đích đầu tư discussion nội bộ.
>
> *© 2025 CVEDIX. All rights reserved.*