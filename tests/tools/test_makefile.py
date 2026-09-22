import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]


def make_n(*args):
    proc = subprocess.run(["make", "-n", *args], cwd=ROOT, capture_output=True, text=True)
    assert proc.returncode == 0, proc.stderr
    return proc.stdout


def make_run(*args):
    proc = subprocess.run(["make", *args], cwd=ROOT, capture_output=True, text=True)
    assert proc.returncode == 0, proc.stderr
    return proc.stdout


def test_export_textures_target():
    out = make_n("export-textures", "gamedata=/g", "textures=/t", "anims=/x")
    assert 'tools/textures.py export --data "/g" --out "/t" --anims "/x"' in out


def test_export_textures_reads_animations_from_dest_by_default():
    assert '--anims "Assets/backgrounds_hd"' in make_n("export-textures")
    assert '--anims "/d"' in make_n("export-textures", "dest=/d")


def test_check_textures_is_import_dry_run():
    out = make_n("check-textures", "textures_ai=/a", "dest=/d", "textures=/t", "dark=all")
    assert 'tools/textures.py import --src "/a" --dest "/d" --originals "/t" --dark "all" --dry-run' in out


def test_import_textures_target():
    out = make_n("import-textures", "textures_ai=/a", "dest=/d", "textures=/t", "dark=none")
    assert 'tools/textures.py import --src "/a" --dest "/d" --originals "/t" --dark "none"' in out
    assert "--dry-run" not in out


def test_import_textures_defaults():
    out = make_n("import-textures")
    assert '--src "data/textures-ai" --dest "Assets/backgrounds_hd" --originals "data/textures" --dark "mirror"' in out


def test_hd_install_packs_assets_into_the_source_tree():
    out = make_n("hd-install")
    assert '"Assets/backgrounds_hd" "TatouSource/backgrounds_hd.hda"' in out


def test_help_lists_the_texture_targets():
    out = make_run("help")
    for target in ("export-textures", "check-textures", "import-textures", "hd-install", "tools-deps", "test-tools"):
        assert target in out
