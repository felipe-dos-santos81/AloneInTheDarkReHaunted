from aitd_textures.catalog import (
    MENU_FRAME_RE,
    PALETTE_ENTRY,
    PALETTE_PAK,
    TITRE_ENTRY,
    ImageSpec,
    anim_folder,
    animation_active,
    animation_engine,
    animation_floor,
    animation_kind,
    animation_max_frames,
    camera_pak_name,
    camera_specs,
    kind_of_pak,
    menu_frame_name,
    parse_target,
    screen_specs,
    target_for_source,
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


def test_target_for_source_accepts_the_exporters_own_layout():
    assert target_for_source("backgrounds/CAMERA02_007.png") == "CAMERA02_007.png"
    assert target_for_source("screens/ITD_RESS_013.png") == "ITD_RESS_013.png"


def test_target_for_source_accepts_the_m_aitd_layout():
    assert target_for_source("backgrounds/floor02/camera007.png") == "CAMERA02_007.png"
    assert target_for_source("backgrounds/floor00/camera000.png") == "CAMERA00_000.png"
    assert target_for_source("screens/ress06.png") == "ITD_RESS_006.png"
    assert target_for_source("screens/ress13.png") == "ITD_RESS_013.png"


def test_target_for_source_maps_alt_backgrounds_to_their_itd_ress_entry():
    # AITD1.h: ITD_RESS 15..19 are CAM07000, CAM07001, CAM06000, CAM06005, CAM06008.
    assert target_for_source("alt_backgrounds/floor06/camera000.png") == "ITD_RESS_017.png"
    assert target_for_source("alt_backgrounds/floor06/camera005.png") == "ITD_RESS_018.png"
    assert target_for_source("alt_backgrounds/floor06/camera008.png") == "ITD_RESS_019.png"
    assert target_for_source("alt_backgrounds/floor07/camera000.png") == "ITD_RESS_015.png"
    assert target_for_source("alt_backgrounds/floor07/camera001.png") == "ITD_RESS_016.png"


def test_target_for_source_rejects_everything_else():
    for bad in ("backgrounds/floor08/camera000.png",     # no CAMERA08 exists
                "backgrounds/floor02/camera7.png",       # wrong digit count
                "alt_backgrounds/floor06/camera001.png", # not a sorcerer variant
                "screens/ress6.png",
                "guides/floor00/camera000.png",          # not a source folder
                "backgrounds/CAMERA02_007_DARK.png",
                "backgrounds/deep/floor02/camera007.png"):
        assert target_for_source(bad) is None, bad


def test_animation_name_rules():
    assert [animation_kind(n) for n in ("CAMERA03_008", "ITD_RESS_002_NOTATOU", "StartupMenuBackground")] == [
        "camera", "screen", "menu"]
    assert animation_floor("CAMERA07_004") == 7
    assert animation_floor("ITD_RESS_011") is None and animation_floor("StartupMenuBackground") is None
    assert animation_engine("CAMERA03_008") == "anim_CAMERA03_008/"
    assert animation_engine("ITD_RESS_002_NOTATOU") == "anim_ITD_RESS_002_NOTATOU/"
    assert animation_engine("StartupMenuBackground") == "StartupMenuBackground_NNN.png"
    assert animation_max_frames("StartupMenuBackground") == 512
    assert animation_max_frames("CAMERA03_008") is None
    assert animation_active("ITD_RESS_012_DISABLED") is False
    assert animation_active("ITD_RESS_002_NOTATOU") is True
    assert anim_folder("CAMERA07_004") == "anim_CAMERA07_004"
    assert menu_frame_name(7) == "StartupMenuBackground_007.png"
    assert MENU_FRAME_RE.match("StartupMenuBackground_512.png")
    assert not MENU_FRAME_RE.match("StartupMenuBackground.png")
    assert not MENU_FRAME_RE.match("StartupMenuBackgroundWithArt_001.png")
