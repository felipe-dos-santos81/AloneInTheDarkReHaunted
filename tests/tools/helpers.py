"""Builders for synthetic game data used by the texture tool tests."""
import struct


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
