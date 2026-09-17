#include "test_assert.h"
#include "../third_party/onnx_yolov11/onnx_yolov11_output.h"

CVEDIX_TEST_CASE(custom_ppe_head_preserves_boxes_and_two_class_scores) {
    int shape[] = {1, 6, 8400};
    cv::Mat head(3, shape, CV_32F, cv::Scalar(0));
    for (int c = 0; c < 6; ++c) head.ptr<float>(0, c)[42] = c + 0.25f;
    const auto rows = onnx_yolov11::normalize_output(head);
    CVEDIX_ASSERT_EQ(rows.rows, 8400);
    CVEDIX_ASSERT_EQ(rows.cols, 6);
    for (int c = 0; c < 6; ++c) CVEDIX_ASSERT_EQ(rows.at<float>(42, c), c + 0.25f);
}

CVEDIX_TEST_CASE(coco_and_transposed_heads_remain_supported) {
    for (int nc : {1, 2, 80}) {
        int shape[] = {1, 8400, 4 + nc};
        cv::Mat head(3, shape, CV_32F, cv::Scalar(0));
        head.ptr<float>(0, 123)[3 + nc] = 0.9f;
        const auto rows = onnx_yolov11::normalize_output(head);
        CVEDIX_ASSERT_EQ(rows.rows, 8400);
        CVEDIX_ASSERT_EQ(rows.cols, 4 + nc);
        CVEDIX_ASSERT_EQ(rows.at<float>(123, 3 + nc), 0.9f);
        cv::Mat transposed = rows.t();
        const auto restored = onnx_yolov11::normalize_output(transposed);
        CVEDIX_ASSERT_EQ(cv::norm(rows, restored, cv::NORM_INF), 0.0);
    }
}

CVEDIX_TEST_CASE(malformed_heads_fail_explicitly) {
    int batch_shape[] = {2, 6, 8400};
    for (const auto& head : {cv::Mat(), cv::Mat(1, 50400, CV_32F),
                            cv::Mat(6, 8400, CV_8U), cv::Mat(3, batch_shape, CV_32F)}) {
        bool rejected = false;
        try { onnx_yolov11::normalize_output(head); }
        catch (const std::runtime_error&) { rejected = true; }
        CVEDIX_ASSERT_TRUE(rejected);
    }
}

int main(int argc, char** argv) { return cvedix_test::run_all(argc, argv); }
