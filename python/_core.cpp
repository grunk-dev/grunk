#define PYBIND11_DETAILED_ERROR_MESSAGES
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <grunk/grunk.hpp>
#include <grunk/helper/String.hpp>

namespace grunkpy {

    // invoke a variadic function f on the elements of a py::args, where each element is cast to T. 
    template <typename T, typename F, size_t... idx>
    decltype(auto) invoke_variadic_rt(F&& f, pybind11::args& args, std::index_sequence<idx...>)
    {
        return std::invoke(f, pybind11::cast<T>(args[idx])...);
    }

    // invoke a variadic function f on the elements of a py::args, where each element is cast to T. 
    // This function converts runtime integers to compile-time integers, effectively instantiating the 
    // templated function for the correct number of arguments.
    template <typename T, typename F>
    decltype(auto) invoke_variadic_rt(F&& f, pybind11::args& args)
    {
        switch (args.size()) 
        {
            case ( 0): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence< 0>{});
            case ( 1): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence< 1>{});
            case ( 2): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence< 2>{});
            case ( 3): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence< 3>{});
            case ( 4): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence< 4>{});
            case ( 5): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence< 5>{});
            case ( 6): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence< 6>{});
            case ( 7): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence< 7>{});
            case ( 8): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence< 8>{});
            case ( 9): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence< 9>{});
            case (10): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence<10>{});
            case (11): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence<11>{});
            case (12): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence<12>{});
            case (13): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence<13>{});
            case (14): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence<14>{});
            case (15): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence<15>{});
            case (16): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence<16>{});
            case (17): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence<17>{});
            case (18): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence<18>{});
            case (19): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence<19>{});
            case (20): return invoke_variadic_rt<T>(std::forward<F>(f), args, std::make_index_sequence<20>{});
            default: throw std::runtime_error("grunk only supports up to 20 positional arguments through python bindings\n");
        }
    }

} //namespace grunkpy

using namespace grunk;
namespace py = pybind11;

PYBIND11_MAKE_OPAQUE(std::unordered_map<std::string, DynamicFeature>);
PYBIND11_MAKE_OPAQUE(std::vector<reflect::DynamicObject>);


PYBIND11_MODULE(_core, m)
{
    m.attr("__version__") = grunk_VERSION;

    // plugins

    py::class_<grunk::PluginRegistry>(m, "PluginRegistry")
    .def("prepend_path", &grunk::PluginRegistry::prepend_path)
    .def("print_plugins", &grunk::PluginRegistry::print_plugins)
    .def("count", &grunk::PluginRegistry::count)
    .def("load_all", &grunk::PluginRegistry::load_all)
    .def("unload_all", &grunk::PluginRegistry::unload_all);

    m.def(
        "get_plugin_registry", 
        &grunk::get_plugin_registry, 
        py::return_value_policy::reference
    );

    // reflect

    auto m_reflect = m.def_submodule("reflect", "python bindings for reflect");

    auto dynobj = py::class_<reflect::DynamicObject>(m_reflect, "DynamicObject")
    .def("get", static_cast<reflect::DynamicObject (reflect::DynamicObject::*)(std::string const&) const>(&reflect::DynamicObject::get))
    .def("has_value", &reflect::DynamicObject::has_value)
    .def("is_owning", &reflect::DynamicObject::is_owning)
    .def("set", static_cast<void (reflect::DynamicObject::*)(std::string const&, reflect::DynamicObject const&)>(&reflect::DynamicObject::set))
    .def("invoke", 
        [](reflect::DynamicObject const& f, std::string const& name, py::args pyargs){
            return grunkpy::invoke_variadic_rt<reflect::DynamicObject>(
                [&](auto&&... args){
                    return f.invoke(name, std::forward<decltype(args)>(args)...);
                },
                pyargs
            );
        }
    );

    m_reflect.def(
        "invoke",
        [](std::string const& name, py::args pyargs){
            return grunkpy::invoke_variadic_rt<reflect::DynamicObject>(
                [&](auto&&... args){
                    return reflect::invoke(name, std::forward<decltype(args)>(args)...);
                },
                pyargs
            );
        }
    );
    
    dynobj.def(py::init<double>())
    .def("as_float", static_cast<double (reflect::DynamicObject::*)() const>(&reflect::DynamicObject::as<double>));
    py::implicitly_convertible<double, reflect::DynamicObject>();

    dynobj.def(py::init<int>())
    .def("as_int", static_cast<int (reflect::DynamicObject::*)() const>(&reflect::DynamicObject::as<int>));
    py::implicitly_convertible<int, reflect::DynamicObject>();

    dynobj.def(
        py::init(
            [](std::string const& s){
                return reflect::DynamicObject(grunk::helper::String(s));
            }
        )
    )
    .def("as_str", static_cast<std::string (reflect::DynamicObject::*)() const>(&reflect::DynamicObject::as<std::string>));
    py::implicitly_convertible<std::string, reflect::DynamicObject>();

    m_reflect.def("help", &reflect::help);

    // dynamic

    using ResultHolder_DynamicAction = ResultHolder<DynamicAction>;
    py::class_<ResultHolder_DynamicAction>(m, "ResultHolder_DynamicAction")
    .def(
        "output", 
        &ResultHolder_DynamicAction::output,
        py::arg("idx") = 0
    )
    .def("size", &ResultHolder_DynamicAction::size)
    .def("eval", &ResultHolder_DynamicAction::eval);

    py::class_<grunk::DynamicFeature>(m, "Feature")
    .def(
        py::init(
            [](std::string const& id, std::string const& name, py::args pyargs){
                return grunkpy::invoke_variadic_rt<reflect::DynamicObject>(
                    [&](auto&&... args) -> grunk::DynamicFeature {
                        return grunk::DynamicFeature(id, name, std::forward<decltype(args)>(args)...);
                    },
                    pyargs
                );
            }
        ),
        py::return_value_policy::take_ownership
    )
    .def("get", &grunk::DynamicFeature::get)
    .def("invoke", 
        [](grunk::DynamicFeature const& f, std::string id, std::string const& mName, py::args pyargs){
            return grunkpy::invoke_variadic_rt<grunk::DynamicFeature>(
                [&](auto&&... args){
                    return f.invoke(id, mName, std::forward<decltype(args)>(args)...);
                },
                pyargs
            );
        }
    )
    .def("is_valid", &grunk::DynamicFeature::is_valid)
    .def("value", &grunk::DynamicFeature::value)
    .def(
        "access_value",
        &grunk::DynamicFeature::access_value,
        py::return_value_policy::reference_internal
    )
    .def("set_value", &grunk::DynamicFeature::set_value)
    .def_property(
        "id", 
        &grunk::DynamicFeature::id,
        &grunk::DynamicFeature::set_id
    );

    using RecipeOutputMap = std::unordered_map<std::string, std::string>;
    py::bind_map<grunk::Recipe::FeatureContainer>(m, "FeatureContainer");
    py::bind_map<RecipeOutputMap>(m, "RecipeOutputMap");

    py::class_<grunk::Recipe>(m, "Recipe")
    .def(
        py::init(
            [](py::args pyargs){
                return grunkpy::invoke_variadic_rt<grunk::DynamicFeature>(
                    [&](auto&&... args) -> grunk::Recipe {
                        return grunk::Recipe(std::forward<decltype(args)>(args)...);
                    },
                    pyargs
                );
            }
        )
    )
    .def("clone", &Recipe::clone)
    .def(
        "get_features", 
        py::overload_cast<>(&grunk::Recipe::get_features, py::const_),
        py::return_value_policy::reference_internal
    )
    .def(
        "at", 
        py::overload_cast<std::string const&>(&grunk::Recipe::at, py::const_),
        py::return_value_policy::reference_internal
    )
    .def("insert_feature", &grunk::Recipe::insert_feature)
    .def("num_features", &grunk::Recipe::num_features)
    .def(
        "feature",
        [](grunk::Recipe& r, std::string const& id, std::string const& type_name, py::args pyargs){
            grunkpy::invoke_variadic_rt<reflect::DynamicObject>(
                [&](auto&&... args) {
                    r.feature(id, type_name, std::forward<decltype(args)>(args)...);
                },
                pyargs
            );
        }
    )
    .def(
        "get_recipe", 
        py::overload_cast<std::string const&>(&grunk::Recipe::get_recipe, py::const_),
        py::return_value_policy::reference_internal
    )
    .def(
        "insert_recipe", 
        [](grunk::Recipe& r, std::string const& id, grunk::Recipe const& other) {
            r.insert_recipe(id, std::move(other.clone()));
        }
    )
    .def("num_recipes", &grunk::Recipe::num_recipes)
    .def(
        "__call__",
        [](
            Recipe const& recipe,
            std::string const& name, 
            RecipeOutputMap const& output_ids, 
            grunk::Recipe::FeatureContainer const& inputs
        ) 
        {
            std::vector<grunk::Recipe::IDPair> output_ids_vec;
            std::transform(
                output_ids.begin(),
                output_ids.end(),
                std::back_inserter(output_ids_vec),
                [](auto const& kv){ return grunk::Recipe::IDPair{kv.first, kv.second}; }
            );
            return recipe(name, output_ids_vec, inputs);
        }
    )
    .def(
        "recipe",
        [](
            Recipe& recipe,
            std::string const& name, 
            RecipeOutputMap const& output_ids, 
            grunk::Recipe::FeatureContainer const& inputs
        ) 
        {
            std::vector<grunk::Recipe::IDPair> output_ids_vec;
            std::transform(
                output_ids.begin(),
                output_ids.end(),
                std::back_inserter(output_ids_vec),
                [](auto const& kv){ return grunk::Recipe::IDPair{kv.first, kv.second}; }
            );
            return recipe.recipe(name, output_ids_vec, inputs);
        }
    );

    m.def(
        "action", 
        [](std::string const& id, std::string const& name, py::args pyargs){
            return grunkpy::invoke_variadic_rt<grunk::DynamicFeature const&>(
                [&](auto&&... args){
                    return grunk::action(id, name, std::forward<decltype(args)>(args)...);
                },
                pyargs
            );
        }
    );

    m.def(
        "expression", 
        [](std::string const& id, std::string const& expr, py::args pyargs){
            return grunkpy::invoke_variadic_rt<grunk::DynamicFeature const&>(
                [&](auto&&... args){
                    return grunk::expression(id, expr, std::forward<decltype(args)>(args)...);
                },
                pyargs
            );
        }
    );

    // io

    m.def("read", &grunk::read);
    m.def(
        "write",
        [](std::string const& filename, py::args pyargs){
            return grunkpy::invoke_variadic_rt<grunk::DynamicFeature>(
                [&](auto&&...args){
                    return grunk::write(filename, args...);
                },
                pyargs
            );
        }
    );
    m.def("write", static_cast<void(*)(std::string const&, grunk::Recipe const&)>(&grunk::write));

}