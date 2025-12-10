# Margin-based Confidence Filtering for Face Recognition

## Vấn đề

### Threshold đơn thuần không đủ

```python
# Current approach: CHỈ dùng threshold
if similarity_score > 0.7:
    return matched_name  # ✅ ACCEPT
else:
    return "Unknown"     # ❌ REJECT
```

**Vấn đề:**
```
Ví dụ 1: Ambiguous case (rất dễ nhầm)
- Person A: score = 0.71
- Person B: score = 0.70
- Difference = 0.01
→ Kết quả: ACCEPT Person A ✅
→ Vấn đề: Chênh nhau quá ít! Rất dễ nhầm!

Ví dụ 2: Confident case
- Person A: score = 0.85
- Person B: score = 0.45
- Difference = 0.40
→ Kết quả: ACCEPT Person A ✅
→ OK: Chênh nhau rõ ràng, confident!
```

## Giải pháp: Margin-based Confidence

### Concept

```python
# New approach: Threshold + Margin
if (top1_score > threshold) AND (top1_score - top2_score > margin):
    return top1_name  # ✅ ACCEPT (confident)
else:
    return "Unknown"   # ❌ REJECT (ambiguous)
```

### Ví dụ với margin = 0.3

```
Test Case 1: REJECT (margin không đủ)
- Top-1: Person A, score = 0.90
- Top-2: Person B, score = 0.70
- Difference = 0.20 < 0.3
→ Kết quả: REJECT (Unknown)
→ Lý do: Không confident, có thể là Person B

Test Case 2: ACCEPT (margin đủ)
- Top-1: Person A, score = 0.90
- Top-2: Person B, score = 0.50
- Difference = 0.40 > 0.3
→ Kết quả: ACCEPT Person A
→ Lý do: Confident, chênh lệch rõ ràng

Test Case 3: REJECT (threshold không đủ)
- Top-1: Person A, score = 0.60
- Top-2: Person B, score = 0.30
- Difference = 0.30 >= 0.3
→ Kết quả: REJECT (Unknown)
→ Lý do: Dù margin OK nhưng score quá thấp
```

## Implementation

### Basic Implementation

```cpp
#include "face_database_with_margin.h"

using namespace cvedix_face_utils;

// Create enhanced database
EnhancedFaceDatabase face_db;
face_db.similarity_threshold = 0.7f;   // Min score
face_db.confidence_margin = 0.3f;      // Min margin
face_db.use_margin_check = true;       // Enable
face_db.strict_mode = true;            // Require BOTH

// Load database
face_db.load("face_database.txt");

// Query with margin checking
MatchResult result = face_db.find_match(query_embedding);

// Check result
if (result.confident) {
    std::cout << "Matched: " << result.name 
              << " (score=" << result.score 
              << ", margin=" << result.margin << ")" << std::endl;
} else {
    std::cout << "Rejected: ambiguous match" << std::endl;
    std::cout << "  Top-1: " << result.name << " (" << result.score << ")" << std::endl;
    std::cout << "  Top-2: " << result.second_name << " (" << result.second_score << ")" << std::endl;
    std::cout << "  Margin: " << result.margin << " < " << face_db.confidence_margin << std::endl;
}
```

### Integration với Pipeline

```cpp
// Add hook to facenet node
facenet->add_hook([&face_db](std::shared_ptr<cvedix_frame_meta> meta) {
    for (auto& face : meta->face_targets) {
        if (!face->embeddings.empty()) {
            // Find match with margin checking
            MatchResult result = face_db.find_match(face->embeddings);
            
            // Update metadata
            face->primary_class_name = result.name;
            face->confidence = result.score;
            
            // Store additional info
            face->custom_data["margin"] = std::to_string(result.margin);
            face->custom_data["confident"] = result.confident ? "yes" : "no";
            
            // Log if rejected
            if (!result.confident) {
                CVEDIX_WARN("Ambiguous match rejected!");
            }
        }
    }
});
```

## Configuration Modes

### 1. Strict Mode (Recommended)

```cpp
face_db.strict_mode = true;

// Decision logic:
if (top1_score >= threshold AND margin >= min_margin):
    ACCEPT
else:
    REJECT
```

**Pros:**
- ✅ Rất ít false positive (nhầm người)
- ✅ High security
- ✅ Tốt cho ảnh kém chất lượng

**Cons:**
- ⚠️ Có thể tăng false negative (reject đúng người)

### 2. Relaxed Mode

```cpp
face_db.strict_mode = false;

// Decision logic:
if (top1_score >= threshold AND margin >= min_margin):
    ACCEPT  # Normal case
elif (margin >= min_margin AND top1_score >= threshold - 0.1):
    ACCEPT  # High margin compensates for lower score
else:
    REJECT
```

**Pros:**
- ✅ Ít false negative hơn
- ✅ Margin cao có thể compensate score thấp

**Cons:**
- ⚠️ Có thể tăng false positive

## Parameter Tuning

### Similarity Threshold

```cpp
// Conservative (high security)
face_db.similarity_threshold = 0.75f;

// Balanced (recommended)
face_db.similarity_threshold = 0.70f;

// Aggressive (more matches)
face_db.similarity_threshold = 0.65f;
```

### Confidence Margin

```cpp
// High security (very strict)
face_db.confidence_margin = 0.40f;  // Top-1 phải hơn top-2 ít nhất 40%

// Balanced (recommended)
face_db.confidence_margin = 0.30f;  // 30% margin

// Moderate
face_db.confidence_margin = 0.20f;  // 20% margin

// Aggressive (like no margin check)
face_db.confidence_margin = 0.10f;  // 10% margin
```

### Recommended Settings

```cpp
// For general use (balanced)
face_db.similarity_threshold = 0.70f;
face_db.confidence_margin = 0.30f;
face_db.strict_mode = true;

// For high security (prefer rejection)
face_db.similarity_threshold = 0.75f;
face_db.confidence_margin = 0.40f;
face_db.strict_mode = true;

// For user convenience (prefer acceptance)
face_db.similarity_threshold = 0.65f;
face_db.confidence_margin = 0.20f;
face_db.strict_mode = false;

// For poor lighting conditions (very strict)
face_db.similarity_threshold = 0.75f;
face_db.confidence_margin = 0.35f;
face_db.strict_mode = true;
```

## Cải thiện cho Ảnh thiếu sáng

### 1. Quality-aware Thresholds

```cpp
// Detect image quality
float brightness = compute_brightness(face_image);
float sharpness = compute_sharpness(face_image);

// Adaptive thresholds
if (brightness < 50 || sharpness < 0.5) {
    // Poor quality → stricter requirements
    face_db.similarity_threshold = 0.75f;
    face_db.confidence_margin = 0.40f;
} else {
    // Good quality → normal requirements
    face_db.similarity_threshold = 0.70f;
    face_db.confidence_margin = 0.30f;
}
```

### 2. Histogram Equalization

```cpp
// Improve poor lighting
cv::Mat enhanced;
cv::cvtColor(face_image, enhanced, cv::COLOR_BGR2GRAY);
cv::equalizeHist(enhanced, enhanced);
cv::cvtColor(enhanced, enhanced, cv::COLOR_GRAY2BGR);
```

### 3. Multiple Embeddings per Person

```cpp
// Register multiple photos with different lighting
face_db.add_face("John", embedding_bright);
face_db.add_face("John", embedding_normal);
face_db.add_face("John", embedding_dark);

// Matching uses best match among all embeddings
MatchResult result = face_db.find_match(query);
```

### 4. Quality-based Rejection

```cpp
// Reject if image quality too poor
float quality_score = estimate_quality(face_image);
if (quality_score < 0.3) {
    return MatchResult{
        .name = "Unknown",
        .score = 0.0f,
        .confident = false
    };
}
```

## Evaluation Metrics

### Confusion Matrix

```
                Predicted
                Positive    Negative
Actual Positive    TP          FN
       Negative    FP          TN

TP = True Positive  (correct match)
FP = False Positive (wrong person)
FN = False Negative (reject correct person)
TN = True Negative  (correct rejection)
```

### Metrics

```cpp
Precision = TP / (TP + FP)      # Accuracy of positive predictions
Recall = TP / (TP + FN)         # Coverage of actual positives
F1-Score = 2 * (P * R) / (P + R)

// Margin check typically:
// - Reduces FP (fewer wrong matches) ✅
// - May increase FN (more rejections) ⚠️
// - Improves Precision
// - May reduce Recall
```

## Testing

### Test với các scenarios

```cpp
// Scenario 1: Similar-looking people
test_similar_faces("person_a.jpg", "person_b.jpg");
// Expected: Margin check helps distinguish

// Scenario 2: Poor lighting
test_poor_lighting("dark_image.jpg");
// Expected: Higher rejection rate (better than wrong match)

// Scenario 3: Partial occlusion
test_occlusion("masked_face.jpg");
// Expected: Reject ambiguous cases

// Scenario 4: Different angles
test_angles("profile_view.jpg", "frontal_view.jpg");
// Expected: Margin helps with pose variation
```

### Benchmark Script

```python
# evaluate_margin.py
import numpy as np

def evaluate_margin(embeddings, labels, thresholds, margins):
    results = []
    for threshold in thresholds:
        for margin in margins:
            tp, fp, tn, fn = compute_metrics(
                embeddings, labels, threshold, margin)
            precision = tp / (tp + fp)
            recall = tp / (tp + fn)
            f1 = 2 * precision * recall / (precision + recall)
            results.append({
                'threshold': threshold,
                'margin': margin,
                'precision': precision,
                'recall': recall,
                'f1': f1
            })
    return results

# Find optimal parameters
best = max(results, key=lambda x: x['f1'])
print(f"Optimal: threshold={best['threshold']}, margin={best['margin']}")
```

## Database Format

### Pipe-separated (Recommended)

```
name|emb[0],emb[1],...,emb[511]
```

Example:
```
PHONG|0.0408,0.0899,-0.0263,...,-0.0087
SANG|0.0156,-0.0234,0.0789,...,0.0123
```

### Space-separated

```
name emb[0] emb[1] ... emb[511]
```

Example:
```
PHONG 0.0408 0.0899 -0.0263 ... -0.0087
SANG 0.0156 -0.0234 0.0789 ... 0.0123
```

## Complete Example

See `samples/facenet_margin_sample.cpp` for full working example.

## References

1. **Face Recognition with Margin-based Softmax:**
   - Liu et al., "SphereFace: Deep Hypersphere Embedding for Face Recognition", CVPR 2017

2. **ArcFace (Additive Angular Margin):**
   - Deng et al., "ArcFace: Additive Angular Margin Loss for Deep Face Recognition", CVPR 2019

3. **Practical Experience:**
   - Renesas AI Accelerator project (facenet without GPU)
   - Focus on avoiding false positives in poor lighting

## Summary

**Benefits của Margin-based Confidence:**

✅ **Giảm False Positive** (nhầm người) - quan trọng nhất!  
✅ **Tốt hơn cho ảnh kém chất lượng** (thiếu sáng, blur)  
✅ **Robust với similar-looking people**  
✅ **High security applications**  

**Trade-offs:**

⚠️ **Có thể tăng False Negative** (reject đúng người)  
⚠️ **Cần tune parameters** cho từng use case  

**Recommendation:**

```cpp
// Start with these values và adjust based on your data
face_db.similarity_threshold = 0.70f;
face_db.confidence_margin = 0.30f;
face_db.strict_mode = true;
```




