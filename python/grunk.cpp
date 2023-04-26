#include <pybind11/pybind11.h>
#include <grunk/grunk.hpp>

namespace py = pybind11;
using namespace grunk;

PYBIND11_MODULE(_grunk, m)
{
    m.attr("__version__") = grunk_VERSION;

    py::class_<DynamicFeature>(m, "DynamicFeature")
    .def("get", &DynamicFeature::get);

    // m.def("action", &action);

}