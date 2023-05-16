#define PYBIND11_DETAILED_ERROR_MESSAGES
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <grunk/grunk.hpp>

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

// the following code is needed to wrap grunk::DynamicActionPtr
// We will tell pybind11 that grunk::DynamicActionPtr is a smart pointer.
// pybind11 expects smartpointer to have a get member. Since parametric::compute_node_ptr
// doesn't have that member, we need to define a holder_helper type,
// see https://pybind11.readthedocs.io/en/stable/advanced/smart_ptrs.html#custom-smart-pointers
namespace PYBIND11_NAMESPACE { namespace detail {

    template <typename T>
    using SmartPtr = parametric::compute_node_ptr<T>;

    template <typename T>
    struct holder_helper<SmartPtr<T>> { // <-- specialization
        static const T *get(const SmartPtr<T> &p) { 
            return const_cast<SmartPtr<T>&>(p).operator->().get(); 
        }
    };
}}
PYBIND11_DECLARE_HOLDER_TYPE(DynamicAction, parametric::compute_node_ptr<DynamicAction>);



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

    //TODO: I seem to have to do this for all builtin types!
    auto dynobj = py::class_<reflect::DynamicObject>(m_reflect, "DynamicObject");
    dynobj.def("get", static_cast<reflect::DynamicObject (reflect::DynamicObject::*)(std::string const&) const>(&reflect::DynamicObject::get));
    
    dynobj.def(py::init<double>())
    .def("as_float", static_cast<double (reflect::DynamicObject::*)() const>(&reflect::DynamicObject::as<double>));
    py::implicitly_convertible<double, reflect::DynamicObject>();

    dynobj.def(py::init<int>())
    .def("as_int", static_cast<int (reflect::DynamicObject::*)() const>(&reflect::DynamicObject::as<int>));
    py::implicitly_convertible<int, reflect::DynamicObject>();

    dynobj.def(py::init<std::string>())
    .def("as_str", static_cast<int (reflect::DynamicObject::*)() const>(&reflect::DynamicObject::as<std::string>));
    py::implicitly_convertible<std::string, reflect::DynamicObject>();

    // dynamic

    py::class_<DynamicAction, parametric::compute_node_ptr<DynamicAction>>(m, "DynamicAction")
    .def(
        "output", 
        // py::overload_cast<size_t>(&DynamicAction::output), 
        static_cast<DynamicFeature (DynamicAction::*)(size_t) const>(&DynamicAction::output),
        py::arg("idx") = 0
    );

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
        [](grunk::DynamicFeature const& f, std::string const& mName, py::args pyargs){
            return grunkpy::invoke_variadic_rt<grunk::DynamicFeature>(
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
            return grunkpy::invoke_variadic_rt<grunk::DynamicFeature const&>(
                [&](auto&&... args){
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
            return grunkpy::invoke_variadic_rt<grunk::DynamicFeature>(
                [&](auto&&...args){
                    return grunk::write(filename, std::forward<decltype(args)>(args)...);
                },
                pyargs
            );
        }
    );

}