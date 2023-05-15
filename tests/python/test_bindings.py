import grunk
import pytest

def test_integration_hello_world():
    grunk.load("PluginA", install_missing=True)
    assert grunk.get_plugin_registry().count() == 1

    a = grunk.Feature("a", "PluginA::Scalar", 3.3)
    b = grunk.Feature("b", "PluginA::Scalar", 2.2)
    c = grunk.action("c", "PluginA::add", a, b).output()
    assert 5.5 == pytest.approx(c.value().get("value").as_float())
    grunk.write("test.grr", c)

    d = grunk.Feature("d", "PluginA::Scalar", 1.1)
    e = grunk.action("e", "PluginA::add", d, grunk.read("test.grr")["c"]).output()
    assert 6.6 == pytest.approx(e.value().get("value").as_float())