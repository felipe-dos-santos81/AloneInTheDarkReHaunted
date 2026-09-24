def test_package_imports_and_has_version():
    import aitd_models

    assert aitd_models.__version__ == "0.1.0"
