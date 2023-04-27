#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <grunk/grunk.hpp>

namespace py = pybind11;

// PYBIND11_MAKE_OPAQUE(grunk::FeatureContainer);
PYBIND11_MAKE_OPAQUE(std::unordered_map<std::string, grunk::DynamicFeature>);

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
    .def(py::init<std::string const&, std::string const&, py::args>())
    .def("get", &grunk::DynamicFeature::get)
    .def("is_valid", &grunk::DynamicFeature::is_valid)
    .def("value", &grunk::DynamicFeature::value)
    .def("access_value", &grunk::DynamicFeature::access_value)
    .def_property(
        "id", 
        &grunk::DynamicFeature::id,
        &grunk::DynamicFeature::set_id
    );

    // m.def("action", &grunk::action);

    // io

    py::bind_map<grunk::FeatureContainer>(m, "FeatureContainer");
    m.def("read", &grunk::read);

}