/**
 * @file cvedix_objects_cereal_archive.h
 * @brief Cereal serialization functions for Core AI Runtime objects
 * 
 * This file defines EXTERNAL archive functions for serializing pipeline objects
 * using the Cereal C++ serialization library. These functions enable objects
 * to be converted to/from JSON, XML, and binary formats.
 * 
 * @section cereal_overview Overview
 * Cereal is a header-only C++11 serialization library. This file provides
 * `serialize()` template functions that define how each object type is
 * converted to structured data formats.
 * 
 * @section cereal_supported Supported Object Types
 * - **cvedix_frame_target**: Generic detected objects
 * - **cvedix_sub_target**: Sub-regions within targets
 * - **cvedix_frame_face_target**: Face detection/recognition results
 * - **cvedix_frame_text_target**: OCR/text detection results
 * - **cvedix_frame_pose_target**: Body pose estimation results
 * 
 * @section cereal_formats Supported Formats
 * - **JSON**: Human-readable, for APIs and debugging
 * - **XML**: Legacy system integration
 * - **Binary**: Fast serialization for internal use
 * 
 * @section cereal_usage Usage Example
 * @code
 * #include "cvedix_objects_cereal_archive.h"
 * #include <sstream>
 * 
 * // Serialize target to JSON
 * cvedix_objects::cvedix_frame_target target;
 * std::stringstream ss;
 * {
 *     cereal::JSONOutputArchive archive(ss);
 *     archive(target);
 * }
 * std::string json = ss.str();
 * @endcode
 * 
 * @see https://uscilab.github.io/cereal/serialization_functions.html
 * @see cvedix_msg_broker_node Uses these functions for message formatting
 */

#pragma once

// object types
#include "cvedix/objects/cvedix_frame_target.h"
#include "cvedix/objects/cvedix_frame_face_target.h"
#include "cvedix/objects/cvedix_frame_text_target.h"
#include "cvedix/objects/cvedix_frame_pose_target.h"
#include "cvedix/objects/cvedix_sub_target.h"
/* extend for more types of objects in SDK. */

// headers from cereal
#include "cvedix/third_party/cereal/cereal.hpp"
#include "cvedix/third_party/cereal/types/vector.hpp"
#include "cvedix/third_party/cereal/types/memory.hpp"
#include "cvedix/third_party/cereal/types/string.hpp"
#include "cvedix/third_party/cereal/types/utility.hpp"
#include "cvedix/third_party/cereal/archives/json.hpp"
#include "cvedix/third_party/cereal/archives/xml.hpp"

/* same namespace as object types */
namespace cvedix_objects {

    /**
     * @brief Serialize cvedix_frame_target to/from archive
     * 
     * Serializes all detection result fields including bounding box,
     * classification, tracking, and embeddings.
     * 
     * @tparam Archive Cereal archive type (JSONOutputArchive, etc.)
     * @param archive The archive to read from or write to
     * @param target The target object to serialize
     */
    template<typename Archive>
    void serialize(Archive& archive, cvedix_frame_target& target) {
        archive(cereal::make_nvp("x", target.x),
                cereal::make_nvp("y", target.y),
                cereal::make_nvp("width", target.width),
                cereal::make_nvp("height", target.height),
                cereal::make_nvp("primary_class_id", target.primary_class_id),
                cereal::make_nvp("primary_score", target.primary_score),
                cereal::make_nvp("primary_label", target.primary_label),
                cereal::make_nvp("channel_index", target.channel_index),
                cereal::make_nvp("frame_index", target.frame_index),
                cereal::make_nvp("track_id", target.track_id),
                cereal::make_nvp("secondary_class_ids", target.secondary_class_ids),
                cereal::make_nvp("secondary_scores", target.secondary_scores),
                cereal::make_nvp("secondary_labels", target.secondary_labels),
                cereal::make_nvp("sub_targets", target.sub_targets),
                cereal::make_nvp("embeddings", target.embeddings));
    }

    /**
     * @brief Serialize cvedix_sub_target to/from archive
     * 
     * Serializes sub-region detection results within a parent target.
     * 
     * @tparam Archive Cereal archive type
     * @param archive The archive to read from or write to
     * @param target The sub-target object to serialize
     */
    template<typename Archive>
    void serialize(Archive& archive, cvedix_sub_target& target) {
        archive(cereal::make_nvp("x", target.x),
                cereal::make_nvp("y", target.y),
                cereal::make_nvp("width", target.width),
                cereal::make_nvp("height", target.height),
                cereal::make_nvp("class_id", target.class_id),
                cereal::make_nvp("score", target.score),
                cereal::make_nvp("label", target.label),
                cereal::make_nvp("frame_index", target.frame_index),
                cereal::make_nvp("channel_index", target.channel_index),
                cereal::make_nvp("attachments", target.attachments));
    }

    /**
     * @brief Serialize cvedix_frame_face_target to/from archive
     * 
     * Serializes face detection results including bounding box,
     * facial keypoints, and recognition embeddings.
     * 
     * @tparam Archive Cereal archive type
     * @param archive The archive to read from or write to
     * @param target The face target object to serialize
     */
    template<typename Archive>
    void serialize(Archive& archive, cvedix_frame_face_target& target) {
        archive(cereal::make_nvp("x", target.x),
                cereal::make_nvp("y", target.y),
                cereal::make_nvp("width", target.width),
                cereal::make_nvp("height", target.height),
                cereal::make_nvp("score", target.score),
                cereal::make_nvp("embeddings", target.embeddings),
                cereal::make_nvp("key_points", target.key_points),
                cereal::make_nvp("track_id", target.track_id));
    }

    /**
     * @brief Serialize cvedix_frame_text_target to/from archive
     * 
     * Serializes OCR/text detection results including recognized text,
     * confidence score, and bounding polygon.
     * 
     * @tparam Archive Cereal archive type
     * @param archive The archive to read from or write to
     * @param target The text target object to serialize
     */
    template<typename Archive>
    void serialize(Archive& archive, cvedix_frame_text_target& target) {
        archive(cereal::make_nvp("text", target.text),
                cereal::make_nvp("score", target.score),
                cereal::make_nvp("region", target.region_vertexes),
                cereal::make_nvp("flags", target.flags));
    }

    /**
     * @brief Serialize cvedix_frame_pose_target to/from archive
     * 
     * Serializes body pose estimation results.
     * 
     * @tparam Archive Cereal archive type
     * @param archive The archive to read from or write to
     * @param target The pose target object to serialize
     * 
     * @todo Implementation pending - add pose keypoints serialization
     */
    template<typename Archive>
    void serialize(Archive& archive, cvedix_frame_pose_target& target) {
        // TODO: Add pose keypoints serialization
    }

}