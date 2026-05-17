// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/unordered_map.h>

#include <grunk/grunk.hpp>

namespace nb = nanobind;

using namespace nb::literals;

namespace {

    template <typename T, typename nbclass_t>
    void add_feature_base_methods(nbclass_t& nbclass) {
        nbclass.def("id", &T::id)
            .def("set_id", &T::set_id, "id"_a)
            .def("is_valid", &T::is_valid)
            .def("with_id", &T::with_id, "id"_a)
            .def("is_placeholder", &T::is_placeholder)
            .def("change_value", &T::change_value)
            .def("value", &T::value);
    };

} // anonymous namespace

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
        .def("as_int", &grunk::object::as<int>)
        .def("as_float", &grunk::object::as<double>)
        .def("as_string", &grunk::object::as<std::string>)
        .def("as_feature", &grunk::object::as<grunk::DynamicFeature>);

    auto feature = nb::class_<grunk::DynamicFeature>(m, "Feature")
        .def(nb::init<>())
        .def(nb::init<grunk::object const&>(), "value"_a)
        .def("set_value", &grunk::DynamicFeature::set_value<int>, "value"_a)
        .def("set_value", &grunk::DynamicFeature::set_value<double>, "value"_a)
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

    add_feature_base_methods<grunk::DynamicFeature>(feature);

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
                grunk::object obj = env.get(key); // use get() to get better error messages when key is not found
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
        .def("run_module_script", &grunk::state::run_module_script, "name"_a, "script"_a)
        .def("clear_module", &grunk::state::clear_module, "name"_a)
        .def("create_object", [](grunk::state const& grunk, nb::object obj) {
            if (nb::isinstance<nb::int_>(obj))
                return grunk.create_object(nb::cast<int>(obj));
            else if (nb::isinstance<nb::float_>(obj))
                return grunk.create_object(nb::cast<double>(obj));
            else if (nb::isinstance<nb::str>(obj))
                return grunk.create_object(nb::cast<std::string>(obj));
            else
                throw nb::type_error("Unsupported type");
        }, "obj"_a)
        .def("feature", [](grunk::state const& grunk, nb::object obj) {
            if (nb::isinstance<nb::int_>(obj))
                return grunk.feature(nb::cast<int>(obj));
            else if (nb::isinstance<nb::float_>(obj))
                return grunk.feature(nb::cast<double>(obj));
            else if (nb::isinstance<nb::str>(obj))
                return grunk.feature(nb::cast<std::string>(obj));
            else
                throw nb::type_error("Unsupported type");
        }, "obj"_a)
        .def("feature", [](grunk::state const& grunk) { return grunk.feature(); });

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
    m.def("run_module_script", [=](std::string const& name, std::string const& script) {
        default_state().run_module_script(name, script);
    }, "name"_a, "script"_a);
    m.def("clear_module", [=](std::string const& name) {
        default_state().clear_module(name);
    }, "name"_a);
    m.def("write", [=](std::string const& filename, grunk::Recipe const& recipe) {
        default_state().write(filename, recipe);
    }, "filename"_a, "recipe"_a);
    m.def("read", [=](std::string const& filename) {
        return default_state().read(filename);
    }, "filename"_a);
    m.def("run_module_script", [=](std::string const& name, std::string const& script){ 
        default_state().run_module_script(name, script);
    }, "name"_a, "script"_a);
    m.def("clear_module", [=](std::string const& name){
        default_state().clear_module(name);
    }, "name"_a);
    m.def("create_object", [=](nb::object obj) {
        if (nb::isinstance<nb::int_>(obj))
            return default_state().create_object(nb::cast<int>(obj));
        else if (nb::isinstance<nb::float_>(obj))
            return default_state().create_object(nb::cast<double>(obj));
        else if (nb::isinstance<nb::str>(obj))
            return default_state().create_object(nb::cast<std::string>(obj));
        else
            throw nb::type_error("Unsupported type");
    }, "obj"_a);
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
    m.def("feature", [=]() { 
        return default_state().feature();
    });

    auto recipe = nb::class_<grunk::Recipe, grunk::environment>(m, "Recipe")
        .def("to_string", &grunk::Recipe::to_string)
        .def("clone", &grunk::Recipe::clone)
        .def("populate_from_file", &grunk::Recipe::populate_from_file, "filename"_a)
        .def("populate_from_string", &grunk::Recipe::populate_from_string, "yml"_a)
        .def(
            "insert_recipe", 
            [](grunk::Recipe& self, std::string const& name, grunk::Recipe const& inner)
            { 
                // this is unfortunate, but without move-semantics in python, 
                // we have to deep clone here
                self.insert_recipe(name, inner.clone()); 
            }, 
            "name"_a,
            "inner_recipe"_a
        )
        .def("tag", &grunk::Recipe::tag)
        .def("get_recipe", [](grunk::Recipe& r, std::string const& key) {
            return r.get_recipe(key);
        }, "key"_a)
        .def_ro("recipes", &grunk::Recipe::recipes);

    auto recipe_feature = nb::class_<grunk::Feature<grunk::Recipe>>(m, "RecipeFeature");
    add_feature_base_methods<grunk::Feature<grunk::Recipe>>(recipe_feature);

    nb::class_<grunk::Recipe::SubRecipe>(recipe, "SubRecipe")
        .def_ro("name", &grunk::Recipe::SubRecipe::name)
        .def_ro("recipe", &grunk::Recipe::SubRecipe::recipe)
        .def("__call__", &grunk::Recipe::SubRecipe::operator());

    auto recipe_caller =nb::class_<grunk::RecipeCaller>(m, "RecipeCaller")
        .def("with_id", &grunk::RecipeCaller::with_id, "id"_a)
        .def("set_id", &grunk::RecipeCaller::set_id, "id"_a)
        .def("get", &grunk::RecipeCaller::get, "key"_a)
        .def("__getitem__", &grunk::RecipeCaller::get, "key"_a)
        .def("__setitem__", [](grunk::RecipeCaller& rc, std::string const& key, grunk::DynamicFeature const& value) {
            rc[key] = value;
        }, "key"_a, "value"_a)
        .def("locked", &grunk::RecipeCaller::locked);

}