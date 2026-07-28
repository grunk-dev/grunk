# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

import pytest
import grunk 


def test_local_state():

    # use a local state
    grnk = grunk.state()
    e1 = grnk.create_env()
    e1.eval("""
        x = 1.2
        y = 3.4
    """)

    # use the default state
    e2 = grunk.create_env()
    e2.eval("z = 5.6")

    x = e1["x"]
    y = e1["y"]
    assert pytest.approx(x.as_float()) == 1.2
    assert pytest.approx(y.as_float()) == 3.4

    with pytest.raises(RuntimeError):
        z = e1["z"]
    
    z = e2["z"]
    assert pytest.approx(z.as_float()) == 5.6

    with pytest.raises(RuntimeError):
        x = e2["x"]
    with pytest.raises(RuntimeError):
        y = e2["y"]


def test_objects_python():

    x = grunk.create_object(1.2)
    y = grunk.create_object(3.4)
    z = x + y
    assert pytest.approx(z.as_float()) == 4.6

def test_operators_python():

    env = grunk.create_env()
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
    
    env = grunk.create_env()
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

    fx = grunk.feature(1.2)
    fy = grunk.feature(3.4)
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
    
    e = grunk.create_parametric_env()
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


def test_recipe_simple():
    
    x = grunk.feature(1.).with_id("x")
    y = grunk.feature(2.).with_id("y")
    z = (x+y).with_id("z")

    recipe = grunk.create_recipe()
    # recipe["w"] = grunk.pow(z, 2).with_id("w")  #TODO: Unfortunately, the implicit conversion to feature does not work from python yet
    recipe["w"] = grunk.pow(z, grunk.feature(2)).with_id("w")

    res = recipe.to_string()
    expected = f"""
uses:
  grunk: {grunk.__version__}
parameters:
  x: 1.0
  y: 2.0
steps: |
  z = x + y
  w = z ^ 2
"""
    
    assert "\n" + res == expected

    grunk.write("test.grr.yml", recipe)

    recipe2 = grunk.read("test.grr.yml")
    w2 = recipe2.get_feature("w")
    z2 = recipe2.get_feature("z")
    y2 = recipe2.get_feature("y")
    x2 = recipe2.get_feature("x")

    assert pytest.approx(w2.value().as_float()) == 9.0
    assert pytest.approx(z2.value().as_float()) == 3.0
    assert pytest.approx(y2.value().as_float()) == 2.0
    assert pytest.approx(x2.value().as_float()) == 1.0


def test_recipe_outputs():

    x = grunk.feature(1.).with_id("x")
    y = grunk.feature(2.).with_id("y")
    w = (x+y).with_id("w")

    recipe = grunk.create_recipe()
    recipe["w"] = w
    recipe.insert_output("result", "w")

    res = recipe.to_string()
    expected = f"""
uses:
  grunk: {grunk.__version__}
parameters:
  x: 1.0
  y: 2.0
steps: |
  w = x + y
outputs:
  result: w
"""

    assert "\n" + res == expected
    assert recipe.outputs == {"result": "w"}

    grunk.write("test.grr.yml", recipe)

    recipe2 = grunk.read("test.grr.yml")
    assert recipe2.outputs == {"result": "w"}
    assert pytest.approx(recipe2.get_output("result").value().as_float()) == 3.0


def test_recipe_clone():

    x = grunk.feature(12.3).with_id("x")
    y = grunk.feature(29.7).with_id("y")
    z = (x + y).with_id("z")

    recipe = grunk.create_recipe()
    recipe["x"] = x
    recipe["y"] = y
    recipe["z"] = z

    clone = recipe.clone()

    # # I expect no rounding errors when copying floating point numbers
    assert clone.get_feature("x").value().as_float() == 12.3
    assert clone.get_feature("y").value().as_float() == 29.7
    assert pytest.approx(clone.get_feature("z").value().as_float()) == 42.0

    x.set_value(13.2)
    assert not z.is_valid()
    assert pytest.approx(z.value().as_float()) == 42.9

    # cloned recipe should be unaffected by changes to the original recipe
    assert clone.get_feature("x").value().as_float() == 12.3
    assert clone.get_feature("y").value().as_float() == 29.7
    assert pytest.approx(clone.get_feature("z").value().as_float()) == 42.0


def test_recipe_call():

    recipe_inner = grunk.create_recipe()
    x = grunk.feature(17.)
    y = grunk.feature(13.)
    z = grunk.feature(2)
    w = (x + y) * z 
    recipe_inner["x"] = x
    recipe_inner["y"] = y
    recipe_inner["z"] = z
    recipe_inner["w"] = w
    recipe_inner.tag()

    recipe_outer = grunk.create_recipe()
    a = grunk.feature(15.)
    b = grunk.feature(11.)
    recipe_outer["a"] = a
    recipe_outer["b"] = b
    recipe_outer.insert_recipe("inner", recipe_inner)

    inner = recipe_outer.recipes["inner"]()
    inner["x"] = a
    inner["y"] = b
    c = inner.get("w")
    recipe_outer["c"] = c
    recipe_outer.tag()

    # c = (a + b) * inner_z = (15 + 11) * 2 = 52
    assert pytest.approx(recipe_outer.get_feature("c").value().as_float()) == 52

    # inner recipe should be unaffected (due to deep-copy)
    inner_recipe = recipe_outer.get_recipe("inner").change_value()
    assert inner_recipe.get_feature("x").value().as_float() == 17
    assert inner_recipe.get_feature("y").value().as_float() == 13
    assert inner_recipe.get_feature("z").value().as_float() == 2
    assert pytest.approx(inner_recipe.get_feature("w").value().as_float()) == 60

    # changing inner recipe should invalidate c
    inner_recipe.get_feature("z").set_value(0.5)
    assert not c.is_valid()
    assert pytest.approx(c.value().as_float()) == 13.0 

    # inner recipe should be unaffected (due to deep-copy)
    assert inner_recipe.get_feature("x").value().as_float() == 17
    assert inner_recipe.get_feature("y").value().as_float() == 13
    assert inner_recipe.get_feature("z").value().as_float() == 0.5
    assert pytest.approx(inner_recipe.get_feature("w").value().as_float()) == 15

 
def test_placeholder():

    x = grunk.feature().with_id("x")  # This is a placeholder: a feature with a name but no value
    y = grunk.feature(5).with_id("y")
    z = (x + y).with_id("z")

    recipe_inner = grunk.create_recipe()
    recipe_inner["x"] = x
    recipe_inner["y"] = y
    recipe_inner["z"] = z

    recipe_outer = grunk.create_recipe()
    recipe_outer.insert_recipe("inner", recipe_inner)

    recipe_outer.eval("""
        a = grunk.feature(2):with_id("a")
        b = grunk.feature(11):with_id("b")
        inner = recipes.inner()
        inner.x = a
        inner.y = b
        c = inner.z
        c:set_id("c")
    """)

    assert pytest.approx(recipe_outer.get_feature("c").value().as_float()) == 13


def test_run_module_script_and_clear():

    grunk.run_module_script("pymod", """
function inc(x)
    return x + 1
end

function lt(a,b)
    return a < b
end
""")

    # a plain environment sees plain, uninstrumented functions
    e = grunk.create_env()
    e.eval("a = 2\nb = pymod.inc(a)")
    assert e["b"].as_int() == 3

    # a parametric environment sees the functions decorated as actions
    pe = grunk.create_parametric_env()
    pe.eval("a = grunk.feature(4)\nb = pymod.inc(a)")
    fb = pe.get_feature("b")
    assert fb.value().as_int() == 5
    fa = pe.get_feature("a")
    fa.set_value(5)
    assert fb.value().as_int() == 6

    # clearing removes it from both environments
    grunk.clear_module("pymod")
    pe2 = grunk.create_parametric_env()
    with pytest.raises(RuntimeError):
        pe2.eval("a = pymod.inc(1)")


def test_run_module_file(tmp_path):

    lua_file = tmp_path / "mymod.lua"
    lua_file.write_text("""
        function add(x) return x + 2 end
        function smaller(a,b) return a < b end
    """)

    grunk.run_module_file("filemod", lua_file.as_posix())

    e = grunk.create_env()
    e.eval("x = filemod.add(3)")
    assert e["x"].as_int() == 5

    pe = grunk.create_parametric_env()
    pe.eval("x = grunk.feature(7)\ny = filemod.add(x)")
    fy = pe.get_feature("y")
    assert fy.value().as_int() == 9
    fx = pe.get_feature("x")
    fx.set_value(9)
    assert fy.value().as_int() == 11


def test_recipe_module_reactive_invalidation():

    # use a local state so this module doesn't leak into other tests
    grnk = grunk.state()

    recipe = grnk.create_recipe()
    x = grnk.feature(1.0).with_id("x")
    recipe["x"] = x

    recipe.insert_module_script("mymod", "function inc(a) return a + 1 end")
    recipe.eval("y = mymod.inc(x)")

    y = recipe.get_feature("y")
    assert pytest.approx(y.value().as_float()) == 2.0

    # editing the parameter invalidates the module call
    x.set_value(10.0)
    assert not y.is_valid()
    assert pytest.approx(y.value().as_float()) == 11.0

    # editing the module's source code also invalidates every call made into it
    recipe.module_scripts["mymod"].set_value("function inc(a) return a + 100 end")
    assert not y.is_valid()
    assert pytest.approx(y.value().as_float()) == 110.0
    