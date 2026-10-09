"""Builders for synthetic game data used by the tool tests."""
from aitd_data.pak import pak_image


def pak_bytes(entries, flag=0):
    """A .PAK image of uncompressed entries (aitd_data.pak.pak_image)."""
    return pak_image(entries, flag)


def synthetic_palette():
    return bytes((i, 255 - i, i // 2)[c] for i in range(256) for c in range(3))

