import grunk

def test_plugin_registry():
    # need to copy plugin here
    gpr = grunk.get_plugin_registry()
    gpr.prepend_path(".")
    gpr.load_all()
    assert gpr.count() == 1

    a = grunk.Feature("a", "SimplePlugin::MyDouble", 3.3)
    b = grunk.Feature("a", "SimplePlugin::MyDouble", 2.2)
    print("fine")
    print(b.value())
    c = grunk.action("c", "SimplePlugin::add", a, b).output() 
    print(c.id)
    print(c.value()) # segfaults here
    grunk.write("test.grr", c)
    print("not fine")