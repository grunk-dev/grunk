from abc import ABC, abstractmethod
import clang.cindex
from enum import Enum
import glob
import importlib.util
import inspect
import itertools
from pathlib import Path
import shutil
import subprocess
import typing
import tempfile
import textwrap
import warnings
import os
import yaml


def has_clang():
    return shutil.which("clang++") is not None 

def check_clang():
    if not has_clang():
            raise RuntimeError("clang++ not found. Clang is needed by grunk's code generator.")


class CodeGenerator(ABC):
    """This class generates the C++ code for registering functions and classes
    """

    prefix = ""
    fully_qualified_names = False
    template_args = True

    def cpp_type_register_type(self, decl):
        return (
            "register_type<"
            + decl.node.type.spelling
            + '>("'
            + decl.registered_name(
                self.prefix, self.fully_qualified_names
            )
            + '")'
        )


    def cpp_type_register_bases(self, decl):
        s = ""
        for b in decl.bases:
            s = s + "\n.add_base<" + b + ">()"
        return s


    def cpp_type_register_constructors(self, decl):
        s = ""
        for c in decl.constructors:
            nargs = len(c.arguments)
            # for every default argument, add an overload ommitting the argument and all following ones
            for idx_last_arg in range(nargs - c.num_default_args - 1, nargs):
                s = s + "\n.add_constructor<"
                for arg in c.arguments[: idx_last_arg + 1]:
                    s = s + arg + ", "
                if idx_last_arg >= 0:
                    # strip last comma
                    s = s[0:-2]
                s = s + ">()"
        return s


    def cpp_type_register_conversions(self, decl):
        s = ""
        for c in decl.conversions:
            s = s + "\n.add_conversion<" + c + ">()"
        return s


    def cpp_type_register_fields(self, decl):
        s = ""
        for [name, field] in decl.fields.items():
            s = s + "\n.add_data_member(" + field["pointer"] + ', "' + name + '")'
        return s


    def cpp_type_register_methods(self, decl):
        s = ""
        for method in decl.methods:
            s = s + "\n." + self.cpp_register_function(method)
        return s


    def cpp_register_type(self, decl):
        return (
            self.cpp_type_register_type(decl)
            + self.cpp_type_register_bases(decl)
            + self.cpp_type_register_constructors(decl)
            + self.cpp_type_register_conversions(decl)
            + self.cpp_type_register_fields(decl)
            + self.cpp_type_register_methods(decl)
            + ";\n"
        )


    def cpp_register_function(self, decl):
        if decl.parent:
            pre = "add_member_function"
            regname = decl.name
        else:
            pre = "register_function"
            regname = decl.registered_name(
                self.prefix, self.fully_qualified_names
            )
        if decl.is_overloaded or self.template_args:
            # add template parameters so that compiler can resolve overload
            prefix = pre + "<" + decl.function_pointer_type + ">("
        else:
            prefix = "("
        post = ', "' + regname + '")'
        if not decl.parent:
            post = post + ";\n"

        s = prefix + decl.function_pointer + post

        # add overload for every default argument
        nargs = len(decl.arguments)
        for idx_last_arg in range(nargs - decl.num_default_args, nargs):
            if decl.parent:
                s = s + "\n."
            s = s + pre + "([]("
            if decl.parent and not decl.is_static:
                # add implicit first argument
                s = s + decl.parent.type.spelling
                if decl.is_const:
                    s = s + " const"
                s = s + "& x, "
            for i in range(0, idx_last_arg):
                s = s + decl.arguments[i] + " arg" + str(i) + ", "
            if s[-2:] == ", ":
                # strip last comma
                s = s[0:-2]
            s = s + "){ return "
            if decl.parent and not decl.is_static:
                s = s + "x."
            # if decl.parent and decl.is_static:
                #TODO: why is this here?
                # s = s + decl.parent.type.spelling + "::"
            if decl.parent:
                s = s + decl.parent.type.spelling + "::" + decl.name
            else:
                s = s + decl.fully_qualified_name
            s = s + "("
            for i in range(0, idx_last_arg):
                s = s + "arg" + str(i) + ", "
            if s[-2:] == ", ":
                # strip last comma
                s = s[0:-2]
            s = s + "); }" + post
        return s


class Decl(ABC):
    def __init__(self, node):
        self.node = node
        self.name = node.spelling
        self.fully_qualified_name = get_decl_fqn(node)

    def registered_name(
        self, prefix="", fully_qualified_names=False, template_args=False
    ):
        name = self.name
        if fully_qualified_names:
            name = self.fully_qualified_name
        if prefix:
            name = prefix + "::" + name

        # mangle name with template arguments
        # TODO: Make this customizable somehow?
        postfix = ""
        for i in range(0, self.node.type.get_num_template_arguments()):
            postfix = (
                postfix + "_" + self.node.type.get_template_argument_type(i).spelling
            )
        name = name + postfix

        return name

    def header(self):
        if self.node.location.file:
            return self.node.location.file.name
        else:
            return None


class Callable(ABC):
    """A structured representation of a callable object that has arguments"""

    def __init__(self, node: clang.cindex.Cursor):

        self.return_type = type_str(node.type.get_result())

        self.num_default_args = 0
        self.arguments = []
        for arg in node.get_arguments():
            self.arguments.append(type_str(arg.type))

            if "=" in [token.spelling for token in arg.get_tokens()]:
                self.num_default_args = self.num_default_args + 1


class FunctionDecl(Decl, Callable):
    """A structured representation of a function declaration parsed
    using clang. It stores strings representing the name of the function,
    the arguments and return type etc.
    """

    def __init__(self, node: clang.cindex.Cursor, parent=None):
        Decl.__init__(self, node)
        Callable.__init__(self, node)

        self.parent = parent

        self.is_static = self.node.is_static_method()
        self.is_const = self.node.is_const_method()
        self.is_overloaded = False

        # store function pointer as string
        self.function_pointer = ""
        if self.parent:
            self.function_pointer = "&" + self.parent.type.spelling + "::" + self.name
        else:
            self.function_pointer = "&" + self.fully_qualified_name

        # store type of function pointer as string
        self.function_pointer_type = self.get_function_pointer_type()

        self.is_variadic = self.node.type.is_function_variadic()

    def get_function_pointer_type(self):
        """constructs the function pointer type"""
        function_pointer_type = self.return_type
        if self.parent and not self.is_static:
            function_pointer_type = (
                function_pointer_type + " (" + self.parent.type.spelling + "::*)("
            )
        else:
            function_pointer_type = function_pointer_type + " (*)("

        for arg in self.arguments:
            function_pointer_type = function_pointer_type + arg + ", "
        if self.arguments:
            function_pointer_type = function_pointer_type[0:-2]
        function_pointer_type = function_pointer_type + ")"

        if self.is_const:
            function_pointer_type = function_pointer_type + " const"

        return function_pointer_type
            


class ClassDecl(Decl):
    """A structured representation of a class or struct declaration parsed
    using clang. It stores string representations of the name, base classes,
    data members, member functions, conversion operators and constructors.
    """

    def __init__(
        self,
        node: clang.cindex.Cursor,
    ):
        super().__init__(node)

        self.bases = []

        self.fields = {}
        self.methods = []

        self.constructors = []
        self.conversions = []

        self.nested_classes = []
        self.nested_enums = []

        def is_base(n):
            return n.kind == clang.cindex.CursorKind.CXX_BASE_SPECIFIER

        def is_ctor(n):
            return n.kind == clang.cindex.CursorKind.CONSTRUCTOR

        def is_field(n):
            return n.kind == clang.cindex.CursorKind.FIELD_DECL

        def is_method(n):
            return n.kind == clang.cindex.CursorKind.CXX_METHOD

        def is_conversion(n):
            return n.kind == clang.cindex.CursorKind.CONVERSION_FUNCTION

        def is_direct_child(n):
            return n.semantic_parent == self.node

        def pred(n):
            return (
                is_base(n)
                or is_ctor(n)
                or is_field(n)
                or is_method(n)
                or is_conversion(n)
                or is_class(n)
                or is_enum(n)
            )

        for node in filter_node_list_by_predicate(self.node.get_children(), pred):
            if is_base(node) and is_public(node):
                self.bases.append(get_decl_fqn(node.referenced))
            elif (
                is_ctor(node)
                and is_public(node)
                and is_available(node)
                and is_direct_child(node)
                and not node.is_copy_constructor()
                and not node.is_move_constructor()
            ):
                self.constructors.append(Callable(node))
            elif is_field(node) and is_public(node) and is_direct_child(node):
                field = {}
                field["type"] = node.type.get_canonical().spelling
                field["pointer"] = "&" + get_decl_fqn(node)
                self.fields[node.spelling] = field
            elif is_method(node) and is_public(node) and is_direct_child(node):
                fd = FunctionDecl(node, parent=self.node)

                overload = next((f for f in self.methods if f.name == fd.name), None)
                if overload:
                    overload.is_overloaded = True
                    fd.is_overloaded = True

                # variadic functions are currently not supported:
                # We need to know the exact number of arguments (for now...)
                if not fd.is_variadic:
                    self.methods.append(fd)
            elif is_conversion(node) and is_public(node):
                self.conversions.append(node.type.get_result().spelling)
            elif is_class(node) and is_public(node) and is_direct_child(node):
                self.nested_classes.append(node.spelling)
            elif is_enum(node) and is_public(node) and is_direct_child(node):
                self.nested_enums.append(node.spelling)


def filter_node_list_by_predicate(
    nodes: typing.Iterable[clang.cindex.Cursor], predicate: typing.Callable
) -> typing.Iterable[clang.cindex.Cursor]:
    """filters a clang node list by cursor predicate

    :param nodes: a list of clang cursors
    :type nodes: typing.Iterable[clang.cindex.Cursor]
    :param predicate: a list of clang cursor types
    :type predicate: a callable that accepts a node and returns bool
    :return: a list of clang cursors
    :rtype: typing.Iterable[clang.cindex.Cursor]
    """
    for i in nodes:
        if predicate(i):
            yield i
        yield from filter_node_list_by_predicate(i.get_children(), predicate)


def is_forward_declaration(node):
    """returns true if the node is a forward declaration, even if the definitioin
    is in the same translation unit (e.g. via included files)

    :param node: clang cursor
    :type node: clang cursor
    :return: True, if the node is a forward definition
    :rtype: bool
    """
    if not node.get_definition():
        return True

    return node != node.get_definition()


def is_public(node):
    """returns True, if the clang cursor has a "public" access specifier.
    This applies specifically to declarations within the body of a class
    declaration.

    :param node: A clang cursor
    :type node: clang cursour
    :return: True, if the clang cursor has a "public" access specifier
    :rtype: bool
    """
    return node.access_specifier == clang.cindex.AccessSpecifier.PUBLIC


def is_available(node):
    """returns True, if the clang cursor is available. For example, deleted
    constructors or operators are not avaialbe

    :param node: A clang cursor
    :type node: clang cursour
    :return: True, if a declaration is available
    :rtype: bool
    """
    return node.availability == clang.cindex.AvailabilityKind.AVAILABLE


def is_class(n):
    return n.kind in [
        clang.cindex.CursorKind.CLASS_DECL,
        clang.cindex.CursorKind.STRUCT_DECL,
    ]


def is_enum(n):
    return n.kind in [
        clang.cindex.CursorKind.ENUM_DECL,
        clang.cindex.CursorKind.ENUM_CONSTANT_DECL,
    ]


def is_func(n):
    return n.kind == clang.cindex.CursorKind.FUNCTION_DECL

# The following functions are taken from the accepted answer here:
# https://stackoverflow.com/questions/77941127/how-can-i-get-the-fully-qualified-names-of-return-types-and-argument-types-using
def get_decl_fqn(decl: clang.cindex.Cursor) -> str:
    """
    Given a Cursor that refers to a Declaration, get its fully
    qualified name.
    """

    # The semantic parent is the enclosing class, namespace, or
    # translation unit.
    parent = decl.semantic_parent
    assert(parent is not None)

    # When we hit the TU, just return the simple identifier.
    if parent.kind == clang.cindex.CursorKind.TRANSLATION_UNIT:
        return decl.spelling

    # Otherwise, print the parent name as a qualifier.
    else:
        # If the parent is a named type, use this as qualifier
        if parent.type is not None and parent.type.spelling:
            return parent.type.spelling + "::" + decl.spelling
        else:
            #Otherwise, recurse
            return get_decl_fqn(parent) + "::" + decl.spelling


def starts_with_letter(s: str) -> bool:
    """
    True if 's' starts with a letter.
    """

    return s != "" and s[0].isalpha()


def ends_with_letter(s: str) -> bool:
    """
    True if 's' ends with a letter.
    """

    return s != "" and s[-1].isalpha()


def join_type_strs(s1: str, s2: str) -> str:
    """
    Join two strings containing fragments of type syntax, inserting a
    space if both are non-empty and either has a letter adjacent to the
    joined edge.
    """

    needs_space = (ends_with_letter(s1) 
        or s1.endswith('>') 
        or starts_with_letter(s2) 
        or s2.startswith('&') 
        or s2.startswith('*'))
    if s1 != "" and s2 != "" and needs_space:
        return s1 + " " + s2
    else:
        return s1 + s2


def type_str(t: clang.cindex.Type) -> str:
    """
    Print 't' in C++ syntax, using fully qualified names for named
    types.  (In contrast, 't.spelling' omits qualifiers.)
    """

    return join_type_strs(before_type_str(t), after_type_str(t))


def before_type_str(t: clang.cindex.Type) -> str:
    """
    Print the part of 't' that would go before the declarator name in a
    declaration of a variable with that type.
    """

    return join_type_strs(cv_qualifiers_str(t), before_type_str_nq(t))


def cv_qualifiers_str(t: clang.cindex.Type) -> str:
    """
    If 't' has any const/volatile/restrict qualifiers, return a string
    containing them, separated by spaces.  Otherwise, return "".
    """

    qualifiers = []
    if t.is_const_qualified():
        qualifiers.append("const")
    if t.is_volatile_qualified():
        qualifiers.append("volatile")
    if t.is_restrict_qualified():
        qualifiers.append("restrict")

    return " ".join(qualifiers)


def get_template_arguments(t: clang.cindex.Type) -> typing.Iterable[str]:
    """returns a list of template arguments as a string. Keeps the non-type
    template arguments verbatim and replaces type template arguments with the 
    fully qualified type string
    """
    # extract template arguments from canoncial type using string methods

    template_arg_strs = []

    cleaned_str = t.get_named_type().spelling.replace(" ", "")

    # find outer-most bracket pair <> and extract substring between
    start = cleaned_str.find('<')+1
    if start > 0:
        end = cleaned_str.rfind('>')
        assert(end>0)
        cleaned_str = cleaned_str[start:end]

        nested_bracket_count = 0
        i = 0
        while i < len(cleaned_str):

            if cleaned_str[i] == '<':
                # we are parsing a template argument of a template argument. ignoring all commas
                # until we hit the corresponding closing bracket
                nested_bracket_count += 1
            elif cleaned_str[i] == '>':
                nested_bracket_count -= 1

            assert(nested_bracket_count >= 0)
            ignore_characters = (nested_bracket_count>0)

            # seperate arguments at comma
            if not ignore_characters and cleaned_str[i] == ',':
                template_arg_strs.append(cleaned_str[0:i].strip())
                cleaned_str = cleaned_str[i+1:]
                i = 0
            else:
                i = i+1

        # add last argument if any
        if cleaned_str:
            template_arg_strs.append(cleaned_str)

    # Replace all type template parameters with fully qualified names. Keep non-type
    # template parameters
    ntargs = t.get_num_template_arguments()
    if ntargs>0:
        # it is possible that the named type has no template arguments in its spelling, 
        # but the type actually has template arguments. This happens with type aliases 
        # and default template arguments for instance
        assert ntargs >= len(template_arg_strs)
    for i in range(0, len(template_arg_strs)):
        arg_type = t.get_template_argument_type(i)
        if arg_type.spelling:
            template_arg_strs[i] = type_str(arg_type)

    return template_arg_strs


def before_type_str_nq(t: clang.cindex.Type) -> str:
    """
    Print the part of 't' that would go before the declarator name in a
    declaration of a variable with that type, ignoring any CV
    qualifiers.
    """

    if t.kind == clang.cindex.TypeKind.ELABORATED:
        # Most named types are represented with the "elaborated" node,
        # which typically has a name.
        ret = get_decl_fqn(t.get_declaration())


        # Properly handle templates. If t is an alias for a template realization
        # we do not want to add the template parameters here. So we need to check 
        # this first. A clue for this is that the end of the  named type is the 
        # same and it contains no <> parenthesis
        targs = get_template_arguments(t)
        if not ret.endswith(t.get_named_type().spelling):
            ret += '<'
            for i in range(0, len(targs)):
                if i>0:
                    ret+= ', '
                ret += targs[i]
            ret += '>'
        
        return ret

    elif t.kind == clang.cindex.TypeKind.POINTER:
        p = t.get_pointee()

        # TODO: This does not handle pointer-to-function properly, since
        # that requires additional parentheses.
        return join_type_strs(before_type_str(p), "*")

    elif t.kind == clang.cindex.TypeKind.LVALUEREFERENCE:
        p = t.get_pointee()
        return join_type_strs(before_type_str(p), "&")

    elif t.kind == clang.cindex.TypeKind.RVALUEREFERENCE:
        p = t.get_pointee()
        return join_type_strs(before_type_str(p), "&&")

    elif t.kind == clang.cindex.TypeKind.FUNCTIONPROTO:
        rettype = t.get_result()
        return before_type_str(rettype)

    # TODO: FUNCTIONNOPROTO, pointer-to-member, and possibly others.

    else:
        # For other types, just use the spelling as its "before" syntax,
        # removing any specifiers
        ret = t.spelling
        ret = ret.replace('const', '')
        ret = ret.replace('volatile', '')
        ret = ret.replace('restrict', '')
        return ret.strip()


def after_type_str(t: clang.cindex.Type) -> str:
    """
    Print the part of 't' that would go after the declarator name in a
    declaration of a variable with that type.
    """

    if t.kind == clang.cindex.TypeKind.FUNCTIONPROTO:
        res = "("
        count = 0
        for argtype in t.argument_types():
            if count > 0:
                res += ", "
            count += 1
            res += type_str(argtype)
        res += ")"
        return res

    # TODO: FUNCTIONNOPROTO and the various array types.

    return ""


def get_system_include_directories():
    """gets a list of standard include paths used by clang.

    By default, the parse method does not search the standard include paths.
    This function creates an empty cpp file and queries clang for the include
    paths with "clang++ -E -x c++ -v empty_file.cpp".

    The returend include directories can be used in the arguments of parse

    :return: a list of clang's standard include directories
    :rtype: list(str)
    """

    check_clang()

    include_directories = []
    cpp = tempfile.NamedTemporaryFile(delete=False)
    try:
        result = subprocess.run(
            ["clang++", "-E", "-x", "c++", "-v", cpp.name], capture_output=True
        )
        stderr = result.stderr.decode("utf-8").split("\n")

        directory_section = False
        for line in stderr:
            # read all lines between the line starting with '#include' and the one starting with 'End of search list.'.
            # Ignore all lines that start with '#include'
            if line.startswith("#include") and not directory_section:
                directory_section = True
            if line.startswith("End of search list.") and directory_section:
                directory_section = False
            if "#include" not in line and directory_section:
                include_directories.append(line.strip())

        return include_directories

    finally:
        cpp.close()
        os.unlink(cpp.name)


def _get_absolute_path(file, directories):
    """given a path relative to one of the directories specified in the second argument,
    returns the full path

    :param file: a relative path
    :type file: str
    :param directories: list of directories
    :type directories: list of str
    """
    for dir in directories:
        candidate = os.path.join(dir, file)
        if os.path.isfile(candidate):
            return candidate
    raise IndexError(f"Could not find {file} in {directories}.")


def parse_headers(headers: typing.Iterable[str], include_dirs: typing.Iterable[str]):
    """parses header files for function, struct and class declarations. Returns two
    dictionaries, the first for the parsed class declarations and the second for the parsed
    function declarations.

    The keys of the dictionaries are the class/struct/function names and the values are
    of type ClassDecl, FunctionDecl respectively.

    Headers are not recursively parsed through #include statements, every header that
    shall be parsed for class and function declarations must be explicitly added
    to the argument list.

    Still, all #include-ed files must be available in the provided include directories,
    so that all necessary information to parse the source code is available to clang.

    :param headers: a list of header files. The header files can be declared with absolute file paths of paths relative to
                    one of the provided include directories
    :type headers: typing.Iterable[str]
    :param include_dirs: A list of (absolute) include directories
    :type include_dirs: typing.Iterable[str]
    :return: Two dictionaries containing the structured representations of class and function declarations
    :rtype: tuple(dict, dict)
    """

    check_clang()
    translation_unit = None

    # create a .cpp file including the headers and parse it with libclang
    cpp = tempfile.NamedTemporaryFile(delete=False)
    try:
        for f in headers:
            cpp.write(str.encode(f'#include "{f.relative_path}"\n'))
        cpp.close()

        index = clang.cindex.Index.create()
        compiler_args = ["-x", "c++", "-std=c++17", "-stdlib=libc++"]

        include_directories = include_dirs + get_system_include_directories()
        for dir in include_directories:
            compiler_args.append("-I{}".format(dir))

        translation_unit = index.parse(
            cpp.name,
            options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD,
            args=compiler_args,
        )

    finally:
        os.unlink(cpp.name)

    if len(translation_unit.diagnostics) > 0:
        errors = ""
        is_error = False
        for diag in translation_unit.diagnostics:
            errors = errors + diag.__str__()
            if diag.severity > 2:
                is_error = True
        if is_error:
            raise RuntimeError(
                "Parsing Source code failed with the following errors:\n"
                + textwrap.indent(errors, " " * 4)
            )
        else:
            print(textwrap.indent(errors, " " * 4))

    # define predicates on clang cursors to search in the AST
    def is_in_headers(n):
        return n.location.file and n.location.file.name in [
            h.absolute_path() for h in headers
        ]

    classes = []
    functions = []
    for node in filter_node_list_by_predicate(
        translation_unit.cursor.get_children(),
        lambda n: is_in_headers(n) and (is_class(n) or is_func(n)),
    ):
        if is_class(node):
            if not is_forward_declaration(node) and node.spelling:
                cd = ClassDecl(node)
                classes.append(cd)
        elif is_func(node):
            fd = FunctionDecl(node)

            overload = next((o for o in functions if o.name == fd.name), None)
            if overload:
                overload.is_overloaded = True
                fd.is_overloaded = True

            # variadic functions are currently not supported:
            # We need to know the exact number of arguments (for now...)
            if not fd.is_variadic:
                functions.append(fd)

    return classes, functions


class CppSource:
    """a helper class to write cpp header and src files. A CppSource
    consists of a preamble, followed by an arbitrary number of nested
    namespaces containing a single content block at the deepest level
    of nesting.

    This way, the generated code can be added to a subnamespace of an
    existing code base.
    """

    def __init__(self, preamble="", contents=""):
        self.preamble = preamble
        self.contents = contents

    def __add__(self, other):
        return CppSource(self.preamble + other.preamble, self.contents + other.contents)

    def string(self, namespaces=[]):
        """
        returns the concatenated source file with correct indentation
        """
        namespaces_start = ""
        namespaces_end = ""
        idx = 0
        for ns in namespaces:
            namespaces_start = (
                namespaces_start + " " * 4 * idx + "namespace " + ns + " {\n"
            )
            namespaces_end = (
                namespaces_end
                + " " * 4 * (len(namespaces) - idx - 1)
                + "} // namespace "
                + namespaces[-idx - 1]
                + "\n"
            )
            idx = idx + 1

        indentation = len(namespaces) * 4

        return (
            self.preamble
            + namespaces_start
            + textwrap.indent(self.contents, " " * indentation)
            + namespaces_end
        )


class HeaderPath:
    """stores the path to a header by storing an include directory, and the part of the absolute path relative to that include directory"""

    def __init__(self, include_dir, rel_path):
        self.include_directory = include_dir
        self.relative_path = rel_path.strip(os.sep)

    def absolute_path(self):
        return os.path.join(self.include_directory, self.relative_path)


class Module:
    def __init__(
        self, config, include_dirs, settings=None
    ):
        if not isinstance(config, dict):
            self.config_path = os.path.dirname(config)
            with open(config, "r")  as file:
                config = yaml.safe_load(file)

        self.name = config["name"]

        if settings is None:
            settings = self.parse_settings(config)

        self.settings = settings
        self.code_generator = settings["code_generator"]

        # set the prefix for all class, struct and function names of this
        # module
        prefix_type = self.set_prefix(config)
        self.code_generator.fully_qualified_names = False
        if prefix_type == Prefix.namespaces:
            self.code_generator.fully_qualified_names = True

        # set the headers
        self.headers = []
        if "headers" in config and config["headers"] is not None:
            # expand headers which may contain a glob pattern
            for g in config["headers"]:
                for dir in include_dirs:
                    for path in glob.iglob(os.path.join(dir, g), recursive=True):
                        # TODO: HeaderPath should be created in parse_headers maybe?
                        h = HeaderPath(dir, path[len(dir) :])
                        self.headers.append(h)

        # set the extra includes
        self.extra_includes = []
        if "extra_includes" in config and config["extra_includes"] is not None:
            # expand headers which may contain a glob pattern
            for g in config["extra_includes"]:
                for dir in include_dirs:
                    for path in glob.iglob(os.path.join(dir, g)):
                        self.extra_includes.append(path)

        # set modules
        self.modules = []
        if "modules" in config and config["modules"] is not None:
            for mod_config in config["modules"]:
                self.modules.append(Module(mod_config, include_dirs, settings))

        # set whitelist and blacklist
        self.whitelist = None
        if "whitelist" in config:
            self.whitelist = config["whitelist"]

        self.blacklist = None
        if "blacklist" in config:
            self.blacklist = config["blacklist"]

        # set declarations
        self.class_declarations = []
        self.function_declarations = []

    def parse_settings(self, config):
        # read and interpret global settings
        settings = {}

        assert "settings" in config
        settings = config["settings"]

        if "concatenate" not in settings:
            settings["concatenate"] = True

        if "customization" not in settings:
            settings["customization"] = False

        if "namespaces" not in settings:
            settings["namespaces"] = []

        if "prefix" in settings:
            prefix = Prefix[settings["prefix"]]

        if "code_generator" in settings:
            # execute the python file specified in code_generator
            file_path = settings["code_generator"]
            if not os.path.isabs(file_path):
                file_path = os.path.join(self.config_path, file_path)
            module_name = file_path.split('.')[0]
            spec = importlib.util.spec_from_file_location(module_name, file_path)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)

            # search for class subclassing CodeGenerator
            def get_subclasses(module, base_class):
                subclasses = []
                for name, obj in inspect.getmembers(module):
                    if inspect.isclass(obj) and issubclass(obj, base_class) and not obj == base_class:
                        subclasses.append(obj)
                return subclasses

            subclasses = get_subclasses(module, CodeGenerator)
            if len(subclasses) > 1:
                raise RuntimeError(f"Found more than one CodeGenerator instance in {file_path}")
            if len(subclasses) == 0:
                raise RuntimeError(f"Found no CodeGenerator instance in {file_path}")

            # replace settings["code_generator"] with instance of subclass
            settings["code_generator"] = subclasses[0]()

        else:
            settings["code_generator"] = CodeGenerator()

        settings["prefix"] = prefix
        settings["module_name_parents"] = ""

        return settings

    def set_prefix(self, config):
        self.prefix = ""
        eprefix = self.settings["prefix"]
        if "prefix" in config:
            # this module overwrites the prefix settings
            eprefix = Prefix[config["prefix"]]

        if eprefix == Prefix.module_name:
            self.prefix = self.name
        elif eprefix == Prefix.module_name_full:
            sep = ""
            if self.settings["module_name_parents"]:
                sep = "::"
            self.prefix = self.settings["module_name_parents"] + sep + self.name

        return eprefix

    def get_all_headers(self):
        headers = self.headers
        for mod in self.modules:
            headers = headers + mod.get_all_headers()
        return headers

    def whitelisted(self, decl_name):
        def matches_any(string, patterns):
            for pattern in patterns:
                if pattern in decl_name:
                    return True
            return False

        keep = True
        if self.whitelist is not None:
            keep = matches_any(decl_name, self.whitelist)
        if self.blacklist is not None:
            keep = keep and not matches_any(decl_name, self.blacklist)
        return keep

    def grab_declarations(self, declarations):

        for decl in declarations:
            if decl.header() in [
                h.absolute_path() for h in self.headers
            ] and self.whitelisted(decl.fully_qualified_name):
                if isinstance(decl, ClassDecl):
                    # filter fields for whitelist
                    fields = {}
                    for key, value in decl.fields.items():
                        if self.whitelisted(decl.fully_qualified_name + "::" + key):
                            fields[key] = value
                    decl.fields = fields

                    # filter methods for whitelist
                    methods = []
                    for method in decl.methods:
                        if self.whitelisted(method.fully_qualified_name):
                            methods.append(method)
                    decl.methods = methods

                    self.class_declarations.append(decl)
                elif isinstance(decl, FunctionDecl):
                    self.function_declarations.append(decl)

        for mod in self.modules:
            mod.grab_declarations(declarations)

    def generate_cpp_source(self, main_module_name):

        preamble = ""
        contents = ""

        self.code_generator.prefix = self.prefix

        for header in self.headers:
            preamble = preamble + f'#include "{header.relative_path}"\n'
        for header in self.extra_includes:
            preamble = preamble + f'#include "{os.path.basename(header)}"\n'

        contents = "// register types\n\n"
        for decl in self.class_declarations:
            contents = (
                contents
                + self.code_generator.cpp_register_type(decl)
                + "\n"
            )

        contents = contents + "// register functions\n\n"

        for decl in self.function_declarations:
            contents = (
                contents
                + self.code_generator.cpp_register_function(decl)
                + "\n"
            )

        contents = (
            f"\n// register module {self.name}\n"
            + f"void {main_module_name}Plugin::{self.name}_init() const\n{{\n"
            + textwrap.indent(contents, " " * 4)
            + "}\n"
        )

        return CppSource(preamble, contents)

    def collect_cpp_source(self, main_module_name=None):

        # not pretty...better be explicit
        if main_module_name is None:
            main_module_name = self.name

        cpp_source = {}
        cpp_source[self.name] = self.generate_cpp_source(main_module_name)
        for mod in self.modules:
            cpp_source = {**cpp_source, **mod.collect_cpp_source(main_module_name)}
        return cpp_source


def create_plugin_src(name, settings, module_names):
    """creates the source files for the grunk plugin"""

    version = settings["version"]
    customization = settings["customization"]

    preamble = "#pragma once\n\n#include <grunk/grunk.hpp>\n\n"

    extra = ""
    if customization:
        extra = extra + "    void custom_init() const;"

    contents = f"""
//TODO: Shoul we make this part of reflect?
template <typename T>
inline reflect::TypeFactory<T> modify_type()
{{
    return reflect::TypeFactory<T>(*reflect::details::resolve<T>());
}}

class {name}Plugin : public grunk::IPlugin
{{
public:
    {name}Plugin() = default;

    virtual std::string name() const override final;
    virtual std::string version() const override final;
    virtual void init() const override final;
private:
"""

    for mod_name in module_names:
        contents = contents + f"    void {mod_name}_init() const;\n"

    contents = (
        contents
        + f"""{extra}
}};
GRUNK_REGISTER_PLUGIN({name}Plugin)\n
"""
    )

    header = CppSource(preamble, contents)

    preamble = f'#include "{name}Plugin.hpp"\n'

    init_calls = ""
    for mod_name in module_names:
        init_calls = init_calls + f"{mod_name}_init();\n"
    if customization:
        init_calls = init_calls + "\n// custom initialization\n" + "custom_init();\n"

    contents = f"""
std::string {name}Plugin::name() const
{{
    return "{name}";
}}

std::string {name}Plugin::version() const
{{
    return "{version}";
}}

void {name}Plugin::init() const
{{

    // initialize modules
{textwrap.indent(init_calls, ' '*4)}
}}\n
"""

    cpp = CppSource(preamble, contents)
    return header, cpp


class Prefix(Enum):
    none = 0
    module_name_full = 1
    module_name = 2
    namespaces = 3

    @classmethod
    def _missing_(cls, value):
        return cls.none


def generate(config_file: str, output_dir: str, include_dirs: typing.Iterable[str]):
    """generates the source code for the grunk plugin

    :param config_file: A yml configuration file
    :type config_file: str
    :param output_dir: the output directory for the generated code
    :type output_dir: str
    :param include_dirs: a list of include directories for the clang parser
    :type include_dirs: typing.Iterable[str]
    """

    # create output_dir if it does not exist
    Path(output_dir).mkdir(parents=True, exist_ok=True)

    # recursively create all modules
    main_module = Module(config_file, include_dirs)

    # collect the list of all headers of all modules
    headers = main_module.get_all_headers()

    # parse the source code once for all modules
    classes, functions = parse_headers(headers, include_dirs)

    # recursively collect all declarations belonging to the modules
    main_module.grab_declarations(classes + functions)

    # get the C++ source code of all modules in a "flat" dictionary
    modules_src = main_module.collect_cpp_source()

    namespaces = main_module.settings["namespaces"]

    # create source code for the grunk plugin

    hpp, cpp = create_plugin_src(
        main_module.name, main_module.settings, modules_src.keys()
    )

    hpp_file = os.path.join(output_dir, f"{main_module.name}Plugin.hpp")
    with open(hpp_file, "w") as f:
        f.write(hpp.string(namespaces))

    cpp_file = os.path.join(output_dir, f"{main_module.name}Plugin.cpp")
    with open(cpp_file, "w") as f:
        f.write(cpp.string(namespaces))

    if main_module.settings["concatenate"]:

        # create .cpp file
        src = CppSource()
        for [_, source] in modules_src.items():
            src = src + source

        src.preamble = src.preamble + f'#include "{main_module.name}Plugin.hpp"\n'
        src.preamble = src.preamble + "\n#include <grunk/grunk.hpp>\n"
        cpp = src.string(namespaces)

        cpp_file = os.path.join(output_dir, f"{main_module.name}.cpp")
        with open(cpp_file, "w") as f:
            f.write(cpp)
    else:
        
        for [module_name, src] in modules_src.items():

            # create .cpp file
            src.preamble = src.preamble + f'#include "{main_module.name}Plugin.hpp"\n'
            src.preamble = src.preamble + "\n#include <grunk/grunk.hpp>\n"
            cpp = src.string(namespaces)

            cpp_file = os.path.join(output_dir, f"{module_name}.cpp")
            with open(cpp_file, "w") as f:
                f.write(cpp)


# To Do:
# - parse docstrings
