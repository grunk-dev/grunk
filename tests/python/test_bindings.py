import grunk
import pytest

def test_integration_hello_world():
    grunk.load("PluginA", version="0.1.0", install_missing=True)
    assert grunk.get_plugin_registry().count() == 1

    a = grunk.Feature("a", "PluginA::Scalar", 3.3)
    b = grunk.Feature("b", "PluginA::Scalar", 2.2)
    c = grunk.action("c", "PluginA::add", a, b).output()
    assert 5.5 == pytest.approx(c.value().get("value").as_float())
    grunk.write("test.grr", c)

    grunk.load("PluginB", version="0.1.0", install_missing=True)
    assert grunk.get_plugin_registry().count() == 2
    
    d = grunk.Feature("d", "PluginA::Scalar", 2.)
    e = grunk.action("e", "PluginB::multiply", d, grunk.read("test.grr")["c"]).output()
    assert 11 == pytest.approx(e.value().get("value").as_float())
    grunk.write("test2.grr", e)

    a.access_value().set("value", 4.4)
    assert 6.6 == pytest.approx(c.value().get("value").as_float())

def test_expression():
    grunk.get_plugin_registry() # initializes standard plugin

    a = grunk.Feature("x", "double", 0.)
    b = grunk.Feature("y", "double", 0.75)
    c = grunk.expression("z", "2*cos(x)*y+1", a, b).output()
    assert 2.5 == pytest.approx(c.value().as_float())

