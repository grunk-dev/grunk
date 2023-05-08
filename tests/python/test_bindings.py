import grunk
import pytest

def test_plugin_registry():
    #TODO: need to copy plugin here
    gpr = grunk.get_plugin_registry()
    gpr.prepend_path(".")
    gpr.load_all()
    assert gpr.count() == 1

    a = grunk.Feature("a", "SimplePlugin::MyDouble", 3.3)
    b = grunk.Feature("b", "SimplePlugin::MyDouble", 2.2)
    c = grunk.action("c", "SimplePlugin::add", a, b).output()
    assert 5.5 == pytest.approx(c.value().get("value").as_float())
    grunk.write("test.grr", c)