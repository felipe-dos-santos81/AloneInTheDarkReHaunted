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


def test_hd_install_packs_assets_into_the_source_tree():
    out = make_n("hd-install")
    assert '"Assets/backgrounds_hd" "TatouSource/backgrounds_hd.hda"' in out


def test_export_models_target():
    out = make_n("export-models", "gamedata=/g", "models=/m")
    assert 'tools/models.py export --data "/g" --out "/m"' in out
    assert "--bodies" not in out


def test_export_models_defaults_and_filter():
    assert '--data "data/aitd1" --out "data/models"' in make_n("export-models")
    assert '--bodies "LISTBODY_011,LISTBOD2_011"' in make_n("export-models", "bodies=LISTBODY_011,LISTBOD2_011")


def test_import_models_target():
    out = make_n("import-models", "gamedata=/g", "models=/m", "models_ai=/a", "models_hd=/d", "bodies=LISTBODY_011")
    assert 'tools/models.py import --data "/g" --models "/m" --src "/a" --dest "/d" --bodies "LISTBODY_011"' in out
    assert "--dry-run" not in out


def test_import_models_defaults():
    out = make_n("import-models")
    assert '--data "data/aitd1" --models "data/models" --src "data/models-ai" --dest "Assets/models_hd"' in out
    assert "--bodies" not in out


def test_check_models_is_import_dry_run():
    assert make_n("check-models").rstrip().endswith("--dry-run")


def test_check_models_writes_reports_when_asked():
    assert '--report "/r"' in make_n("check-models", "report=/r")
    assert "--report" not in make_n("check-models")


def test_identity_models_target():
    out = make_n("identity-models", "models_identity=/i", "bodies=LISTBODY_011")
    assert 'tools/models.py identity --models "data/models" --out "/i" --bodies "LISTBODY_011"' in out


def test_identity_models_never_writes_into_the_delivery_tree_by_default():
    # The identity meshes must not overwrite generated art in data/models-ai.
    out = make_n("identity-models", "bodies=LISTBODY_011")
    assert '--out "data/models-identity"' in out and "data/models-ai" not in out
    out = make_n("identity-models", "models_ai=/a", "bodies=LISTBODY_011")
    assert '--out "data/models-identity"' in out


def test_blender_models_target():
    out = make_n("blender-models", "models=/m", "models_ai=/a", "bodies=LISTBODY_011", "BLENDER=/b")
    assert ('tools/models.py blender --models "/m" --out "/a" --work "data/models-blender" '
            '--blender "/b" --bodies "LISTBODY_011"') in out


def test_help_lists_the_asset_targets():
    out = make_run("help")
    for target in ("hd-install", "tools-deps", "test-tools",
                   "export-models", "identity-models", "blender-models", "check-models", "import-models",
                   "models-install"):
        assert target in out


def test_models_install_mirrors_the_models_and_adds_the_atlases(tmp_path):
    models, atlases, bundle = tmp_path / "m", tmp_path / "a", tmp_path / "bundle"
    for f in (models / "new.hdm", models / "note.txt", atlases / "body_X.png", atlases / "Backups/old.png",
              bundle / "models_hd/stale.hdm", bundle / "models_hd/keep.txt", bundle / "atlases/point_X.png"):
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text("x")
    make_run("models-install", f"models_hd={models}", f"atlases={atlases}", f"BUNDLE_RESOURCES={bundle}")
    assert sorted(p.name for p in (bundle / "models_hd").iterdir()) == ["keep.txt", "new.hdm"]
    assert sorted(p.name for p in (bundle / "atlases").iterdir()) == ["body_X.png", "point_X.png"]
