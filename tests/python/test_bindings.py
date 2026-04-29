# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

import pytest
import grunk 

def test_operators_python():

    grnk = grunk.state()
    env = grnk.create_env()
    env.eval("""
        x = 1.2
        y = 3.4
    """)
    x = env["x"]
    y = env["y"]

    z = x + y
    assert pytest.approx(z.as_float()) == 4.6
    
    z_sub = x - y
    assert pytest.approx(z_sub.as_float()) == -2.2

    z_mul = x * y
    assert pytest.approx(z_mul.as_float()) == 4.08

    z_div = x / y
    assert pytest.approx(z_div.as_float()) == 0.35294117647058826

    z_pow = x ** y
    assert pytest.approx(z_pow.as_float()) == 1.2 ** 3.4

    z_mod = x % y
    assert pytest.approx(z_mod.as_float()) == 1.2 % 3.4

    z_neg = -x
    assert pytest.approx(z_neg.as_float()) == -1.2


def test_operators_lua():
    
    grnk = grunk.state()
    env = grnk.create_env()
    env.eval("""
        x = 1.2
        y = 3.4
             
        z_add = x + y
        z_sub = x - y
        z_mul = x * y
        z_div = x / y
        z_pow = x ^ y
        z_mod = x % y
        z_neg = -x
    """)
    z_add = env["z_add"]
    z_sub = env["z_sub"]
    z_mul = env["z_mul"]
    z_div = env["z_div"]
    z_pow = env["z_pow"]
    z_mod = env["z_mod"]
    z_neg = env["z_neg"]

    assert pytest.approx(z_add.as_float()) == 4.6
    assert pytest.approx(z_sub.as_float()) == -2.2
    assert pytest.approx(z_mul.as_float()) == 4.08
    assert pytest.approx(z_div.as_float()) == 0.35294117647058826
    assert pytest.approx(z_pow.as_float()) == 1.2 ** 3.4
    assert pytest.approx(z_mod.as_float()) == 1.2 % 3.4
    assert pytest.approx(z_neg.as_float()) == -1.2


def test_operators_feature():

    grnk = grunk.state()
    fx = grnk.feature(1.2)
    fy = grnk.feature(3.4)
    fz_add = fx + fy
    fz_sub = fx - fy
    fz_mul = fx * fy
    fz_div = fx / fy
    fz_pow = fx ** fy
    fz_mod = fx % fy
    fz_neg = -fx

    assert not fz_add.is_valid()
    assert not fz_sub.is_valid()
    assert not fz_mul.is_valid()
    assert not fz_div.is_valid()
    assert not fz_pow.is_valid()
    assert not fz_mod.is_valid()
    assert not fz_neg.is_valid()

    assert pytest.approx(fz_add.value().as_float()) == 4.6
    assert pytest.approx(fz_sub.value().as_float()) == -2.2
    assert pytest.approx(fz_mul.value().as_float()) == 4.08
    assert pytest.approx(fz_div.value().as_float()) == 0.35294117647058826
    assert pytest.approx(fz_pow.value().as_float()) == 1.2 ** 3.4
    assert pytest.approx(fz_mod.value().as_float()) == 1.2 % 3.4
    assert pytest.approx(fz_neg.value().as_float()) == -1.2

    assert fz_add.is_valid()
    assert fz_sub.is_valid()
    assert fz_mul.is_valid()
    assert fz_div.is_valid()
    assert fz_pow.is_valid()
    assert fz_mod.is_valid()
    assert fz_neg.is_valid()

    fy.set_value(1.2)

    assert not fz_add.is_valid()
    assert not fz_sub.is_valid()
    assert not fz_mul.is_valid()
    assert not fz_div.is_valid()
    assert not fz_pow.is_valid()
    assert not fz_mod.is_valid()
    assert fz_neg.is_valid()

    assert pytest.approx(fz_add.value().as_float()) == 2.4
    assert pytest.approx(fz_sub.value().as_float()) == 0.0
    assert pytest.approx(fz_mul.value().as_float()) == 1.44
    assert pytest.approx(fz_div.value().as_float()) == 1.0
    assert pytest.approx(fz_pow.value().as_float()) == 1.2 ** 1.2
    assert pytest.approx(fz_mod.value().as_float()) == 0.0
    assert pytest.approx(fz_neg.value().as_float()) == -1.2


def test_feature_id():

    grnk = grunk.state()
    
    e = grnk.create_parametric_env()
    e.eval("""
        x = grunk.feature(1.2)
        y = grunk.feature(3.4):with_id("a")           
    """)
    x = e.get_feature("x")
    y = e.get_feature("y")
    assert x.id() == ""
    assert y.id() == "a"
    e.tag_features()
    assert x.id() == "x"
    assert y.id() == "y"
    x.set_id("b")
    assert x.id() == "b"