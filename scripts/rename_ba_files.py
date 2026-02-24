#!/usr/bin/env python3
"""Rename BA node files to use ba_line_ and ba_area_ prefixes."""
import os

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

RENAMES = [
    # BA nodes
    ('nodes/ba/cvedix_ba_crossline_node.h', 'nodes/ba/cvedix_ba_line_crossline_node.h'),
    ('nodes/ba/cvedix_ba_crossline_node.cpp', 'nodes/ba/cvedix_ba_line_crossline_node.cpp'),
    ('nodes/ba/cvedix_ba_jam_node.h', 'nodes/ba/cvedix_ba_area_jam_node.h'),
    ('nodes/ba/cvedix_ba_jam_node.cpp', 'nodes/ba/cvedix_ba_area_jam_node.cpp'),
    ('nodes/ba/cvedix_ba_crowding_node.h', 'nodes/ba/cvedix_ba_area_crowding_node.h'),
    ('nodes/ba/cvedix_ba_crowding_node.cpp', 'nodes/ba/cvedix_ba_area_crowding_node.cpp'),
    ('nodes/ba/cvedix_ba_loitering_node.h', 'nodes/ba/cvedix_ba_area_loitering_node.h'),
    ('nodes/ba/cvedix_ba_loitering_node.cpp', 'nodes/ba/cvedix_ba_area_loitering_node.cpp'),
    ('nodes/ba/cvedix_ba_line_counting.h', 'nodes/ba/cvedix_ba_line_counting_node.h'),
    ('nodes/ba/cvedix_ba_line_counting.cpp', 'nodes/ba/cvedix_ba_line_counting_node.cpp'),
    # OSD nodes
    ('nodes/osd/cvedix_ba_crossline_osd_node.h', 'nodes/osd/cvedix_ba_line_crossline_osd_node.h'),
    ('nodes/osd/cvedix_ba_crossline_osd_node.cpp', 'nodes/osd/cvedix_ba_line_crossline_osd_node.cpp'),
    ('nodes/osd/cvedix_ba_jam_osd_node.h', 'nodes/osd/cvedix_ba_area_jam_osd_node.h'),
    ('nodes/osd/cvedix_ba_jam_osd_node.cpp', 'nodes/osd/cvedix_ba_area_jam_osd_node.cpp'),
    ('nodes/osd/cvedix_ba_crowding_osd_node.h', 'nodes/osd/cvedix_ba_area_crowding_osd_node.h'),
    ('nodes/osd/cvedix_ba_crowding_osd_node.cpp', 'nodes/osd/cvedix_ba_area_crowding_osd_node.cpp'),
]

ok = 0
skip = 0
for old, new in RENAMES:
    src = os.path.join(BASE, old)
    dst = os.path.join(BASE, new)
    if os.path.exists(src):
        os.rename(src, dst)
        print(f'  ✓ {old} → {new}')
        ok += 1
    else:
        print(f'  ⊘ {old} (already renamed or not found)')
        skip += 1

print(f'\nDone! Renamed: {ok}, Skipped: {skip}')
