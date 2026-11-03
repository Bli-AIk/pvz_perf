"""Verify generated float UVs against authoritative pixel atlas regions."""
import re
import struct

FLOAT = r"([-+.\deE]+)f"
REGION = r"\{\s*" + r"\s*,\s*".join([FLOAT]*4 + [r"(-?\d+)"]*2) + r"\s*\}"

def regions(text):
    return [tuple(map(float, row[:4])) + tuple(map(int, row[4:]))
            for row in re.findall(REGION, text)]

def assert_regions(test, actual, expected):
    test.assertEqual(len(actual), len(expected))
    for uv, (rect, width, height) in zip(actual, expected):
        x,y,w,h = rect
        target = (x/width,y/height,(x+w)/width,(y+h)/height) if w and h else (0,0,0,0)
        test.assertEqual(tuple(struct.pack("<f",v) for v in uv[:4]),
                         tuple(struct.pack("<f",v) for v in target), (uv,rect,width,height))
        test.assertEqual(uv[4:], (w,h))
