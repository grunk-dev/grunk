// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/string.h>

#include <grunk/grunk.hpp>

namespace nb = nanobind;

using namespace nb::literals;

NB_MODULE(_bindings, m) {
    m.attr("__version__") = grunk_VERSION;
    m.doc() = "A parametric modeling backend for Python, C++ and Lua.";

    nb::class_<grunk::object>(m, "object")
        .def(
            "__add__", 
            [](grunk::object const& l, grunk::object const& r) {
                return l + r;
            }, 
            nb::is_operator()
        )
        .def(
            "__sub__", 
            [](grunk::object const& l, grunk::object const& r) {
                return l - r;
            }, 
            nb::is_operator()
        )
        .def(
            "__mul__", 
            [](grunk::object const& l, grunk::object const& r) {
                return l * r;
            },
            nb::is_operator()
        )
        .def(
            "__truediv__", 
            [](grunk::object const& l, grunk::object const& r) {
                return l / r;
            },
            nb::is_operator()
        )
        .def(
            "__pow__", 
            [](grunk::object const& l, grunk::object const& r) {
                return grunk::pow(l, r);
            },
            nb::is_operator()
        )
        .def(
            "__mod__", 
            [](grunk::object const& l, grunk::object const& r) {
                return l % r;
            },
            nb::is_operator()
        )
        .def(
            "__neg__", 
            [](grunk::object const& obj) {
                return -obj;
            },
            nb::is_operator()
        )
        .def("as_bool", &grunk::object::as<bool>)
        .def("as_float", &grunk::object::as<double>)
        .def("as_string", &grunk::object::as<std::string>)
        .def("as_feature", &grunk::object::as<grunk::DynamicFeature>);


    nb::class_<grunk::DynamicFeature>(m, "Feature")
        .def(nb::init<>())
        .def(nb::init<grunk::object const&>(), "value"_a)
        .def("with_id", &grunk::DynamicFeature::with_id, "id"_a)
        .def("is_placeholder", &grunk::DynamicFeature::is_placeholder)
        .def("value", &grunk::DynamicFeature::value)
        .def("set_value", &grunk::DynamicFeature::set_value<int>, "value"_a)
        .def("set_value", &grunk::DynamicFeature::set_value<double>, "value"_a)
        .def("set_value", &grunk::DynamicFeature::set_value<std::string>, "value"_a)
        .def("set_value", &grunk::DynamicFeature::set_value<grunk::object>, "value"_a)
        .def("set_value", &grunk::DynamicFeature::set_value<bool>, "value"_a   )
        .def(
            "__add__", 
            [](grunk::DynamicFeature const& l, grunk::DynamicFeature const& r) {
                return l + r;
            }, 
            nb::is_operator()
        )
        .def(
            "__sub__", 
            [](grunk::DynamicFeature const& l, grunk::DynamicFeature const& r) {
                return l - r;
            }, 
            nb::is_operator()
        )
        .def(
            "__mul__", 
            [](grunk::DynamicFeature const& l, grunk::DynamicFeature const& r) {
                return l * r;
            },
            nb::is_operator()
        )
        .def(
            "__truediv__", 
            [](grunk::DynamicFeature const& l, grunk::DynamicFeature const& r) {
                return l / r;
            },
            nb::is_operator()
        )
        .def(
            "__pow__", 
            [](grunk::DynamicFeature const& l, grunk::DynamicFeature const& r) {
                return grunk::pow(l, r);
            },
            nb::is_operator()
        )
        .def(
            "__mod__", 
            [](grunk::DynamicFeature const& l, grunk::DynamicFeature const& r) {
                return l % r;
            },
            nb::is_operator()
        )
        .def(
            "__neg__", 
            [](grunk::DynamicFeature const& obj) {
                return -obj;
            },
            nb::is_operator()
        );

    nb::class_<grunk::environment>(m, "environment")
        .def(
            "eval", 
            [](grunk::environment& env, std::string const& code) {
                env.eval(code);
            },
            "code"_a
        )
        .def(
            "__getitem__", 
            [](grunk::environment& env, std::string const& key) {
                grunk::object obj = env[key];
                return obj;
            },
            "key"_a
        )
        .def("tag_features", &grunk::environment::tag_features);

    nb::class_<grunk::state>(m, "state")
        .def(nb::init<>())
        .def("create_env", &grunk::state::create_env)
        .def("create_parametric_env", &grunk::state::create_parametric_env)
        .def("create_recipe", &grunk::state::create_recipe)
        .def("write", &grunk::state::write, "filename"_a, "recipe"_a)
        .def("read", &grunk::state::read, "filename"_a)
        .def("feature", [](grunk::state const& grunk, nb::object obj) {
            if (nb::isinstance<nb::int_>(obj))
                return grunk.feature(nb::cast<int>(obj));
            else if (nb::isinstance<nb::float_>(obj))
                return grunk.feature(nb::cast<double>(obj));
            else if (nb::isinstance<nb::str>(obj))
                return grunk.feature(nb::cast<std::string>(obj));
            else
                throw nb::type_error("Unsupported type");
        }, "obj"_a);

}