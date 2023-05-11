from grunk._util import deconstruct_package_string, reconstruct_package_string
import pytest


def test_deconstruct_package_string():

    p, v, u, c = deconstruct_package_string("pack/0.42.42@u/c")
    assert p == "pack"
    assert v == "0.42.42"
    assert u == "u"
    assert c == "c"

    p, v, u, c = deconstruct_package_string("pack")
    assert p == "pack"
    assert v is None
    assert u is None
    assert c is None

    p, v, u, c = deconstruct_package_string("pack/[>1.1 <2.1]")
    assert p == "pack"
    assert v == "[>1.1 <2.1]"
    assert u is None
    assert c is None

    with pytest.raises(ValueError):
        # no package/version
        p, v, u, c = deconstruct_package_string("@u/c")

    with pytest.raises(ValueError):
        # too many "@"-splits
        p, v, u, c = deconstruct_package_string("too@many@splits")

    with pytest.raises(ValueError):
        # too many "/"-splits in package/version
        p, v, u, c = deconstruct_package_string("too/many/splits@u/c")

    with pytest.raises(ValueError):
        # no splits in user/channel
        p, v, u, c = deconstruct_package_string("abc/0.0.1@nosplits")

    with pytest.raises(ValueError):
        # too many "/"-splits in user/channel
        p, v, u, c = deconstruct_package_string("abc/0.0.1@too/many/splits")


def test_reconstruct_package_string():
    s = "pack/0.42.42@u/c"
    assert reconstruct_package_string(*deconstruct_package_string(s)) == s

    s = "pack"
    assert reconstruct_package_string(*deconstruct_package_string(s)) == s

    s = "pack/[>1.1 <2.1]"
    assert reconstruct_package_string(*deconstruct_package_string(s)) == s
