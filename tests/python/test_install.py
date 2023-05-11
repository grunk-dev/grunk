from grunk.plugin_manager import install


def test_install():
    """
    test the install command
    """
    install("reflect", "0.1.4")
