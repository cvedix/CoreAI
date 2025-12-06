#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

#include "../models/insight_face_recognition.h"
#include "../util/algorithm_util.h"

using namespace std;
using namespace trt_insightface;

/*
 * Standalone test for InsightFace recognition
 * 
 * Usage:
 *   ./face_recognition_test <engine_path> <image1> [image2] ...
 * 
 * Example:
 *   ./face_recognition_test ./arcface_r50_fp16.engine face1.jpg face2.jpg
 */

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <engine_path> <image1> [image2] ..." << endl;
        return -1;
    }

    string engine_path = argv[1];
    
    // Load images
    vector<cv::Mat> images;
    for (int i = 2; i < argc; i++) {
        cv::Mat img = cv::imread(argv[i]);
        if (img.empty()) {
            cerr << "Error: Cannot load image: " << argv[i] << endl;
            continue;
        }
        
        // Resize to 112x112 (assuming images are already aligned)
        if (img.rows != 112 || img.cols != 112) {
            cv::resize(img, img, cv::Size(112, 112), 0, 0, cv::INTER_LINEAR);
        }
        
        images.push_back(img);
    }

    if (images.empty()) {
        cerr << "Error: No valid images loaded!" << endl;
        return -1;
    }

    cout << "Loading TensorRT engine from: " << engine_path << endl;
    InsightFaceRecognition recognizer(engine_path);
    cout << "Engine loaded successfully!" << endl;
    cout << "Embedding size: " << recognizer.get_embedding_size() << endl;

    // Extract features
    cout << "\nExtracting features from " << images.size() << " image(s)..." << endl;
    vector<vector<float>> embeddings;
    recognizer.extract_features(images, embeddings);

    cout << "Extracted " << embeddings.size() << " embedding(s)" << endl;

    // Display embeddings
    for (size_t i = 0; i < embeddings.size(); i++) {
        cout << "\nImage " << (i + 1) << " embedding (first 10 values): ";
        for (size_t j = 0; j < min(10UL, embeddings[i].size()); j++) {
            cout << fixed << setprecision(4) << embeddings[i][j] << " ";
        }
        cout << "..." << endl;
    }

    // Calculate similarities if multiple images
    if (embeddings.size() >= 2) {
        cout << "\n=== Similarity Matrix ===" << endl;
        cout << "      ";
        for (size_t i = 0; i < embeddings.size(); i++) {
            cout << setw(8) << "Img" << (i + 1);
        }
        cout << endl;

        for (size_t i = 0; i < embeddings.size(); i++) {
            cout << "Img" << setw(2) << (i + 1) << "  ";
            for (size_t j = 0; j < embeddings.size(); j++) {
                float sim = util::cosine_similarity(embeddings[i], embeddings[j]);
                cout << setw(8) << fixed << setprecision(4) << sim;
            }
            cout << endl;
        }

        // Detailed comparison
        cout << "\n=== Detailed Comparisons ===" << endl;
        for (size_t i = 0; i < embeddings.size(); i++) {
            for (size_t j = i + 1; j < embeddings.size(); j++) {
                float cosine_sim = util::cosine_similarity(embeddings[i], embeddings[j]);
                float l2_dist = util::l2_distance(embeddings[i], embeddings[j]);
                cout << "Image " << (i + 1) << " vs Image " << (j + 1) << ":" << endl;
                cout << "  Cosine Similarity: " << fixed << setprecision(4) << cosine_sim << endl;
                cout << "  L2 Distance: " << fixed << setprecision(4) << l2_dist << endl;
                if (cosine_sim > 0.6) {
                    cout << "  -> Same person (high similarity)" << endl;
                } else if (cosine_sim < 0.4) {
                    cout << "  -> Different person (low similarity)" << endl;
                } else {
                    cout << "  -> Uncertain" << endl;
                }
                cout << endl;
            }
        }
    }

    cout << "\nTest completed successfully!" << endl;
    return 0;
}





