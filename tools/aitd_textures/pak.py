# SPDX-License-Identifier: GPL-2.0-only
# Ported from m-aitd (PyAitD/engine/data/pak.py), GPL-2.0-only; only the
# explode import changed.
"""Reader for AITD .PAK archives (offset table + per-entry compression)."""
import pathlib
import struct
import zlib
from dataclasses import dataclass

from .explode import explode

FLAG_RAW = 0
FLAG_EXPLODE = 1
FLAG_DEFLATE = 4


class PakError(Exception):
    pass


@dataclass
class PakInfo:
    disc_size: int
    uncompressed_size: int
    flag: int
    info5: int
    name: str


class Pak:
    def __init__(self, path):
        self.path = pathlib.Path(path)
        if not self.path.is_file():
            raise PakError(f"PAK not found: {self.path}")
        self._data = self.path.read_bytes()
        if len(self._data) < 8:
            raise PakError(f"PAK too small: {self.path}")
        try:
            table_end = struct.unpack_from("<I", self._data, 4)[0]
            self._offsets = [
                struct.unpack_from("<I", self._data, (i + 1) * 4)[0]
                for i in range(table_end // 4 - 1)
            ]
        except struct.error:
            raise PakError(f"{self.path.name}: corrupt offset table")

    @property
    def count(self):
        return len(self._offsets)

    def _header(self, index):
        if not 0 <= index < self.count:
            raise PakError(f"{self.path.name}: entry {index} out of range (0..{self.count - 1})")
        off = self._offsets[index]
        data = self._data
        try:
            add = struct.unpack_from("<I", data, off)[0]
            p = off + 4 + (add - 4 if add else 0)
            disc, uncomp = struct.unpack_from("<II", data, p)
            flag, info5, name_len = struct.unpack_from("<BBH", data, p + 8)
        except struct.error:
            raise PakError(f"{self.path.name}: entry {index} corrupt header")
        name = data[p + 12 : p + 12 + name_len]
        name = name[2:].decode("ascii", "replace") if name_len > 2 else ""
        payload = p + 12 + name_len
        return PakInfo(disc, uncomp, flag, info5, name), payload

    def info(self, index):
        return self._header(index)[0]

    def read(self, index):
        info, payload = self._header(index)
        data = self._data
        raw = data[payload : payload + info.disc_size]
        if len(raw) < info.disc_size:
            raise PakError(f"{self.path.name}: entry {index} truncated")
        if info.flag == FLAG_RAW:
            out = raw
        elif info.flag == FLAG_EXPLODE:
            out = explode(raw, info.uncompressed_size, info.info5)
        elif info.flag == FLAG_DEFLATE:
            try:
                out = zlib.decompressobj(-15).decompress(raw)
            except zlib.error:
                raise PakError(f"{self.path.name}: entry {index} invalid deflate data")
        else:
            raise PakError(f"{self.path.name}: entry {index} unknown flag {info.flag}")
        if len(out) != info.uncompressed_size:
            raise PakError(
                f"{self.path.name}: entry {index} size {len(out)} != {info.uncompressed_size}"
            )
        return out
