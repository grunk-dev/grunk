import grunk
import pytest

@pytest.fixture
def load_plugins():
    plugins = [
        {"name": "PluginA", "version": "0.1.1"},
        {"name": "PluginB", "version": "0.1.1"}
    ]
    for plugin in plugins:
        if not grunk.plugin_manager.is_installed(plugin["name"], plugin["version"]):
            with grunk.plugin_manager.PluginManager() as pm:
                pm.install(plugin["name"], plugin["version"])
        for dir in grunk.plugin_manager.get_dll_paths(plugin["name"], plugin["version"]):
            grunk.get_plugin_registry().prepend_path(dir)


def test_integration_hello_world(load_plugins, tmp_path):
    grunk.get_plugin_registry().load("PluginA")
    assert grunk.get_plugin_registry().count() == 1

    a = grunk.Feature("a", "PluginA::Scalar", 3.3)
    b = grunk.Feature("b", "PluginA::Scalar", 2.2)
    c = grunk.action("c", "PluginA::add", a, b).output()
    assert 5.5 == pytest.approx(c.value().get("value").as_float())
    grunk.write(str(tmp_path / "test.grr.yml"), c)

    grunk.get_plugin_registry().load("PluginB")
    assert grunk.get_plugin_registry().count() == 2
    
    d = grunk.Feature("d", "PluginA::Scalar", 2)
    e = grunk.action("e", "PluginB::multiply", d, grunk.read(str(tmp_path / "test.grr.yml"))["c"]).output()
    assert 11 == pytest.approx(e.value().get("value").as_float())
    grunk.write(str(tmp_path / "test2.grr.yml"), e)

    a.access_value().set("value", 4.4)
    assert 6.6 == pytest.approx(c.value().get("value").as_float())

def test_expression():
    grunk.init()

    a = grunk.Feature("x", "double", 0.)
    b = grunk.Feature("y", "double", 0.75)
    c = grunk.expression("z", "2*cos(x)*y+1", a, b)
    assert 2.5 == pytest.approx(c.value().as_float())

def test_recipe(load_plugins, tmp_path):
    grunk.get_plugin_registry().load("PluginA")
    grunk.get_plugin_registry().load("PluginB")
    
    recipe = grunk.Recipe()
    recipe.feature("a", "PluginA::Scalar", 17.)
    recipe.feature("b", "PluginA::Scalar", 15.)
    recipe.insert_feature(
        grunk.action("c", "PluginA::add", recipe["a"], recipe["b"]).output()
    )

    x = grunk.Feature("x", "PluginA::Scalar", 2.)
    y = grunk.Feature("y", "PluginA::Scalar", 5.)
    z = grunk.action("z", "PluginB::multiply", x, y).output()
    recipe.insert_recipe("multiply", grunk.Recipe(x,y,z))

    recipe.recipe("multiply", {"d": "z"}, {"x": recipe["c"]})

    assert (17. + 15.) == pytest.approx(recipe["c"].value().as_float())
    assert ( 2. *  5.) == pytest.approx(recipe.get_recipe("multiply")["z"].value().as_float())
    assert ( 5. * 32.) == pytest.approx(recipe["d"].value().as_float())

    # evaluation of d should not effect value of z in inner recipe
    assert ( 2. *  5.) == pytest.approx(recipe.get_recipe("multiply")["z"].value().as_float())

    # just make sure this doesn't fail:
    grunk.write(str(tmp_path / "test_nested_recipe.grr.yml"), recipe)


def test_script(load_plugins, tmp_path):
    grunk.get_plugin_registry().load("PluginA")
    grunk.get_plugin_registry().load("PluginB")

    x = grunk.Feature("x", "PluginA::Scalar", -0.25)
    y = grunk.Feature("y", "PluginA::Scalar", -0.75)
    p = grunk.script(
        steps=[
            grunk.ScriptStep(
                function_name="PluginA::add", 
                outputs=["z"], 
                inputs=[x, y]
            ),
            grunk.ScriptStep("PluginB::multiply", ["q"], ["z", y])
        ],
        returns=["q"]
    ).output()
    
    assert (0.75) == pytest.approx(p.value().as_float())

    # just make sure this doesn't fail:
    grunk.write(str(tmp_path / "script_action.grr.yml"), p)

def test_vec(load_plugins, tmp_path):
    grunk.get_plugin_registry().load("PluginA")
    grunk.get_plugin_registry().load("PluginB")

    x = grunk.Feature("x", "PluginA::Scalar", -0.25)
    y = grunk.Feature("y", "PluginA::Scalar", -0.75)
    v = grunk.vec("v", x, y)

    # just make sure this doesn't fail:
    grunk.write(str(tmp_path / "vec_action.grr.yml"), v)

def test_constant(load_plugins, tmp_path):
    grunk.get_plugin_registry().load("PluginA")
    grunk.get_plugin_registry().load("PluginB")

    a = grunk.Feature("a", "PluginA::Scalar", 3.3)
    c = grunk.action("c", "PluginA::add", a, 2.2).output()
    grunk.write(str(tmp_path / "constant.grr.yml"), c)