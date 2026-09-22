from aitd_textures.catalog import (
    PALETTE_ENTRY,
    PALETTE_PAK,
    TITRE_ENTRY,
    ImageSpec,
    camera_pak_name,
    camera_specs,
    kind_of_pak,
    parse_target,
    screen_specs,
)


def test_engine_target_names_and_relative_paths():
    cam = ImageSpec("camera", "CAMERA02", 7, 2)
    assert cam.target == "CAMERA02_007.png"
    assert cam.rel_path == "backgrounds/CAMERA02_007.png"
    scr = ImageSpec("screen", "ITD_RESS", 13, None, 770, 2)
    assert scr.target == "ITD_RESS_013.png"
    assert scr.rel_path == "screens/ITD_RESS_013.png"


def test_camera_specs_cover_every_entry_of_a_floor():
    specs = camera_specs(3, 4)
    assert camera_pak_name(3) == "CAMERA03"
    assert [s.entry for s in specs] == [0, 1, 2, 3]
    assert all(s.kind == "camera" and s.pak == "CAMERA03" and s.floor == 3 for s in specs)
    assert all(s.pixel_offset == 0 and s.palette_offset is None for s in specs)


def test_screen_specs_are_the_thirteen_known_screens():
    specs = screen_specs()
    assert [s.entry for s in specs] == [6, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19]
    assert all(s.kind == "screen" and s.pak == "ITD_RESS" and s.floor is None for s in specs)
    titre = next(s for s in specs if s.entry == TITRE_ENTRY)
    assert (titre.pixel_offset, titre.palette_offset) == (770, 2)
    others = [s for s in specs if s.entry != TITRE_ENTRY]
    assert all(s.pixel_offset == 0 and s.palette_offset is None for s in others)


def test_palette_location():
    assert (PALETTE_PAK, PALETTE_ENTRY) == ("ITD_RESS", 3)


def test_parse_target_accepts_engine_names_only():
    assert parse_target("CAMERA02_007.png") == ("CAMERA02", 7)
    assert parse_target("ITD_RESS_013.png") == ("ITD_RESS", 13)
    for bad in ("CAMERA02_007_DARK.png", "camera02_007.png", "CAMERA08_000.png",
                "CAMERA00_0000.png", "CAMERA00_000.PNG", "anything.png"):
        assert parse_target(bad) is None, bad


def test_kind_of_pak():
    assert kind_of_pak("CAMERA05") == "camera"
    assert kind_of_pak("ITD_RESS") == "screen"
