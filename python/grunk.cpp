#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <grunk/grunk.hpp>

namespace grunk {
    namespace py {

        // invoke a variadic function f on the elements of a py::args, where each element is cast to T. 
        template <typename T, typename F, size_t... idx>
        decltype(auto) invoke_variadic_rt(F&& f, pybind11::args& args, std::index_sequence<idx...>)
        {
            return std::invoke(f, pybind11::cast<T>(args[idx])...);
        }

        // invoke a variadic function f on the elements of a py::args, where each element is cast to T. 
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

    } //namespace py
} //namespace grunk

// PYBIND11_MAKE_OPAQUE(grunk::FeatureContainer);
PYBIND11_MAKE_OPAQUE(std::unordered_map<std::string, grunk::DynamicFeature>);

namespace py = pybind11;

PYBIND11_MODULE(_grunk, m)
{
    m.attr("__version__") = grunk_VERSION;

    // plugins

    py::class_<grunk::PluginRegistry>(m, "PluginRegistry")
    .def("prepend_path", &grunk::PluginRegistry::prepend_path)
    .def("print_plugins", &grunk::PluginRegistry::print_plugins)
    .def("count", &grunk::PluginRegistry::count)
    .def("load_all", &grunk::PluginRegistry::load_all)
    .def("unload_all", &grunk::PluginRegistry::unload_all);

    m.def("get_plugin_registry", &grunk::get_plugin_registry);

    // dynamic

    py::class_<grunk::DynamicFeature>(m, "Feature")
    .def(py::init<std::string const&, std::string const&, py::args>()) //TODO!!
    .def("get", &grunk::DynamicFeature::get)
    .def("invoke", 
        [](grunk::DynamicFeature const& f, std::string const& mName, py::args pyargs){
            return grunk::py::invoke_variadic_rt<grunk::DynamicFeature>(
                [&](auto&&... args){
                    return f.invoke(mName, std::forward<decltype(args)>(args)...);
                },
                pyargs
            );
        }
    )
    .def("is_valid", &grunk::DynamicFeature::is_valid)
    .def("value", &grunk::DynamicFeature::value)
    .def("access_value", &grunk::DynamicFeature::access_value)
    .def_property(
        "id", 
        &grunk::DynamicFeature::id,
        &grunk::DynamicFeature::set_id
    );

    m.def(
        "action", 
        [](std::string const& id, std::string const& name, py::args pyargs){
            return grunk::py::invoke_variadic_rt<grunk::DynamicFeature>(
                [&](auto&&...args){
                    return grunk::action(id, name, std::forward<decltype(args)>(args)...);
                },
                pyargs
            );
        }
    );

    // io

    py::bind_map<grunk::FeatureContainer>(m, "FeatureContainer");
    m.def("read", &grunk::read);
    m.def(
        "write",
        [](std::string const& filename, py::args pyargs){
            return grunk::py::invoke_variadic_rt<grunk::DynamicFeature>(
                [&](auto&&...args){
                    return grunk::write(filename, std::forward<decltype(args)>(args)...);
                },
                pyargs
            );
        }
    );

}