#!/bin/bash
# Rename BA node files to use ba_line_ and ba_area_ prefixes
# Run from project root: bash scripts/rename_ba_files.sh

set -e
cd "$(dirname "$0")/.."

echo "=== Renaming BA node files ==="

# BA nodes (nodes/ba/)
mv nodes/ba/cvedix_ba_crossline_node.h nodes/ba/cvedix_ba_line_crossline_node.h
mv nodes/ba/cvedix_ba_crossline_node.cpp nodes/ba/cvedix_ba_line_crossline_node.cpp
mv nodes/ba/cvedix_ba_jam_node.h nodes/ba/cvedix_ba_area_jam_node.h
mv nodes/ba/cvedix_ba_jam_node.cpp nodes/ba/cvedix_ba_area_jam_node.cpp
mv nodes/ba/cvedix_ba_crowding_node.h nodes/ba/cvedix_ba_area_crowding_node.h
mv nodes/ba/cvedix_ba_crowding_node.cpp nodes/ba/cvedix_ba_area_crowding_node.cpp
mv nodes/ba/cvedix_ba_loitering_node.h nodes/ba/cvedix_ba_area_loitering_node.h
mv nodes/ba/cvedix_ba_loitering_node.cpp nodes/ba/cvedix_ba_area_loitering_node.cpp
mv nodes/ba/cvedix_ba_line_counting.h nodes/ba/cvedix_ba_line_counting_node.h
mv nodes/ba/cvedix_ba_line_counting.cpp nodes/ba/cvedix_ba_line_counting_node.cpp

# OSD nodes (nodes/osd/)
mv nodes/osd/cvedix_ba_crossline_osd_node.h nodes/osd/cvedix_ba_line_crossline_osd_node.h
mv nodes/osd/cvedix_ba_crossline_osd_node.cpp nodes/osd/cvedix_ba_line_crossline_osd_node.cpp
mv nodes/osd/cvedix_ba_jam_osd_node.h nodes/osd/cvedix_ba_area_jam_osd_node.h
mv nodes/osd/cvedix_ba_jam_osd_node.cpp nodes/osd/cvedix_ba_area_jam_osd_node.cpp
mv nodes/osd/cvedix_ba_crowding_osd_node.h nodes/osd/cvedix_ba_area_crowding_osd_node.h
mv nodes/osd/cvedix_ba_crowding_osd_node.cpp nodes/osd/cvedix_ba_area_crowding_osd_node.cpp

echo "=== Done! All BA files renamed ==="
echo ""
echo "Next: run 'cmake ..' from build directory to regenerate build files"
