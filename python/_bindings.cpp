// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/shared_ptr.h>

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
        .def("id", &grunk::DynamicFeature::id)
        .def("set_id", &grunk::DynamicFeature::set_id, "id"_a)
        .def("is_valid", &grunk::DynamicFeature::is_valid)
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

    m.def("pow", [](grunk::DynamicFeature const& base, grunk::DynamicFeature const& exponent) {
        return grunk::pow(base, exponent);
    }, "base"_a, "exponent"_a);

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
        .def(
            "__setitem__", 
            [](grunk::environment& env, std::string const& key, grunk::object const& value) {
                env[key] = value;
            },
            "key"_a, "value"_a
        )
        .def(
            "__setitem__", 
            [](grunk::environment& env, std::string const& key, grunk::DynamicFeature const& value) {
                env[key] = value;
            },
            "key"_a, "value"_a
        )
        .def("get_feature", [](grunk::environment& env, std::string const& key) {
            return env.get_feature(key);
        }, "key"_a)
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

    // Expose a default state for convenience. This allows users to use grunk without explicitly creating a state
    // Member functions of the default state can be accessed via module-level functions that forward to the default state. 
    auto _default_state = std::make_shared<grunk::state>();
    m.attr("__default_state__") = _default_state;
    auto default_state = [m]() -> grunk::state& {
        auto ptr = nb::cast<std::shared_ptr<grunk::state>>(m.attr("__default_state__"));
        return *(ptr.get());
    };
    m.def("create_env", [=]() {
        return default_state().create_env();
    });
    m.def("create_parametric_env", [=]() {
        return default_state().create_parametric_env();
    });
    m.def("create_recipe", [=]() {
        return default_state().create_recipe();
    });
    m.def("write", [=](std::string const& filename, grunk::Recipe const& recipe) {
        default_state().write(filename, recipe);
    }, "filename"_a, "recipe"_a);
    m.def("read", [=](std::string const& filename) {
        return default_state().read(filename);
    }, "filename"_a);
    m.def("feature", [=](nb::object obj) {
        if (nb::isinstance<nb::int_>(obj))
            return default_state().feature(nb::cast<int>(obj));
        else if (nb::isinstance<nb::float_>(obj))
            return default_state().feature(nb::cast<double>(obj));
        else if (nb::isinstance<nb::str>(obj))
            return default_state().feature(nb::cast<std::string>(obj));
        else
            throw nb::type_error("Unsupported type");
    }, "obj"_a);

    auto recipe = nb::class_<grunk::Recipe, grunk::environment>(m, "Recipe")
        .def("to_string", &grunk::Recipe::to_string)
        .def("clone", &grunk::Recipe::clone)
        .def("populate_from_file", &grunk::Recipe::populate_from_file, "filename"_a)
        .def("populate_from_string", &grunk::Recipe::populate_from_string, "yml"_a)
        .def("insert_recipe", &grunk::Recipe::insert_recipe, "name"_a, "recipe"_a)
        .def("tag", &grunk::Recipe::tag);

    // nb::class_<grunk::Recipe::SubRecipe>(recipe, "SubRecipe")
    //     .def_readonly("name", &grunk::Recipe::SubRecipe::name)
    //     .def_readonly("recipe", &grunk::Recipe::SubRecipe::recipe)
    //     .def("__call__", &grunk::Recipe::SubRecipe::operator());

    // auto recipe_caller =nb::class_<grunk::RecipeCaller>(m, "RecipeCaller")
    //     .def(nb::init<std::string const&, grunk::Feature<grunk::Recipe> const&, lua_State*>(), "name"_a, "recipe"_a, "lua"_a)
    //     .def("with_id", &grunk::RecipeCaller::with_id, "id"_a)
    //     .def("set_id", &grunk::RecipeCaller::set_id, "id"_a)
    //     .def("get", &grunk::RecipeCaller::get, "key"_a)
    //     .def("__getitem__", &grunk::RecipeCaller::get, "key"_a)
    //     .def("locked", &grunk::RecipeCaller::locked)

    // nb::class_<grunk::RecipeCaller::Proxy>(recipe_caller, "Proxy")
    //     .def("__set__", &grunk::RecipeCaller::Proxy::operator=)
    //     .def("__call__", &grunk::RecipeCaller::Proxy::operator DynamicFeature);

}