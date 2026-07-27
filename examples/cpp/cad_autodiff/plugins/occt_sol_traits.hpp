// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <Standard_Handle.hxx>
#include <sol/sol.hpp>

// Lets sol2 treat an OCCT opencascade::handle<T> (i.e. Handle(T)) like any other
// unique-ownership smart pointer, so a Handle can be returned from a registered
// grunk function or pulled back out of a grunk::object via .as<Handle(T)>() without
// a raw-pointer detour. Shared by main.cpp (recipe read-back) and the geoml plugin
// (bezier_curve's return type) so both translation units agree on the conversion.
namespace sol {
    template <typename T>
    struct unique_usertype_traits<opencascade::handle<T>> {
        using type = T;
        using actual_type = opencascade::handle<T>;
        static const bool value = true;

        static bool is_null(const actual_type& ptr) {
            return ptr.IsNull();
        }

        static type* get(const actual_type& ptr) {
            return ptr.get();
        }
    };
}
