import os
from grunk.codegen import parse_headers, FunctionDecl, Module, generate, HeaderPath, CodeGenerator
import clang.cindex
import pytest
import yaml

def data_dir():
    wd = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(wd, os.pardir, "test_data")
    

@pytest.fixture(scope="session")
def parse_Foo_and_Included():

    include_dir = data_dir()

    return parse_headers(
        [HeaderPath(include_dir, "Foo.hxx"), HeaderPath(include_dir, "Included.hxx")],
        [
            include_dir,
        ],
    )


@pytest.fixture(scope="session")
def parse_Foo():

    include_dir = data_dir()

    return parse_headers(
        [
            HeaderPath(include_dir, "Foo.hxx"),
        ],
        [
            include_dir,
        ],
    )


def test_parse_multiple_headers(parse_Foo_and_Included):

    classes, functions = parse_Foo_and_Included

    assert len(classes) == 5
    assert classes[0].name == "Other"
    assert classes[1].name == "ForwardDeclared"
    assert classes[2].name == "Baz"
    assert classes[3].name == "Bar"
    assert classes[4].name == "Foo"

    assert len(functions) == 2
    assert functions[0].name == "some_function"
    assert functions[0].is_overloaded
    assert functions[1].name == "some_function"
    assert functions[1].is_overloaded


def test_parse_single_header(parse_Foo):

    classes, functions = parse_Foo

    # forward declared classes and structs should be skipped
    assert "ForwardDeclared" not in classes
    assert "Other" not in classes  # from Included.hxx

    # There should be two class/struct and one function declaration
    assert len(classes) == 3
    assert len(functions) == 1

    #######
    # Baz #
    #######

    baz = classes[0]
    assert baz.name == "Baz"
    assert baz.fully_qualified_name == "Baz"
    assert not baz.bases
    assert not baz.constructors
    assert not baz.fields
    assert len(baz.methods) == 1
    assert baz.methods[0].name == "operator new"
    assert baz.methods[0].fully_qualified_name == "Baz::operator new"
    assert "void * (*)(unsigned long" in baz.methods[0].function_pointer_type # some clang versins make size_t unsigned long, others unsigned long long
    assert not baz.methods[0].is_const
    assert baz.methods[0].is_static
    assert not baz.methods[0].is_overloaded
    assert not baz.conversions

    #######
    # Bar #
    #######

    bar = classes[1]
    assert bar.name == "Bar"
    assert bar.fully_qualified_name == "ns2::Bar"
    assert not bar.bases
    assert not bar.conversions

    assert len(bar.constructors) == 1
    assert not bar.constructors[0].arguments

    assert len(bar.fields) == 2
    assert "x" in bar.fields
    assert bar.fields["x"]["type"] == "int"
    assert bar.fields["x"]["pointer"] == "&ns2::Bar::x"
    assert "y" in bar.fields
    assert bar.fields["y"]["type"] == "double"
    assert bar.fields["y"]["pointer"] == "&ns2::Bar::y"

    assert len(bar.methods) == 1
    assert bar.methods[0].name == "bar_fun"
    assert bar.methods[0].fully_qualified_name == "ns2::Bar::bar_fun"
    assert not bar.methods[0].is_static
    assert not bar.methods[0].is_overloaded
    assert not bar.methods[0].is_const
    assert bar.methods[0].return_type == "void"
    assert not bar.methods[0].arguments

    #######
    # Foo #
    #######

    foo = classes[2]

    assert foo.name == "Foo"
    assert foo.fully_qualified_name == "ns2::Foo"

    # Bases

    assert len(foo.bases) == 1
    assert "ns2::Bar" in foo.bases

    # Conversions

    assert len(foo.conversions) == 1
    assert foo.conversions[0] == "ns1::Other"

    # Constructors

    assert len(foo.constructors) == 3
    assert len(foo.constructors[0].arguments) == 0  # Foo()
    assert (
        len(foo.constructors[1].arguments) == 3
    )  # Foo(Standard_Real, Standard_Real, Standard_Real)
    assert foo.constructors[1].arguments[0] == "double"
    assert foo.constructors[1].arguments[1] == "double"
    assert foo.constructors[1].arguments[2] == "double"
    assert len(foo.constructors[2].arguments) == 1  # Foo(bool)
    assert foo.constructors[2].arguments[0] == "bool"

    # Fields

    assert len(foo.fields) == 1  # only public data member
    assert "data_member" in foo.fields
    assert foo.fields["data_member"]["type"] == "std::vector<ForwardDeclared>"
    assert foo.fields["data_member"]["pointer"] == "&ns2::Foo::data_member"

    # Methods

    assert len(foo.methods) == 3  # baz and static_func

    baz0 = foo.methods[0]
    assert baz0.name == "baz"
    assert baz0.fully_qualified_name == "ns2::Foo::baz"
    assert len(baz0.arguments) == 1
    assert not baz0.is_static
    assert baz0.is_const
    assert baz0.is_overloaded
    assert baz0.arguments[0] == "int"
    assert baz0.return_type == "double"

    baz1 = foo.methods[1]
    assert baz1.name == "baz"
    assert baz1.fully_qualified_name == "ns2::Foo::baz"
    assert len(baz1.arguments) == 1
    assert not baz1.is_static
    assert baz1.is_const
    assert baz1.is_overloaded
    assert baz1.arguments[0] == "double"
    assert baz1.return_type == "double"

    static_func = foo.methods[2]
    assert static_func.name == "static_func"
    assert static_func.fully_qualified_name == "ns2::Foo::static_func"
    assert static_func.is_static
    assert not static_func.is_const
    assert not static_func.is_overloaded
    assert len(static_func.arguments) == 1
    assert static_func.arguments[0] == "const std::basic_string<char> &"
    assert static_func.return_type == "void"

    #################
    # some_function #
    #################

    sf = functions[0]
    assert sf.name == "some_function"
    assert sf.fully_qualified_name == "ns2::some_function"
    assert not sf.is_overloaded
    assert not sf.is_static
    assert len(sf.arguments) == 2
    assert sf.arguments[0] == "const ForwardDeclared &"
    assert sf.arguments[1] == "ns2::Bar *"
    assert sf.return_type == "ns1::Other"


def test_codegen_classes_none(parse_Foo):

    classes, functions = parse_Foo
    c = CodeGenerator()

    assert classes[0].name == "Baz"
    baz_cpp_code = c.cpp_register_type(classes[0])
    assert 'register_type<Baz>("Baz")' in baz_cpp_code
    assert '.add_member_function<void * (*)(unsigned long' in baz_cpp_code # size_t is sometimes unsigned long, sometimes unsinged long long
    assert '(&Baz::operator new, "operator new");' in baz_cpp_code

    assert classes[1].name == "Bar"
    bar_cpp_code = c.cpp_register_type(classes[1])
    assert (
        bar_cpp_code
        == """register_type<ns2::Bar>("Bar")
.add_constructor<>()
.add_data_member(&ns2::Bar::x, "x")
.add_data_member(&ns2::Bar::y, "y")
.add_member_function<void (ns2::Bar::*)()>(&ns2::Bar::bar_fun, "bar_fun");
"""
    )

    assert classes[2].name == "Foo"
    foo_cpp_code = c.cpp_register_type(classes[2])
    assert (
        foo_cpp_code
        == """register_type<ns2::Foo>("Foo")
.add_base<ns2::Bar>()
.add_constructor<>()
.add_constructor<double, double, double>()
.add_constructor<bool>()
.add_conversion<ns1::Other>()
.add_data_member(&ns2::Foo::data_member, "data_member")
.add_member_function<double (ns2::Foo::*)(int) const>(&ns2::Foo::baz, "baz")
.add_member_function<double (ns2::Foo::*)(double) const>(&ns2::Foo::baz, "baz")
.add_member_function<void (*)(const std::basic_string<char> &)>(&ns2::Foo::static_func, "static_func");
"""
    )

    c = CodeGenerator()

    assert functions[0].name == "some_function"
    some_function_cpp_code = c.cpp_register_function(functions[0])
    assert (
        some_function_cpp_code
        == 'register_function<ns1::Other (*)(const ForwardDeclared &, ns2::Bar *)>(&ns2::some_function, "some_function");\n'
    )


def test_codegen_classes_fully_qualified_names(parse_Foo):

    classes, functions = parse_Foo
    c = CodeGenerator()
    c.fully_qualified_names = True

    assert classes[1].name == "Bar"
    bar_cpp_code = c.cpp_register_type(classes[1])
    assert bar_cpp_code.startswith('register_type<ns2::Bar>("ns2::Bar")\n')

    assert functions[0].name == "some_function"
    some_function_cpp_code = c.cpp_register_function(functions[0])
    assert (
        some_function_cpp_code
        == 'register_function<ns1::Other (*)(const ForwardDeclared &, ns2::Bar *)>(&ns2::some_function, "ns2::some_function");\n'
    )


def test_codegen_classes_prefix(parse_Foo):

    classes, functions = parse_Foo
    c = CodeGenerator()
    c.prefix = "schurz"

    assert classes[1].name == "Bar"
    bar_cpp_code = c.cpp_register_type(classes[1])
    assert bar_cpp_code.startswith('register_type<ns2::Bar>("schurz::Bar")\n')

    assert functions[0].name == "some_function"
    some_function_cpp_code = c.cpp_register_function(functions[0])
    assert (
        some_function_cpp_code
        == 'register_function<ns1::Other (*)(const ForwardDeclared &, ns2::Bar *)>(&ns2::some_function, "schurz::some_function");\n'
    )


def test_codegen_classes_prefix_fully_qualified_names(parse_Foo):

    classes, functions = parse_Foo
    c = CodeGenerator()
    c.prefix = "schurz"
    c.fully_qualified_names=True

    assert classes[1].name == "Bar"
    bar_cpp_code = c.cpp_register_type(classes[1])
    assert bar_cpp_code.startswith('register_type<ns2::Bar>("schurz::ns2::Bar")\n')

    assert functions[0].name == "some_function"
    some_function_cpp_code = c.cpp_register_function(functions[0])
    assert (
        some_function_cpp_code
        == 'register_function<ns1::Other (*)(const ForwardDeclared &, ns2::Bar *)>(&ns2::some_function, "schurz::ns2::some_function");\n'
    )


def test_no_whitelist_no_blacklist():
    config_file = os.path.join(data_dir(), "config.yml")
    include_dirs = [
        data_dir(),
    ]
    with open(config_file, "r") as file:
        config = yaml.safe_load(file)
        module = Module(config, include_dirs)
        headers = module.get_all_headers()
        classes, functions = parse_headers(headers, include_dirs)
        module.grab_declarations(classes + functions)
        assert len(module.class_declarations) == 3
        assert len(module.function_declarations) == 1
        assert len(module.modules[0].class_declarations) == 2
        assert len(module.modules[0].function_declarations) == 1


def test_whitelist():
    config_file = os.path.join(data_dir(), "config_whitelist.yml")
    include_dirs = [
        data_dir(),
    ]
    with open(config_file, "r") as file:
        config = yaml.safe_load(file)
        module = Module(config, include_dirs)
        headers = module.get_all_headers()
        classes, functions = parse_headers(headers, include_dirs)
        module.grab_declarations(classes + functions)
        assert len(module.class_declarations) == 2
        assert "Bar" not in map(lambda x: x.name, module.class_declarations)
        assert len(module.function_declarations) == 0
        assert len(module.modules[0].class_declarations) == 1
        assert "ForwardDeclared" not in map(
            lambda x: x.name, module.modules[0].class_declarations
        )
        assert len(module.modules[0].function_declarations) == 1


def test_blackist_fields_and_methods():
    config_file = os.path.join(
        data_dir(), "config_blackist_fields_and_methods.yml"
    )
    include_dirs = [
        data_dir(),
    ]
    with open(config_file, "r") as file:
        config = yaml.safe_load(file)
        module = Module(config, include_dirs)
        headers = module.get_all_headers()
        classes, functions = parse_headers(headers, include_dirs)
        module.grab_declarations(classes + functions)
        assert len(module.class_declarations) == 3
        assert len(module.function_declarations) == 1
        bardecl = module.class_declarations[1]
        assert "x" not in bardecl.fields
        assert len(bardecl.methods) == 0


def test_blacklist():
    config_file = os.path.join(data_dir(), "config_blacklist.yml")
    include_dirs = [
        data_dir(),
    ]
    with open(config_file, "r") as file:
        config = yaml.safe_load(file)
        module = Module(config, include_dirs)
        headers = module.get_all_headers()
        classes, functions = parse_headers(headers, include_dirs)
        module.grab_declarations(classes + functions)
        assert len(module.class_declarations) == 1
        assert "Bar" in map(lambda x: x.name, module.class_declarations)
        assert len(module.function_declarations) == 1
        assert "some_function" in map(lambda x: x.name, module.function_declarations)
        assert len(module.modules[0].class_declarations) == 1
        assert "ForwardDeclared" in map(
            lambda x: x.name, module.modules[0].class_declarations
        )
        assert len(module.modules[0].function_declarations) == 0


def test_whitelist_blacklist():
    config_file = os.path.join(data_dir(), "config_whitelist_blacklist.yml")
    include_dirs = [
        data_dir(),
    ]
    with open(config_file, "r") as file:
        config = yaml.safe_load(file)
        module = Module(config, include_dirs)
        headers = module.get_all_headers()
        classes, functions = parse_headers(headers, include_dirs)
        module.grab_declarations(classes + functions)
        assert len(module.class_declarations) == 1
        assert "Foo" in map(lambda x: x.name, module.class_declarations)
        assert len(module.function_declarations) == 0


def test_generate():
    wd = os.path.dirname(os.path.abspath(__file__))
    config = os.path.join(data_dir(), "config.yml")
    include_dir = data_dir()
    output_dir = os.path.join(wd, "test_output")
    generate(
        config,
        output_dir,
        [
            include_dir,
        ],
    )


def test_nested_class():

    include_dir = data_dir()

    classes, functions = parse_headers(
        [
            HeaderPath(include_dir, "nested_class.hxx"),
        ],
        [
            include_dir,
        ],
    )
    assert len(classes) == 2
    assert classes[0].fully_qualified_name == "Foo"
    assert classes[1].fully_qualified_name == "Foo::Bar"

    assert len(classes[0].nested_enums) == 2
    assert classes[0].nested_enums[0] == "Color"
    assert classes[0].nested_enums[1] == "Boolean"

    c = CodeGenerator()
    cpp_code = c.cpp_register_type(classes[0])
    assert (
        cpp_code
        == """register_type<Foo>("Foo")
.add_member_function<Foo::Bar (Foo::*)(Foo::Color)>(&Foo::baz, "baz");
"""
    )


def test_default_arguments():

    include_dir = data_dir()

    classes, functions = parse_headers(
        [
            HeaderPath(include_dir, "default_arguments.hxx"),
        ],
        [
            include_dir,
        ],
    )

    assert len(functions) == 1
    fun = functions[0]
    assert fun.num_default_args == 1


# TODO:
# - test generated code (smaller header, actually compile with clang)
# - test prefix variants
