# SPDX-License-Identifier: GPL-2.0-only
import pytest

from aitd_data import lang
from aitd_data.pak import Pak

EN0 = "@1:Alone in the Dark\n@2:©1992\n\n@10:You cannot carry anything else\n"
EN_DOC = "#CTitle\n#G4\nSome text.\n#Pmore\n"


def test_encode_decode_round_trip_keeps_bytes():
    raw = b"@1:Caf\x82 \xa91992\r\n@2:n\xc6o\r\n\x1a\r\n"
    text = lang.decode(raw)
    assert text == "@1:Café ©1992\n@2:não\n"
    assert lang.encode(text) == raw


@pytest.mark.parametrize("variant", ["\r\n", "﻿"])
def test_windows_line_ends_and_bom_pack_identically(variant):
    text = "@1:não\n@2:mão\n"
    edited = ("﻿" + text) if variant == "﻿" else text.replace("\n", "\r\n")
    assert lang.encode(edited) == lang.encode(text)


def test_bad_chars_reports_unencodable_and_missing_glyphs():
    # ã composes, Á draws as A; ñ exists in CP850 but the Portuguese set excludes it,
    # and the em dash is not in CP850 at all.
    assert lang.bad_chars("@1:não Á\n@2:niño — x\n") == [(2, "ñ"), (2, "—")]


def test_validate_message_numbers_and_image_codes():
    en = [lang.encode(EN0), lang.encode(EN_DOC)]
    ok = [lang.encode("@1:Sozinho no Escuro\n@2:©1992\n\n@10:Você não pode carregar mais nada\n"),
          lang.encode("#CTítulo\n#G4\nAlgum texto.\n#Pmais\n")]
    assert lang.validate(ok, en) == []
    missing = [lang.encode("@1:x\n"), ok[1]]
    assert lang.validate(missing, en) == ["messages: missing @2, @10"]
    extra = [lang.encode(EN0 + "@11:x\n"), ok[1]]
    assert lang.validate(extra, en) == ["messages: extra @11"]
    no_image = [ok[0], lang.encode("#CTítulo\nAlgum texto.\n")]
    assert lang.validate(no_image, en) == ["doc01: image codes [] != English ['#G4']"]


def test_pak_image_reads_back_through_pak(tmp_path):
    entries = [lang.encode(EN0), lang.encode(EN_DOC)]
    path = tmp_path / "PORTUGUE.PAK"
    path.write_bytes(lang.pak_image(entries))
    pak = Pak(path)
    assert pak.count == 2
    assert [pak.read(i) for i in range(2)] == entries
    assert int.from_bytes(path.read_bytes()[4:8], "little") == 4 * (len(entries) + 1)


def test_embedded_cpp_matches_the_registry_format():
    cpp = lang.embedded_cpp("PORTUGUE", bytes(range(20)))
    assert "extern const unsigned char embdata_PORTUGUE_PAK[] = {" in cpp
    assert "    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,\n" in cpp
    assert cpp.rstrip().endswith("extern const unsigned long long embdata_PORTUGUE_PAK_size = 20ULL;")
