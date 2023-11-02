#include <grunk/grunk.hpp>
#include <iostream>

int main()
{
	auto& plugins = grunk::get_plugin_registry();
	plugins.prepend_path("/home/reis_at/.grunk/.conan/data/grocc/0.1.1/_/_/package/d32037acca450eabbebea80e73484bb01a02d6fa/lib");
	plugins.prepend_path("/home/reis_at/.grunk/.conan/data/geo/0.2.0/_/_/package/e2b0526526650ed92e3900bb2fd52b392dcac30a/lib");
	plugins.load_all();
	plugins.print_plugins();

//std::cout << reflect::help("geo") << "\n";

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
/////////																		   /////////
/////////					        // load_carrier //							   /////////
/////////							///////////////////							   /////////
/////////																		   /////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


// define constants:
	auto n_0 = grunk::Feature("n_0", "int", 0);
	auto n_1 = grunk::Feature("n_1", "int", 1);
	auto n_2 = grunk::Feature("n_2", "int", 2);
	auto n_3 = grunk::Feature("n_3", "int", 3);
	auto n_4 = grunk::Feature("n_4", "int", 4);
	auto n_5 = grunk::Feature("n_5", "int", 5);
	auto n_6 = grunk::Feature("n_6", "int", 6);
	auto n_7 = grunk::Feature("n_7", "int", 7);
	auto n_8 = grunk::Feature("n_8", "int", 8);
	auto n_9 = grunk::Feature("n_9", "int", 9);
	auto n_10 = grunk::Feature("n_10", "int", 10);

    auto tol = grunk::Feature("tol", "double", 0.1);

// create points (corner points of a rectangle of width rect_width and hight rect_hight):
    auto o_corner1 = grunk::Feature("o_corner1", "grocc::gp_Pnt", 0.0, 0.0, 0.0);
    auto vec_y = grunk::Feature("vec_y", "grocc::gp_Vec", 0.0, 1.0, 0.0);
    auto rect_width = grunk::Feature("rect_width", "double", 2.0);

    auto o_corner2 = grunk::action("o_corner2", "geo::move", o_corner1, vec_y, rect_width).output();

    auto vec_z = grunk::Feature("vec_z", "grocc::gp_Vec", 0.0, 0.0, 1.0);
    auto rect_hight = grunk::Feature("rect_hight", "double", 3.0);

    auto o_corner3 = grunk::action("o_corner3", "geo::move", o_corner2, vec_z, rect_hight).output();

    auto o_corner4 = grunk::action("o_corner4", "geo::move", o_corner1, vec_z, rect_hight).output();

// define a surface defined by 4 points:
    auto o_corner_points = grunk::vec("o_corner_points", o_corner1, o_corner2, o_corner3, o_corner4);

    auto o_four_point_srf = grunk::action("o_four_point_srf", "geo::surface_from_4_points", o_corner_points).output();

// now, extrude this surface:

// define the extrusion direction:
    auto extr_dir = grunk::Feature("extr_dir", "grocc::gp_Vec", 1.0, 0.0, 0.0);
    auto box_length = grunk::Feature("box_length", "double", 10.0);

    auto extr_vec = grunk::action("extr_vec", "geo::scale_vector", extr_dir, box_length).output();

// create the extrusion:
    auto o_extruded_shape = grunk::action("o_extruded_shape", "geo::extrude_surface", o_four_point_srf, extr_vec).output();

// now, position spheres to create cutouts later:
    auto sphere_position = grunk::Feature("sphere_position", "double", 3.0);

    auto center_1 = grunk::action("center_1", "geo::move", o_corner1, extr_dir, sphere_position).output();
    auto center_2 = grunk::action("center_2", "geo::move", o_corner2, extr_dir, sphere_position).output();
    auto center_3 = grunk::action("center_3", "geo::move", o_corner3, extr_dir, sphere_position).output();
    auto center_4 = grunk::action("center_4", "geo::move", o_corner4, extr_dir, sphere_position).output();

// now, create the spheres:
    auto sphere_radius = grunk::Feature("sphere_radius", "double", 0.2);

    auto sphere_shape_1 = grunk::action("sphere_shape_1", "geo::sphere", center_1, sphere_radius).output();
    auto sphere_shape_2 = grunk::action("sphere_shape_2", "geo::sphere", center_2, sphere_radius).output();
    auto sphere_shape_3 = grunk::action("sphere_shape_3", "geo::sphere", center_3, sphere_radius).output();
    auto sphere_shape_4 = grunk::action("sphere_shape_4", "geo::sphere", center_4, sphere_radius).output();

// now, make fillets:
/*std::initializer_list<int> index_list {1,2,5,8};

TopoDS_Shape rounded_box = geoml::make_fillet(o_extruded_shape, index_list, 0.05);*/

    auto index_list = grunk::vec("index_list", n_1, n_2, n_5, n_8);
    auto radius_fillet = grunk::Feature("radius_fillet", "double", 0.05);

    auto rounded_box = grunk::action("rounded_box", "geo::make_fillet", o_extruded_shape, index_list, radius_fillet).output();


// now, subtract the spheres from rounded_box: // create a geoml funktion that executes the following calculation.
    auto list_of_sphere_shapes = grunk::vec("list_of_spheres", sphere_shape_1, sphere_shape_2, sphere_shape_3, sphere_shape_4);
    auto sphere_compound = grunk::action("sphere_compound", "geo::make_compound", list_of_sphere_shapes).output();

    auto cut_shape = grunk::action("cut_shape", "geo::cut_away", rounded_box, sphere_compound).output();




// extract boundary as shell
    auto shell_prelim = grunk::action("shell_prelim", "geo::make_shell", cut_shape).output();

// remove faces from shell_prelim (indices found by trial and error)

    auto face_to_remove_1 = grunk::action("face_to_remove_1", "geo::internal::GetFace", cut_shape, n_1).output();
    auto face_to_remove_2 = grunk::action("face_to_remove_2", "geo::internal::GetFace", cut_shape, n_5).output();

    auto exclude_faces = grunk::vec("exclude_faces", face_to_remove_1, face_to_remove_2);
	
    auto load_carrier = grunk::action("final_shell_shape", "geo::remove_faces", shell_prelim, exclude_faces).output();

// write grunk recipe:
	grunk::write("load_carrier.grr", load_carrier);

// write final_shell_shape as stp file:
	auto load_carrier_name_stp = grunk::Feature("load_carrier_name_stp", "String", "load_carrier.stp");
	auto out = grunk::action("out", "geo::writeGeomEntityToStepFile", load_carrier, load_carrier_name_stp).output();
	out.value();

std::cout << "end of main" << std::endl;

return 0;
}
