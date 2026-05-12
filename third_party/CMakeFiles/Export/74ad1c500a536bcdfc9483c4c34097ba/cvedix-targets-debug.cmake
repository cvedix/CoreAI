#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "cvedix::tinyexpr" for configuration "Debug"
set_property(TARGET cvedix::tinyexpr APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(cvedix::tinyexpr PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/cvedix/libtinyexpr.so"
  IMPORTED_SONAME_DEBUG "libtinyexpr.so"
  )

list(APPEND _cmake_import_check_targets cvedix::tinyexpr )
list(APPEND _cmake_import_check_files_for_cvedix::tinyexpr "${_IMPORT_PREFIX}/lib/cvedix/libtinyexpr.so" )

# Import target "cvedix::cvedix_yolo_ort_detector" for configuration "Debug"
set_property(TARGET cvedix::cvedix_yolo_ort_detector APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(cvedix::cvedix_yolo_ort_detector PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG "opencv_calib3d;opencv_core;opencv_dnn;opencv_features2d;opencv_flann;opencv_highgui;opencv_imgcodecs;opencv_imgproc;opencv_ml;opencv_objdetect;opencv_photo;opencv_stitching;opencv_video;opencv_videoio;opencv_alphamat;opencv_aruco;opencv_bgsegm;opencv_bioinspired;opencv_ccalib;opencv_datasets;opencv_dnn_objdetect;opencv_dnn_superres;opencv_dpm;opencv_face;opencv_freetype;opencv_fuzzy;opencv_hfs;opencv_img_hash;opencv_intensity_transform;opencv_line_descriptor;opencv_mcc;opencv_optflow;opencv_phase_unwrapping;opencv_plot;opencv_quality;opencv_rapid;opencv_reg;opencv_rgbd;opencv_saliency;opencv_shape;opencv_stereo;opencv_structured_light;opencv_superres;opencv_surface_matching;opencv_text;opencv_tracking;opencv_videostab;opencv_wechat_qrcode;opencv_xfeatures2d;opencv_ximgproc;opencv_xobjdetect;opencv_xphoto"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/cvedix/libcvedix_yolo_ort_detector.so"
  IMPORTED_SONAME_DEBUG "libcvedix_yolo_ort_detector.so"
  )

list(APPEND _cmake_import_check_targets cvedix::cvedix_yolo_ort_detector )
list(APPEND _cmake_import_check_files_for_cvedix::cvedix_yolo_ort_detector "${_IMPORT_PREFIX}/lib/cvedix/libcvedix_yolo_ort_detector.so" )

# Import target "cvedix::cvedix_core" for configuration "Debug"
set_property(TARGET cvedix::cvedix_core APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(cvedix::cvedix_core PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG "cvedix::tinyexpr"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/cvedix/libcvedix_core.so"
  IMPORTED_SONAME_DEBUG "libcvedix_core.so"
  )

list(APPEND _cmake_import_check_targets cvedix::cvedix_core )
list(APPEND _cmake_import_check_files_for_cvedix::cvedix_core "${_IMPORT_PREFIX}/lib/cvedix/libcvedix_core.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
