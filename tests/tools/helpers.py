"""Builders for synthetic game data used by the texture tool tests."""
import struct

import numpy as np


def pak_bytes(entries, flag=0):
    """Build a .PAK image with raw (flag 0) entries, in the layout
    aitd_textures.pak.Pak reads: u32 pad, u32 offsets[n], then per entry
    u32 add(=0), u32 disc, u32 uncomp, u8 flag, u8 info5, u16 name_len,
    payload."""
    blobs = [
        struct.pack("<I", 0) + struct.pack("<IIBBH", len(e), len(e), flag, 0, 0) + e
        for e in entries
    ]
    table_end = 4 * (len(entries) + 1)
    offsets, pos = [], table_end
    for blob in blobs:
        offsets.append(pos)
        pos += len(blob)
    head = struct.pack("<I", 0) + b"".join(struct.pack("<I", o) for o in offsets)
    assert len(head) == table_end
    return head + b"".join(blobs)


SYNTHETIC_CAMERA_COUNTS = (2, 1, 1, 1, 1, 1, 1, 1)
SYNTHETIC_SCREEN_ENTRIES = (6, 7, 8, 10, 11, 12, 14, 15, 16, 17, 18, 19)


def synthetic_palette():
    return bytes((i, 255 - i, i // 2)[c] for i in range(256) for c in range(3))


def synthetic_plate(seed):
    return ((np.arange(64000) + seed) % 256).astype(np.uint8).tobytes()


def write_synthetic_data_dir(d):
    """Fill folder `d` like a real INDARK, with raw (uncompressed) entries:
    8 CAMERA PAKs and a 20-entry ITD_RESS whose entry 3 is the palette,
    entries 6..19 (minus 9 and 13) are plates, and entry 13 is the title
    screen with a u16 header, its own inverted palette, then pixels."""
    d.mkdir(parents=True, exist_ok=True)
    for floor, n in enumerate(SYNTHETIC_CAMERA_COUNTS):
        plates = [synthetic_plate(floor * 10 + i) for i in range(n)]
        (d / f"CAMERA{floor:02d}.PAK").write_bytes(pak_bytes(plates))
    palette = synthetic_palette()
    entries = [b"filler"] * 20
    entries[3] = palette
    for e in SYNTHETIC_SCREEN_ENTRIES:
        entries[e] = synthetic_plate(100 + e)
    titre_palette = bytes(255 - b for b in palette)
    entries[13] = b"\x56\x50" + titre_palette + synthetic_plate(113)
    (d / "ITD_RESS.PAK").write_bytes(pak_bytes(entries))
    return d
