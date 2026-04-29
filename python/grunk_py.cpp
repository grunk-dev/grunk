// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <nanobind/nanobind.h>
#include <grunk/grunk.hpp>

namespace nb = nanobind;

using namespace nb::literals;

NB_MODULE(grunkpy, m) {
    m.attr("__version__") = grunk_VERSION;
    m.doc() = "This is a \"hello world\" example with nanobind";
    m.def("add", [](int a, int b) { return a + b; }, "a"_a, "b"_a);
}