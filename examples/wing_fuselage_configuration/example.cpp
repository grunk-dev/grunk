#include <grunk/grunk.hpp>
#include <iostream>

int main()
{
	auto& plugins = grunk::get_plugin_registry();
	plugins.prepend_path("/home/reis_at/.grunk/.conan/data/grocc/0.1.1/_/_/package/d32037acca450eabbebea80e73484bb01a02d6fa/lib");
	plugins.prepend_path("/home/reis_at/.grunk/.conan/data/geo/0.2.0/_/_/package/e2b0526526650ed92e3900bb2fd52b392dcac30a/lib");
	plugins.load_all();
	plugins.print_plugins();




////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
/////////																		   /////////
/////////					        // Wing-Fuselage-Configuration //			   /////////
/////////							/////////////////////////////////			   /////////
/////////																		   /////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

// parameter to vary for demonstration purposes:
// third component of start_point_profile 
// thir component of end_point_profile
// end_point_arc
// end_point_arc_cap
// move_vec_o_wing
// move_factor_o_wing

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
	auto n_11 = grunk::Feature("n_10", "int", 11);
	auto n_12 = grunk::Feature("n_10", "int", 12);
	auto n_13 = grunk::Feature("n_10", "int", 13);
	auto n_14 = grunk::Feature("n_10", "int", 14);
	auto n_15 = grunk::Feature("n_10", "int", 15);
	auto n_16 = grunk::Feature("n_10", "int", 16);
	auto n_17 = grunk::Feature("n_10", "int", 17);
	auto n_18 = grunk::Feature("n_10", "int", 18);
	auto n_19 = grunk::Feature("n_10", "int", 19);



////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////  
//////////   														
//////////	Fuselage 	 
//////////
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////  


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//////////	Mid part
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

// define profile curve for the revolving surface (cylinder):

// control points:
	auto start_point_profile = grunk::Feature("start_point_profile", "grocc::gp_Pnt", 7000., 0., 2000.); // original: 7000., 0., 2000. 
	auto end_point_profile = grunk::Feature("end_point_profile", "grocc::gp_Pnt", 23000., 0., 2000.); // original: 23000., 0., 2000.

	auto profile_points_cylinder_arg_1 = grunk::Feature("profile_points_cylinder_arg_1", "int", 1);
	auto profile_points_cylinder_arg_2 = grunk::Feature("profile_points_cylinder_arg_2", "int", 2);

	auto profile_points_cylinder = grunk::script(
		{
			{"grocc::TColgp_Array1OfPnt", {"profile_points_cylinder"}, {profile_points_cylinder_arg_1, profile_points_cylinder_arg_2}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"profile_points_cylinder", n_1, start_point_profile}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"profile_points_cylinder", n_2, end_point_profile}},
		},
		{"profile_points_cylinder"}
	).output();

// degree:
	auto degree_profile = grunk::Feature("degree_profile", "int", 1);

// weights:
	auto weights_profile_arg_1 = grunk::Feature("weights_profile_arg_1", "int", 1);
	auto weights_profile_arg_2 = grunk::Feature("weights_profile_arg_2", "int", 2);

	auto weights_profile_weight_1 = grunk::Feature("weights_profile_weight_1", "double", 1.0);
	auto weights_profile_weight_2 = grunk::Feature("weights_profile_weight_2", "double", 1.0);

	auto weights_profile = grunk::script(
		{
			{"grocc::TColStd_Array1OfReal", {"weights_profile"}, {weights_profile_arg_1, weights_profile_arg_2}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"weights_profile", n_1, weights_profile_weight_1}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"weights_profile", n_2, weights_profile_weight_2}},
		},
		{"weights_profile"}
	).output();

// knots:
	auto knots_profile_arg_1 = grunk::Feature("knots_profile_arg_1", "int", 1);
	auto knots_profile_arg_2 = grunk::Feature("knots_profile_arg_2", "int", 2);

	auto knots_profile_val_1 = grunk::Feature("knots_profile_val_1", "double", 0.0);
	auto knots_profile_val_2 = grunk::Feature("knots_profile_val_2", "double", 1.0);

	auto knots_profile = grunk::script(
		{
			{"grocc::TColStd_Array1OfReal", {"knots_profile"}, {knots_profile_arg_1, knots_profile_arg_2}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"knots_profile", n_1, knots_profile_val_1}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"knots_profile", n_2, knots_profile_val_2}},
		},
		{"knots_profile"}
	).output();

// multiplicities: 
	auto mults_profile_1_arg_1 = grunk::Feature("mults_profile_arg_1", "int", 1);
	auto mults_profile_2_arg_2 = grunk::Feature("mults_profile_arg_2", "int", 2);

	auto mults_profile_val_1 = grunk::Feature("mults_profile_val_1", "int", 2);
	auto mults_profile_val_2 = grunk::Feature("mults_profile_val_2", "int", 2);

	auto mults_profile = grunk::script(
		{
			{"grocc::TColStd_Array1OfInteger", {"mults_profile"}, {mults_profile_1_arg_1, mults_profile_2_arg_2}},
			{"grocc::TColStd_Array1OfInteger::SetValue", {}, {"mults_profile", n_1, mults_profile_val_1}},
			{"grocc::TColStd_Array1OfInteger::SetValue", {}, {"mults_profile", n_2, mults_profile_val_2}},
		},
		{"mults_profile"}
	).output();

// create the profile curve:
	auto profile_curve_cylinder = grunk::action("profile_curve_cylinder", "geo::b_spline_curve", profile_points_cylinder, 
												weights_profile, knots_profile, mults_profile, degree_profile).output(); 

// use this profile curve to define the cylinder:
	auto rot_axis_Pnt = grunk::Feature("rot_axis_Pnt", "grocc::gp_Pnt", 0., 0., 0.);
	auto rot_axis_Dir = grunk::Feature("rot_axis_Dir", "grocc::gp_Dir", 1., 0., 0.);

	auto rot_axis = grunk::action("rot_axis", "grocc::gp_Ax1", rot_axis_Pnt, rot_axis_Dir).output();

	auto fuselage_cylinder = grunk::action("fuselage_cylinder", "geo::revolving_surface", profile_curve_cylinder, rot_axis).output();


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//////////	rear part
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////


// extract the 2nd control point column (U-direction) of fuselage_cylinder: (i.e. the control points
// of the rear edge):
	auto cp_column_second_arg = grunk::Feature("cp_column_second_arg", "int", 1);
	auto cp_column_third_arg = grunk::Feature("cp_column_third_arg", "int", 2);

	auto cp_column = grunk::action("cp_column", "geo::extract_control_point_column_row", fuselage_cylinder, cp_column_second_arg, cp_column_third_arg).output();

// now, create two copies of the periodic rear control points cp_column and move them
// in x-direction of the world coordinate system, relative to each other:

// define the direction vector:
	auto move_vec = grunk::Feature("move_vec", "grocc::gp_Vec", 1.0, 0.0, 0.0);

// define the factor multiplied to this vector for the first move:
	auto move_factor_1 = grunk::Feature("move_factor_1", "double", 2500.0);

// move the control points cp_column:
	auto moved_cp_points_1 = grunk::action("moved_cp_points_1", "geo::move", cp_column, move_vec, move_factor_1).output();

// move the control points moved_cp_points_1:

// define the factor multiplied to move_vec for the second (relative) move:
	auto move_factor_2 = grunk::Feature("move_factor_2", "double", 3000.0);

	auto moved_cp_points_2 = grunk::action("moved_cp_points_2", "geo::move", moved_cp_points_1, move_vec, move_factor_2).output();

// next, define two hand picked 1-d collections of points; these will be the two last
// control point columns of the fuselage rear surface:

// second last column of control points (periodic case) - hand selected:
	auto sl_pt_1 = grunk::Feature("sl_pt_1", "grocc::gp_Pnt", 31500.0, 0.0, 1992.0);
 	auto sl_pt_2 = grunk::Feature("sl_pt_2", "grocc::gp_Pnt", 31500.0, -3464.0, 1992.0);
	auto sl_pt_3 = grunk::Feature("sl_pt_3", "grocc::gp_Pnt", 31500.0, -1732.0, -679.0);
	auto sl_pt_4 = grunk::Feature("sl_pt_4", "grocc::gp_Pnt", 31500.0, 0.0, -3350.0);
	auto sl_pt_5 = grunk::Feature("sl_pt_5", "grocc::gp_Pnt", 31500.0, 1732.0, -679.0);
	auto sl_pt_6 = grunk::Feature("sl_pt_6", "grocc::gp_Pnt", 31500.0, 3464.0, 1992.0);	

	auto moved_cp_points_3_arg_1 = grunk::Feature("moved_cp_points_3_arg_1", "int", 1);
	auto moved_cp_points_3_arg_2 = grunk::Feature("moved_cp_points_3_arg_2", "int", 6);

	auto moved_cp_points_3 = grunk::script(
		{
			{"grocc::TColgp_Array1OfPnt", {"moved_cp_points_3"}, {moved_cp_points_3_arg_1, moved_cp_points_3_arg_2}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"moved_cp_points_3", n_1, sl_pt_1}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"moved_cp_points_3", n_2, sl_pt_2}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"moved_cp_points_3", n_3, sl_pt_3}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"moved_cp_points_3", n_4, sl_pt_4}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"moved_cp_points_3", n_5, sl_pt_5}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"moved_cp_points_3", n_6, sl_pt_6}},
		},
		{"moved_cp_points_3"}
	).output();

// last column of control points (periodic case):
	auto l_pt_1 = grunk::Feature("l_pt_1", "grocc::gp_Pnt", 37500.0, 0.0, 1337.0);
	auto l_pt_2 = grunk::Feature("l_pt_2", "grocc::gp_Pnt", 37500.0, -631.0, 1337.0);
	auto l_pt_3 = grunk::Feature("l_pt_3", "grocc::gp_Pnt", 37500.0, -316.0, 795.0);
	auto l_pt_4 = grunk::Feature("l_pt_4", "grocc::gp_Pnt", 37500.0, 0.0, 254.0);
	auto l_pt_5 = grunk::Feature("l_pt_5", "grocc::gp_Pnt", 37500.0, 316.0, 795.0);
	auto l_pt_6 = grunk::Feature("l_pt_6", "grocc::gp_Pnt", 37500.0, 631.0, 1337.0);

	auto moved_cp_points_4_arg_1 = grunk::Feature("moved_cp_points_4_arg_1", "int", 1);
	auto moved_cp_points_4_arg_2 = grunk::Feature("moved_cp_points_4_arg_2", "int", 6);

	auto moved_cp_points_4 = grunk::script(
		{
			{"grocc::TColgp_Array1OfPnt", {"moved_cp_points_4"}, {moved_cp_points_4_arg_1, moved_cp_points_4_arg_2}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"moved_cp_points_4", n_1, l_pt_1}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"moved_cp_points_4", n_2, l_pt_2}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"moved_cp_points_4", n_3, l_pt_3}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"moved_cp_points_4", n_4, l_pt_4}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"moved_cp_points_4", n_5, l_pt_5}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"moved_cp_points_4", n_6, l_pt_6}},
		},
		{"moved_cp_points_4"}
	).output();

// create a control point net consisting of the defined four columns:
	auto my_point_lists = grunk::vec("my_point_lists", cp_column, moved_cp_points_1, moved_cp_points_2, moved_cp_points_3, moved_cp_points_4);

// the point arrays should be the columns of the control point net:
	auto rear_srf_cp_net_second_arg = grunk::Feature("rear_srf_cp_net_second_arg", "int", 2);

	auto rear_srf_cp_net = grunk::action("rear_srf_cp_net", "geo::create_point_net_from_arrays", my_point_lists, rear_srf_cp_net_second_arg).output();

// degree:
	auto degree_U = grunk::Feature("degree_U", "int", 2);
	auto degree_V = grunk::Feature("degree_V", "int", 4);

// define the weight net for the fuselage rear surface:
	auto rear_srf_weight_net_arg_1 = grunk::Feature("rear_srf_weight_net_arg_1", "int", 1);	
	auto rear_srf_weight_net_arg_2 = grunk::action("rear_srf_weight_net_arg_2", "grocc::TColgp_Array2OfPnt::ColLength", rear_srf_cp_net).output();                
	auto rear_srf_weight_net_arg_3 = grunk::Feature("rear_srf_weight_net_arg_3", "int", 1);
	auto rear_srf_weight_net_arg_4 = grunk::action("rear_srf_weight_net_arg_4", "grocc::TColgp_Array2OfPnt::RowLength", rear_srf_cp_net).output(); 

	auto rear_srf_weight_net_val_1_1 = grunk::Feature("rear_srf_weight_net_val_1_1", "double", 1.0);
	auto rear_srf_weight_net_val_2_1 = grunk::Feature("rear_srf_weight_net_val_2_1", "double", 0.5);
	auto rear_srf_weight_net_val_3_1 = grunk::Feature("rear_srf_weight_net_val_3_1", "double", 1.0);
	auto rear_srf_weight_net_val_4_1 = grunk::Feature("rear_srf_weight_net_val_4_1", "double", 0.5);
	auto rear_srf_weight_net_val_5_1 = grunk::Feature("rear_srf_weight_net_val_5_1", "double", 1.0);
	auto rear_srf_weight_net_val_6_1 = grunk::Feature("rear_srf_weight_net_val_6_1", "double", 0.5);

	auto rear_srf_weight_net_val_1_2 = grunk::Feature("rear_srf_weight_net_val_1_2", "double", 1.0);
	auto rear_srf_weight_net_val_2_2 = grunk::Feature("rear_srf_weight_net_val_2_2", "double", 0.5);
	auto rear_srf_weight_net_val_3_2 = grunk::Feature("rear_srf_weight_net_val_3_2", "double", 1.0);
	auto rear_srf_weight_net_val_4_2 = grunk::Feature("rear_srf_weight_net_val_4_2", "double", 0.5);
	auto rear_srf_weight_net_val_5_2 = grunk::Feature("rear_srf_weight_net_val_5_2", "double", 1.0);
	auto rear_srf_weight_net_val_6_2 = grunk::Feature("rear_srf_weight_net_val_6_2", "double", 0.5);

	auto rear_srf_weight_net_val_1_3 = grunk::Feature("rear_srf_weight_net_val_1_3", "double", 1.0);
	auto rear_srf_weight_net_val_2_3 = grunk::Feature("rear_srf_weight_net_val_2_3", "double", 0.5);
	auto rear_srf_weight_net_val_3_3 = grunk::Feature("rear_srf_weight_net_val_3_3", "double", 1.0);
	auto rear_srf_weight_net_val_4_3 = grunk::Feature("rear_srf_weight_net_val_4_3", "double", 0.5);
	auto rear_srf_weight_net_val_5_3 = grunk::Feature("rear_srf_weight_net_val_5_3", "double", 1.0);
	auto rear_srf_weight_net_val_6_3 = grunk::Feature("rear_srf_weight_net_val_6_3", "double", 0.5);

	auto rear_srf_weight_net_val_1_4 = grunk::Feature("rear_srf_weight_net_val_1_4", "double", 1.0);
	auto rear_srf_weight_net_val_2_4 = grunk::Feature("rear_srf_weight_net_val_2_4", "double", 0.5);
	auto rear_srf_weight_net_val_3_4 = grunk::Feature("rear_srf_weight_net_val_3_4", "double", 1.0);
	auto rear_srf_weight_net_val_4_4 = grunk::Feature("rear_srf_weight_net_val_4_4", "double", 0.5);
	auto rear_srf_weight_net_val_5_4 = grunk::Feature("rear_srf_weight_net_val_5_4", "double", 1.0);
	auto rear_srf_weight_net_val_6_4 = grunk::Feature("rear_srf_weight_net_val_6_4", "double", 0.5);

	auto rear_srf_weight_net_val_1_5 = grunk::Feature("rear_srf_weight_net_val_1_5", "double", 1.0);
	auto rear_srf_weight_net_val_2_5 = grunk::Feature("rear_srf_weight_net_val_2_5", "double", 0.5);
	auto rear_srf_weight_net_val_3_5 = grunk::Feature("rear_srf_weight_net_val_3_5", "double", 1.0);
	auto rear_srf_weight_net_val_4_5 = grunk::Feature("rear_srf_weight_net_val_4_5", "double", 0.5);
	auto rear_srf_weight_net_val_5_5 = grunk::Feature("rear_srf_weight_net_val_5_5", "double", 1.0);
	auto rear_srf_weight_net_val_6_5 = grunk::Feature("rear_srf_weight_net_val_6_5", "double", 0.5);

	auto rear_srf_weight_net = grunk::script(
		{
			{"grocc::TColStd_Array2OfReal", {"rear_srf_weight_net"}, {rear_srf_weight_net_arg_1, rear_srf_weight_net_arg_2, rear_srf_weight_net_arg_3, rear_srf_weight_net_arg_4}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_1, n_1, rear_srf_weight_net_val_1_1}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_2, n_1, rear_srf_weight_net_val_2_1}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_3, n_1, rear_srf_weight_net_val_3_1}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_4, n_1, rear_srf_weight_net_val_4_1}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_5, n_1, rear_srf_weight_net_val_5_1}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_6, n_1, rear_srf_weight_net_val_6_1}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_1, n_2, rear_srf_weight_net_val_1_2}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_2, n_2, rear_srf_weight_net_val_2_2}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_3, n_2, rear_srf_weight_net_val_3_2}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_4, n_2, rear_srf_weight_net_val_4_2}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_5, n_2, rear_srf_weight_net_val_5_2}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_6, n_2, rear_srf_weight_net_val_6_2}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_1, n_3, rear_srf_weight_net_val_1_3}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_2, n_3, rear_srf_weight_net_val_2_3}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_3, n_3, rear_srf_weight_net_val_3_3}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_4, n_3, rear_srf_weight_net_val_4_3}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_5, n_3, rear_srf_weight_net_val_5_3}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_6, n_3, rear_srf_weight_net_val_6_3}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_1, n_4, rear_srf_weight_net_val_1_4}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_2, n_4, rear_srf_weight_net_val_2_4}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_3, n_4, rear_srf_weight_net_val_3_4}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_4, n_4, rear_srf_weight_net_val_4_4}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_5, n_4, rear_srf_weight_net_val_5_4}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_6, n_4, rear_srf_weight_net_val_6_4}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_1, n_5, rear_srf_weight_net_val_1_5}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_2, n_5, rear_srf_weight_net_val_2_5}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_3, n_5, rear_srf_weight_net_val_3_5}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_4, n_5, rear_srf_weight_net_val_4_5}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_5, n_5, rear_srf_weight_net_val_5_5}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_weight_net", n_6, n_5, rear_srf_weight_net_val_6_5}},
		},
		{"rear_srf_weight_net"}
	).output();

// set the U-knots 
	auto knots_U_arg_1 = grunk::Feature("knots_U_arg_1", "int", 1);
	auto knots_U_arg_2 = grunk::Feature("knots_U_arg_2", "int", 4);

	auto knots_U_val_1 = grunk::Feature("knots_U_val_1", "double", 0.0);
	auto knots_U_val_2 = grunk::Feature("knots_U_val_2", "double", 1.0);
	auto knots_U_val_3 = grunk::Feature("knots_U_val_3", "double", 2.0);
	auto knots_U_val_4 = grunk::Feature("knots_U_val_4", "double", 3.0);

	auto knots_U = grunk::script(
		{
			{"grocc::TColStd_Array1OfReal", {"knots_U"}, {knots_U_arg_1, knots_U_arg_2}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"knots_U", n_1, knots_U_val_1}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"knots_U", n_2, knots_U_val_2}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"knots_U", n_3, knots_U_val_3}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"knots_U", n_4, knots_U_val_4}},
		},
		{"knots_U"}
	).output();

// set the V-knots 
	auto knots_V_arg_1 = grunk::Feature("knots_V_arg_1", "int", 1);
	auto knots_V_arg_2 = grunk::Feature("knots_V_arg_2", "int", 2);

	auto knots_V_val_1 = grunk::Feature("knots_V_val_1", "double", 0.0);
	auto knots_V_val_2 = grunk::Feature("knots_V_val_2", "double", 1.0);

	auto knots_V = grunk::script(
		{
			{"grocc::TColStd_Array1OfReal", {"knots_V"}, {knots_V_arg_1, knots_V_arg_2}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"knots_V", n_1, knots_V_val_1}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"knots_V", n_2, knots_V_val_2}},
		},
		{"knots_V"}
	).output();

// multiplicities U-direction: 
	auto mults_U_arg_1 = grunk::Feature("mults_U_arg_1", "int", 1);
	auto mults_U_arg_2 = grunk::Feature("mults_U_arg_2", "int", 4);

	auto mults_U_val_1 = grunk::Feature("mults_U_val_1", "double", 2);
	auto mults_U_val_2 = grunk::Feature("mults_U_val_2", "double", 2);
	auto mults_U_val_3 = grunk::Feature("mults_U_val_3", "double", 2);
	auto mults_U_val_4 = grunk::Feature("mults_U_val_4", "double", 2);

	auto mults_U = grunk::script(
		{
			{"grocc::TColStd_Array1OfInteger", {"mults_U"}, {mults_U_arg_1, mults_U_arg_2}},
			{"grocc::TColStd_Array1OfInteger::SetValue", {}, {"mults_U", n_1, mults_U_val_1}},
			{"grocc::TColStd_Array1OfInteger::SetValue", {}, {"mults_U", n_2, mults_U_val_2}},
			{"grocc::TColStd_Array1OfInteger::SetValue", {}, {"mults_U", n_3, mults_U_val_3}},
			{"grocc::TColStd_Array1OfInteger::SetValue", {}, {"mults_U", n_4, mults_U_val_4}},
		},
		{"mults_U"}
	).output();

// multiplicities V-direction: 
	auto mults_V_arg_1 = grunk::Feature("mults_V_arg_1", "int", 1);
	auto mults_V_arg_2 = grunk::Feature("mults_V_arg_2", "int", 2);

	auto mults_V_val_1 = grunk::Feature("mults_V_val_1", "int", 5);
	auto mults_V_val_2 = grunk::Feature("mults_V_val_2", "int", 5);

	auto mults_V = grunk::script(
		{
			{"grocc::TColStd_Array1OfInteger", {"mults_V"}, {mults_V_arg_1, mults_V_arg_2}},
			{"grocc::TColStd_Array1OfInteger::SetValue", {}, {"mults_V", n_1, mults_V_val_1}},
			{"grocc::TColStd_Array1OfInteger::SetValue", {}, {"mults_V", n_2, mults_V_val_2}},
		},
		{"mults_V"}
	).output();

// boolean arguments:
	auto periodic_U = grunk::Feature("periodic_U", "bool", true);

// define the rear surface:
	auto rear_surface = grunk::action("rear_surface", "geo::b_spline_surface",  rear_srf_cp_net, 
                 																rear_srf_weight_net, 
                 																knots_U, 
                 																knots_V, 
                 																mults_U, 
                 																mults_V, 
                 																degree_U, 
                 																degree_V, 
                 																periodic_U ).output(); 

// rear cap:

// first edge:

// the first edge is given by the last control point row of the rear surface: moved_cp_points_4
// the second edge by 6 overlapping points cp_points_overl. We work in the periodic case.
	auto ovl_pt_arg_val_1 = grunk::action("ovl_pt_arg_1_val_1", "grocc::TColgp_Array1OfPnt::Value", moved_cp_points_4, n_1).output();
	auto ovl_pt_arg_val_2 = grunk::action("ovl_pt_arg_1_val_2", "grocc::TColgp_Array1OfPnt::Value", moved_cp_points_4, n_3).output();
	auto ovl_pt_arg_val_3 = grunk::action("ovl_pt_arg_1_val_3", "grocc::TColgp_Array1OfPnt::Value", moved_cp_points_4, n_5).output();

	auto ovl_pt_arg_val_1_x = grunk::action("ovl_pt_arg_val_1_x", "grocc::gp_Pnt::X", ovl_pt_arg_val_1).output();
	auto ovl_pt_arg_val_2_x = grunk::action("ovl_pt_arg_val_2_x", "grocc::gp_Pnt::X", ovl_pt_arg_val_2).output();
	auto ovl_pt_arg_val_3_x = grunk::action("ovl_pt_arg_val_3_x", "grocc::gp_Pnt::X", ovl_pt_arg_val_3).output();

	auto ovl_pt_arg_val_1_y = grunk::action("ovl_pt_arg_val_1_y", "grocc::gp_Pnt::Y", ovl_pt_arg_val_1).output();
	auto ovl_pt_arg_val_2_y = grunk::action("ovl_pt_arg_val_2_y", "grocc::gp_Pnt::Y", ovl_pt_arg_val_2).output();
	auto ovl_pt_arg_val_3_y = grunk::action("ovl_pt_arg_val_3_y", "grocc::gp_Pnt::Y", ovl_pt_arg_val_3).output();

	auto ovl_pt_arg_val_1_z = grunk::action("ovl_pt_arg_val_1_z", "grocc::gp_Pnt::Z", ovl_pt_arg_val_1).output();
	auto ovl_pt_arg_val_2_z = grunk::action("ovl_pt_arg_val_2_z", "grocc::gp_Pnt::Z", ovl_pt_arg_val_2).output();
	auto ovl_pt_arg_val_3_z = grunk::action("ovl_pt_arg_val_3_z", "grocc::gp_Pnt::Z", ovl_pt_arg_val_3).output();

	auto ovl_pt_expr_x = grunk::expression("ovl_pt_expr_x", "(ovl_pt_arg_val_1_x + ovl_pt_arg_val_2_x + ovl_pt_arg_val_3_x)/3", ovl_pt_arg_val_1_x, ovl_pt_arg_val_2_x, ovl_pt_arg_val_3_x);
	auto ovl_pt_expr_y = grunk::expression("ovl_pt_expr_y", "(ovl_pt_arg_val_1_y + ovl_pt_arg_val_2_y + ovl_pt_arg_val_3_y)/3", ovl_pt_arg_val_1_y, ovl_pt_arg_val_2_y, ovl_pt_arg_val_3_y);
	auto ovl_pt_expr_z = grunk::expression("ovl_pt_expr_z", "(ovl_pt_arg_val_1_z + ovl_pt_arg_val_2_z + ovl_pt_arg_val_3_z)/3", ovl_pt_arg_val_1_z, ovl_pt_arg_val_2_z, ovl_pt_arg_val_3_z);

	auto ovl_pt = grunk::action("ovl_pt", "grocc::gp_Pnt", ovl_pt_expr_x, ovl_pt_expr_y, ovl_pt_expr_z).output();

	auto ovl_cp_points_arg_1 = grunk::Feature("ovl_cp_points_arg_1", "int", 1);
	auto ovl_cp_points_arg_2 = grunk::Feature("ovl_cp_points_arg_2", "int", 6);

	auto ovl_cp_points = grunk::script(
		{
			{"grocc::TColgp_Array1OfPnt", {"ovl_cp_points"}, {ovl_cp_points_arg_1, ovl_cp_points_arg_2}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"ovl_cp_points", n_1, ovl_pt}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"ovl_cp_points", n_2, ovl_pt}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"ovl_cp_points", n_3, ovl_pt}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"ovl_cp_points", n_4, ovl_pt}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"ovl_cp_points", n_5, ovl_pt}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"ovl_cp_points", n_6, ovl_pt}},
		},
		{"ovl_cp_points"}
	).output();

// create a control point net consisting of the defined two columns:
	auto my_point_lists_rear_cap = grunk::vec("my_point_lists_rear_cap", moved_cp_points_4, ovl_cp_points);

// the point arrays should be the columns of the control point net:
	auto rear_srf_cap_cp_net = grunk::action("rear_srf_cap_cp_net", "geo::create_point_net_from_arrays", my_point_lists_rear_cap, n_2).output();

	auto rear_srf_cap_weight_net_arg_1 = grunk::Feature("rear_srf_cap_weight_net_arg_1", "int", 1);	
	auto rear_srf_cap_weight_net_arg_2 = grunk::action("rear_srf_cap_weight_net_arg_2", "grocc::TColgp_Array2OfPnt::ColLength", rear_srf_cap_cp_net).output();                
	auto rear_srf_cap_weight_net_arg_3 = grunk::Feature("rear_srf_cap_weight_net_arg_3", "int", 1);
	auto rear_srf_cap_weight_net_arg_4 = grunk::action("rear_srf_cap_weight_net_arg_4", "grocc::TColgp_Array2OfPnt::RowLength", rear_srf_cap_cp_net).output(); 

	auto rear_srf_cap_weight_net_val_1_1 = grunk::Feature("rear_srf_cap_weight_net_1_1", "double", 1.0);
	auto rear_srf_cap_weight_net_val_2_1 = grunk::Feature("rear_srf_cap_weight_net_2_1", "double", 0.5);
	auto rear_srf_cap_weight_net_val_3_1 = grunk::Feature("rear_srf_cap_weight_net_3_1", "double", 1.0);
	auto rear_srf_cap_weight_net_val_4_1 = grunk::Feature("rear_srf_cap_weight_net_4_1", "double", 0.5);
	auto rear_srf_cap_weight_net_val_5_1 = grunk::Feature("rear_srf_cap_weight_net_5_1", "double", 1.0);
	auto rear_srf_cap_weight_net_val_6_1 = grunk::Feature("rear_srf_cap_weight_net_6_1", "double", 0.5);

	auto rear_srf_cap_weight_net_val_1_2 = grunk::Feature("rear_srf_cap_weight_net_1_2", "double", 1.0);
	auto rear_srf_cap_weight_net_val_2_2 = grunk::Feature("rear_srf_cap_weight_net_2_2", "double", 0.5);
	auto rear_srf_cap_weight_net_val_3_2 = grunk::Feature("rear_srf_cap_weight_net_3_2", "double", 1.0);
	auto rear_srf_cap_weight_net_val_4_2 = grunk::Feature("rear_srf_cap_weight_net_4_2", "double", 0.5);
	auto rear_srf_cap_weight_net_val_5_2 = grunk::Feature("rear_srf_cap_weight_net_5_2", "double", 1.0);
	auto rear_srf_cap_weight_net_val_6_2 = grunk::Feature("rear_srf_cap_weight_net_6_2", "double", 0.5);

	auto rear_srf_cap_weight_net = grunk::script(
		{
			{"grocc::TColStd_Array2OfReal", {"rear_srf_cap_weight_net"}, {rear_srf_cap_weight_net_arg_1, rear_srf_cap_weight_net_arg_2, rear_srf_cap_weight_net_arg_3, rear_srf_cap_weight_net_arg_4}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_cap_weight_net", n_1, n_1, rear_srf_cap_weight_net_val_1_1}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_cap_weight_net", n_2, n_1, rear_srf_cap_weight_net_val_2_1}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_cap_weight_net", n_3, n_1, rear_srf_cap_weight_net_val_3_1}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_cap_weight_net", n_4, n_1, rear_srf_cap_weight_net_val_4_1}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_cap_weight_net", n_5, n_1, rear_srf_cap_weight_net_val_5_1}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_cap_weight_net", n_6, n_1, rear_srf_cap_weight_net_val_6_1}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_cap_weight_net", n_1, n_2, rear_srf_cap_weight_net_val_1_2}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_cap_weight_net", n_2, n_2, rear_srf_cap_weight_net_val_2_2}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_cap_weight_net", n_3, n_2, rear_srf_cap_weight_net_val_3_2}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_cap_weight_net", n_4, n_2, rear_srf_cap_weight_net_val_4_2}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_cap_weight_net", n_5, n_2, rear_srf_cap_weight_net_val_5_2}},
			{"grocc::TColStd_Array2OfReal::SetValue", {}, {"rear_srf_cap_weight_net", n_6, n_2, rear_srf_cap_weight_net_val_6_2}},
		},
		{"rear_srf_cap_weight_net"}
	).output();

// the knots, mults in U direction as in the rear fuselage surface:
// the V direction needs some adjustment:
	auto cap_degree_V = grunk::Feature("cap_degree_V", "int", 1);

// set the V-knots (rows):
	auto cap_knots_V_arg_1 = grunk::Feature("cap_knots_V_arg_1", "int", 1);
	auto cap_knots_V_arg_2 = grunk::Feature("cap_knots_V_arg_2", "int", 2);

	auto cap_knots_V_val_1 = grunk::Feature("cap_knots_V_val_1", "double", 0.0);
	auto cap_knots_V_val_2 = grunk::Feature("cap_knots_V_val_2", "double", 1.0);

	auto cap_knots_V = grunk::script(
		{
			{"grocc::TColStd_Array1OfReal", {"cap_knots_V"}, {cap_knots_V_arg_1, cap_knots_V_arg_2}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"cap_knots_V", n_1, cap_knots_V_val_1}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"cap_knots_V", n_2, cap_knots_V_val_2}},
		},
		{"cap_knots_V"}
	).output();

// multiplicities V-direction: 
	auto cap_mults_V_arg_1 = grunk::Feature("cap_mults_V_arg_1", "int", 1);
	auto cap_mults_V_arg_2 = grunk::Feature("cap_mults_V_arg_2", "int", 2);

	auto cap_mults_V_val_1 = grunk::Feature("cap_mults_V_val_1", "int", 2);
	auto cap_mults_V_val_2 = grunk::Feature("cap_mults_V_val_2", "int", 2);

	auto cap_mults_V = grunk::script(
		{
			{"grocc::TColStd_Array1OfInteger", {"cap_mults_V"}, {cap_mults_V_arg_1, cap_mults_V_arg_2}},
			{"grocc::TColStd_Array1OfInteger::SetValue", {}, {"cap_mults_V", n_1, cap_mults_V_val_1}},
			{"grocc::TColStd_Array1OfInteger::SetValue", {}, {"cap_mults_V", n_2, cap_mults_V_val_2}},
		},
		{"cap_mults_V"}
	).output();

// define the rear surface:
	auto rear_surface_cap = grunk::action("rear_surface_cap", "geo::b_spline_surface",  rear_srf_cap_cp_net, 
                 																rear_srf_cap_weight_net, 
                 																knots_U, 
                 																cap_knots_V, 
                 																mults_U, 
                 																cap_mults_V, 
                 																degree_U, 
                 																cap_degree_V, 
                 																periodic_U ).output(); 



/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//////////	Nose of fuselage
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////


// start with the start point (start_point_profile) and start 
// vector of the mid-fuselage profile curve (profile_curve_cylinder):
	auto end_point_arc = grunk::Feature("end_point_arc", "grocc::gp_Pnt", 2000.0, 0.0, 1000.0); // original: 2000.0, 0.0, 1000.0
	auto start_param = grunk::action("start_param", "grocc::Geom_BSplineCurve::FirstParameter", profile_curve_cylinder).output();

	auto start_point_arc_tangent_vector = grunk::script(
		{
			{"grocc::gp_Pnt", {"start_point_arc"}, {}},
			{"grocc::gp_Vec", {"tangent_vector"}, {}},
			{"grocc::Geom_BSplineCurve::D1", {}, {profile_curve_cylinder, start_param, "start_point_arc", "tangent_vector"}},
			{"grocc::gp_Vec::Reverse", {}, {"tangent_vector"}},
		},
		{"start_point_arc", "tangent_vector"}
	);

	auto start_point_arc = start_point_arc_tangent_vector.output(0);
	auto tangent_vector = start_point_arc_tangent_vector.output(1);
	
	auto arc_maker = grunk::action("arc_maker", "grocc::GC_MakeArcOfCircle", start_point_arc, tangent_vector, end_point_arc).output();
	auto circular_arc_trimmed = grunk::action("circular_arc", "grocc::GC_MakeArcOfCircle::Value", arc_maker).output(); //

	auto arc_b_spline_curve = grunk::action("arc_b_spline_curve", "geo::geom_convert_from_trimmed_curve", circular_arc_trimmed).output();

// now, create a surface of revolution around the profile curve arc_b_spline_curve:
	auto nose_ogive_srf = grunk::script(
		{
			{"geo::revolving_surface", {"nose_ogive_srf"}, {arc_b_spline_curve, rot_axis}},
			{"grocc::Geom_BSplineSurface::ExchangeUV", {}, {"nose_ogive_srf"}},
		},
		{"nose_ogive_srf"}
	).output();

// cap for nose surface:

// define a profile curve tangential to the profile curve of 
// nose_ogive_srf (arc_b_spline_curve):

// last control point of the cap-profile curve:
	auto end_point_arc_cap = grunk::Feature("end_point_arc_cap", "grocc::gp_Pnt", 1000.0, 0.0, 0.0); // original: 1000.0, 0.0, 0.0 

// get the first control point of the cap-profile curve and the starting tangent vector:
	auto start_param_cap = grunk::action("start_param_cap", "grocc::Geom_BSplineCurve::LastParameter", arc_b_spline_curve).output();


		auto start_point_arc_cap_tangent_vector_cap = grunk::script(
		{
			{"grocc::gp_Pnt", {"start_point_arc_cap"}, {}},
			{"grocc::gp_Vec", {"tangent_vector_cap"}, {}},
			{"grocc::Geom_BSplineCurve::D1", {}, {arc_b_spline_curve, start_param_cap, "start_point_arc_cap", "tangent_vector_cap"}},
		},
		{"start_point_arc_cap", "tangent_vector_cap"}
	);

	auto start_point_arc_cap = start_point_arc_cap_tangent_vector_cap.output(0);
	auto tangent_vector_cap = start_point_arc_cap_tangent_vector_cap.output(1);

// second control point of cap-profile curve:
	auto cp_cap_2_third_arg = grunk::Feature("cp_cap_2_third_arg", "double", 0.04);

	auto cp_cap_2 = grunk::action("cp_cap_2", "geo::move", start_point_arc_cap, tangent_vector_cap, cp_cap_2_third_arg).output();

//third control point of cap-profile curve:
	auto cp_cap_3_second_arg = grunk::Feature("cp_cap_3_second_arg", "grocc::gp_Vec", 0.0, 0.0, 1.0);
	auto cp_cap_3_third_arg = grunk::Feature("cp_cap_3_third_arg", "double", 550.0);

	auto cp_cap_3 = grunk::action("cp_cap_3", "geo::move", end_point_arc_cap, cp_cap_3_second_arg, cp_cap_3_third_arg).output();

// define the cap-profile curve:
	auto profile_points_cap_arg_1 = grunk::Feature("profile_points_cap_arg_1", "int", 1);
	auto profile_points_cap_arg_2 = grunk::Feature("profile_points_cap_arg_2", "int", 4);

	auto profile_points_cap = grunk::script(
		{
			{"grocc::TColgp_Array1OfPnt", {"profile_points_cap"}, {profile_points_cap_arg_1, profile_points_cap_arg_2}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"profile_points_cap", n_1, start_point_arc_cap}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"profile_points_cap", n_2, cp_cap_2}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"profile_points_cap", n_3, cp_cap_3}},
			{"grocc::TColgp_Array1OfPnt::SetValue", {}, {"profile_points_cap", n_4, end_point_arc_cap}},
		},
		{"profile_points_cap"}
	).output();

// degree:
	auto degree_profile_cap = grunk::Feature("degree_profile_cap", "int", 3);

// weights:
	auto weights_profile_cap_arg_1 = grunk::Feature("weights_profile_cap_arg_1", "int", 1);
	auto weights_profile_cap_arg_2 = grunk::Feature("weights_profile_cap_arg_2", "int", 4);

	auto weights_profile_cap_val_1 = grunk::Feature("weights_profile_cap_val_1", "double", 1.0);
	auto weights_profile_cap_val_2 = grunk::Feature("weights_profile_cap_val_2", "double", 1.0);
	auto weights_profile_cap_val_3 = grunk::Feature("weights_profile_cap_val_3", "double", 1.0);
	auto weights_profile_cap_val_4 = grunk::Feature("weights_profile_cap_val_4", "double", 1.0);

	auto weights_profile_cap = grunk::script(
		{
			{"grocc::TColStd_Array1OfReal", {"weights_profile_cap"}, {weights_profile_cap_arg_1, weights_profile_cap_arg_2}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"weights_profile_cap", n_1, weights_profile_cap_val_1}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"weights_profile_cap", n_2, weights_profile_cap_val_2}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"weights_profile_cap", n_3, weights_profile_cap_val_3}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"weights_profile_cap", n_4, weights_profile_cap_val_4}},
		},
		{"weights_profile_cap"}
	).output();

// knots:
	auto knots_profile_cap_arg_1 = grunk::Feature("knots_profile_cap_arg_1", "int", 1);
	auto knots_profile_cap_arg_2 = grunk::Feature("knots_profile_cap_arg_2", "int", 2);

	auto knots_profile_cap_val_1 = grunk::Feature("knots_profile_cap_val_1", "double", 0.0);
	auto knots_profile_cap_val_2 = grunk::Feature("knots_profile_cap_val_2", "double", 1.0);

	auto knots_profile_cap = grunk::script(
		{
			{"grocc::TColStd_Array1OfReal", {"knots_profile_cap"}, {knots_profile_cap_arg_1, knots_profile_cap_arg_2}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"knots_profile_cap", n_1, knots_profile_cap_val_1}},
			{"grocc::TColStd_Array1OfReal::SetValue", {}, {"knots_profile_cap", n_2, knots_profile_cap_val_2}},
		},
		{"knots_profile_cap"}
	).output();

// multiplicities: 
	auto mults_profile_cap_arg_1 = grunk::Feature("mults_profile_cap_arg_1", "int", 1);
	auto mults_profile_cap_arg_2 = grunk::Feature("mults_profile_cap_arg_2", "int", 2);

	auto mults_profile_cap_val_1 = grunk::Feature("mults_profile_cap_val_1", "int", 4);
	auto mults_profile_cap_val_2 = grunk::Feature("mults_profile_cap_val_2", "int", 4);

	auto mults_profile_cap = grunk::script(
		{
			{"grocc::TColStd_Array1OfInteger", {"mults_profile_cap"}, {mults_profile_cap_arg_1, mults_profile_cap_arg_2}},
			{"grocc::TColStd_Array1OfInteger::SetValue", {}, {"mults_profile_cap", n_1, mults_profile_cap_val_1}},
			{"grocc::TColStd_Array1OfInteger::SetValue", {}, {"mults_profile_cap", n_2, mults_profile_cap_val_2}},
		},
		{"mults_profile_cap"}
	).output();

// create the profile curve:
	auto profile_curve_cap = grunk::action("profile_curve_cap", "geo::b_spline_curve", profile_points_cap, weights_profile_cap, knots_profile_cap, mults_profile_cap, degree_profile_cap).output();

// now, create a surface of revolution around the profile curve profile_curve_cap:
	auto nose_ogive_cap_srf = grunk::script(
		{
			{"geo::revolving_surface", {"nose_ogive_cap_srf"}, {profile_curve_cap, rot_axis}},
			{"grocc::Geom_BSplineSurface::ExchangeUV", {}, {"nose_ogive_cap_srf"}},
		},
		{"nose_ogive_cap_srf"}
	).output();


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////  
//////////   														
//////////	Wings
//////////
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////  


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
//////////	Main wings
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////


// start with the upper part of the wing:

// define the inner-upper profile curve:
// note that the second and the 21st point are commented out
auto i_u_pc_1 = grunk::Feature("i_u_pc_1", "grocc::gp_Pnt", 12743.744, -1875.046, -1242.3);
//auto i_u_pc_2 = grunk::Feature("i_u_pc_2", "grocc::gp_Pnt", 12743.761, -1875.046, -1241.294);
auto i_u_pc_3 = grunk::Feature("i_u_pc_3", "grocc::gp_Pnt", 12744.907, -1875.046, -1192.336);
auto i_u_pc_4 = grunk::Feature("i_u_pc_4", "grocc::gp_Pnt", 12780.961, -1875.046, -1085.249);
auto i_u_pc_5 = grunk::Feature("i_u_pc_5", "grocc::gp_Pnt", 12922.121, -1875.046, -981.973);
auto i_u_pc_6 = grunk::Feature("i_u_pc_6", "grocc::gp_Pnt", 13134.59, -1875.046, -914.366);
auto i_u_pc_7 = grunk::Feature("i_u_pc_7", "grocc::gp_Pnt", 13412.355, -1875.046, -882.808);
auto i_u_pc_8 = grunk::Feature("i_u_pc_8", "grocc::gp_Pnt", 13751.543, -1875.046, -872.338);
auto i_u_pc_9 = grunk::Feature("i_u_pc_9", "grocc::gp_Pnt", 14145.044, -1875.046, -881.43);
auto i_u_pc_10 = grunk::Feature("i_u_pc_10", "grocc::gp_Pnt", 14582.641, -1875.046, -908.641);
auto i_u_pc_11 = grunk::Feature("i_u_pc_11", "grocc::gp_Pnt", 15052.873, -1875.046, -946.113);
auto i_u_pc_12 = grunk::Feature("i_u_pc_12", "grocc::gp_Pnt", 15543.057, -1875.046, -990.982);
auto i_u_pc_13 = grunk::Feature("i_u_pc_13", "grocc::gp_Pnt", 16039.909, -1875.046, -1040.433);
auto i_u_pc_14 = grunk::Feature("i_u_pc_14", "grocc::gp_Pnt", 16529.885, -1875.046, -1093.018);
auto i_u_pc_15 = grunk::Feature("i_u_pc_15", "grocc::gp_Pnt", 16999.603, -1875.046, -1148.158);
auto i_u_pc_16 = grunk::Feature("i_u_pc_16", "grocc::gp_Pnt", 17436.043, -1875.046, -1206.562);
auto i_u_pc_17 = grunk::Feature("i_u_pc_17", "grocc::gp_Pnt", 17827.393, -1875.046, -1266.624);
auto i_u_pc_18 = grunk::Feature("i_u_pc_18", "grocc::gp_Pnt", 18162.459, -1875.046, -1324.558);
auto i_u_pc_19 = grunk::Feature("i_u_pc_19", "grocc::gp_Pnt", 18433.527, -1875.046, -1376.16);
auto i_u_pc_20 = grunk::Feature("i_u_pc_20", "grocc::gp_Pnt", 18666.664, -1875.046, -1423.234);
//auto i_u_pc_21 = grunk::Feature("i_u_pc_21", "grocc::gp_Pnt", 18791.177, -1875.046, -1444.845);
auto i_u_pc_22 = grunk::Feature("i_u_pc_22", "grocc::gp_Pnt", 18815.763, -1875.046, -1454.34);

auto i_u_profile_curve_points_initial = grunk::vec("i_u_profile_curve_points_initial", i_u_pc_1,
																						i_u_pc_3,
																						i_u_pc_4,
																						i_u_pc_5,
																						i_u_pc_6,
																						i_u_pc_7,
																						i_u_pc_8,
																						i_u_pc_9,
																						i_u_pc_10,
																						i_u_pc_11,
																						i_u_pc_12,
																						i_u_pc_13,
																						i_u_pc_14,
																						i_u_pc_15,
																						i_u_pc_16,
																						i_u_pc_17,
																						i_u_pc_18,
																						i_u_pc_19,
																						i_u_pc_20, 
																						i_u_pc_22);

// introduce a move in y direction parameter
// define the direction vector:
	auto move_vec_i_wing = grunk::Feature("move_vec_i_wing", "grocc::gp_Vec", 0.0, 1.0, 0.0);

// define the factor multiplied to this vector for the first move:
	auto move_factor_i_wing = grunk::Feature("move_factor_i_wing", "double", 1500.0);

// move the control points cp_column (for the periodic case):
	auto i_u_profile_curve_points = grunk::action("i_u_profile_curve_points", "geo::move", i_u_profile_curve_points_initial, move_vec_i_wing, move_factor_i_wing).output();

	auto i_u_profile_curve = grunk::action("i_u_profile_curve", "geo::interpolate_points_to_b_spline_curve", i_u_profile_curve_points).output();

// define the mid upper profile curve:

// note that the second and the 21st point are commented out
	auto m_u_pc_1 = grunk::Feature("m_u_pc_1", "grocc::gp_Pnt", 15081.173, -6340.33, -933.025);
	//auto m_u_pc_2 = grunk::Feature("m_u_pc_2", "grocc::gp_Pnt", 15081.183, -6340.33, -932.402);
	auto m_u_pc_3 = grunk::Feature("m_u_pc_3", "grocc::gp_Pnt", 15081.892, -6340.33, -902.118);
	auto m_u_pc_4 = grunk::Feature("m_u_pc_4", "grocc::gp_Pnt", 15104.195, -6340.33, -835.873);
	auto m_u_pc_5 = grunk::Feature("m_u_pc_5", "grocc::gp_Pnt", 15191.517, -6340.33, -771.987);
	auto m_u_pc_6 = grunk::Feature("m_u_pc_6", "grocc::gp_Pnt", 15322.95, -6340.33, -730.165);
	auto m_u_pc_7 = grunk::Feature("m_u_pc_7", "grocc::gp_Pnt", 15494.776, -6340.33, -710.643);
	auto m_u_pc_8 = grunk::Feature("m_u_pc_8", "grocc::gp_Pnt", 15704.597, -6340.33, -704.166);
	auto m_u_pc_9 = grunk::Feature("m_u_pc_9", "grocc::gp_Pnt", 15948.017, -6340.33, -709.791);
	auto m_u_pc_10 = grunk::Feature("m_u_pc_10", "grocc::gp_Pnt", 16218.715, -6340.33, -726.624);
	auto m_u_pc_11 = grunk::Feature("m_u_pc_11", "grocc::gp_Pnt", 16509.6, -6340.33, -749.804);
	auto m_u_pc_12 = grunk::Feature("m_u_pc_12", "grocc::gp_Pnt", 16812.828, -6340.33, -777.56);
	auto m_u_pc_13 = grunk::Feature("m_u_pc_13", "grocc::gp_Pnt", 17120.18, -6340.33, -808.15);
	auto m_u_pc_14 = grunk::Feature("m_u_pc_14", "grocc::gp_Pnt", 17423.28, -6340.33, -840.679);
	auto m_u_pc_15 = grunk::Feature("m_u_pc_15", "grocc::gp_Pnt", 17713.847, -6340.33, -874.789);
	auto m_u_pc_16 = grunk::Feature("m_u_pc_16", "grocc::gp_Pnt", 17983.829, -6340.33, -910.917);
	auto m_u_pc_17 = grunk::Feature("m_u_pc_17", "grocc::gp_Pnt", 18225.918, -6340.33, -948.072);
	auto m_u_pc_18 = grunk::Feature("m_u_pc_18", "grocc::gp_Pnt", 18433.19, -6340.33, -983.91);
	auto m_u_pc_19 = grunk::Feature("m_u_pc_19", "grocc::gp_Pnt", 18600.873, -6340.33, -1015.831);
	auto m_u_pc_20 = grunk::Feature("m_u_pc_20", "grocc::gp_Pnt", 18745.091, -6340.33, -1044.951);
	//auto m_u_pc_21 = grunk::Feature("m_u_pc_21", "grocc::gp_Pnt", 18822.115, -6340.33, -1058.319);
	auto m_u_pc_22 = grunk::Feature("m_u_pc_22", "grocc::gp_Pnt", 18837.323, -6340.33, -1064.193);

	auto m_u_profile_curve_points = grunk::vec("m_u_profile_curve_points", m_u_pc_1,
																		m_u_pc_3,
																		m_u_pc_4,
																		m_u_pc_5,
																		m_u_pc_6,
																		m_u_pc_7,
																		m_u_pc_8,
																		m_u_pc_9,
																		m_u_pc_10,
																		m_u_pc_11,
																		m_u_pc_12,
																		m_u_pc_13,
																		m_u_pc_14,
																		m_u_pc_15,
																		m_u_pc_16,
																		m_u_pc_17,
																		m_u_pc_18,
																		m_u_pc_19,
																		m_u_pc_20, 
																		m_u_pc_22);

	auto m_u_profile_curve = grunk::action("m_u_profile_curve", "geo::interpolate_points_to_b_spline_curve", m_u_profile_curve_points).output();																		

// define the outer upper profile curve:
	auto o_u_pc_22 = grunk::Feature("o_u_pc_22", "grocc::gp_Pnt", 22136.972, -16963.479, -249.447);
	//auto o_u_pc_21 = grunk::Feature("o_u_pc_21", "grocc::gp_Pnt", 22130.919, -16963.479, -247.11);
	auto o_u_pc_20 = grunk::Feature("o_u_pc_20", "grocc::gp_Pnt", 22100.264, -16963.479, -241.789);
	auto o_u_pc_19 = grunk::Feature("o_u_pc_19", "grocc::gp_Pnt", 22042.866, -16963.479, -230.2);
	auto o_u_pc_18 = grunk::Feature("o_u_pc_18", "grocc::gp_Pnt", 21976.129, -16963.479, -217.495);
	auto o_u_pc_17 = grunk::Feature("o_u_pc_17", "grocc::gp_Pnt", 21893.636, -16963.479, -203.232);
	auto o_u_pc_16 = grunk::Feature("o_u_pc_16", "grocc::gp_Pnt", 21797.285, -16963.479, -188.444);
	auto o_u_pc_15 = grunk::Feature("o_u_pc_15", "grocc::gp_Pnt", 21689.834, -16963.479, -174.065);
	auto o_u_pc_14 = grunk::Feature("o_u_pc_14", "grocc::gp_Pnt", 21574.189, -16963.479, -160.49);
	auto o_u_pc_13 = grunk::Feature("o_u_pc_13", "grocc::gp_Pnt", 21453.557, -16963.479, -147.543);
	auto o_u_pc_12 = grunk::Feature("o_u_pc_12", "grocc::gp_Pnt", 21331.232, -16963.479, -135.369);
	auto o_u_pc_11 = grunk::Feature("o_u_pc_11", "grocc::gp_Pnt", 21210.549, -16963.479, -124.322);
	auto o_u_pc_10 = grunk::Feature("o_u_pc_10", "grocc::gp_Pnt", 21094.778, -16963.479, -115.096);
	auto o_u_pc_9 = grunk::Feature("o_u_pc_9", "grocc::gp_Pnt", 20987.041, -16963.479, -108.397);
	auto o_u_pc_8 = grunk::Feature("o_u_pc_8", "grocc::gp_Pnt", 20890.161, -16963.479, -106.158);
	auto o_u_pc_7 = grunk::Feature("o_u_pc_7", "grocc::gp_Pnt", 20806.653, -16963.479, -108.736);
	auto o_u_pc_6 = grunk::Feature("o_u_pc_6", "grocc::gp_Pnt", 20738.268, -16963.479, -116.506);
	auto o_u_pc_5 = grunk::Feature("o_u_pc_5", "grocc::gp_Pnt", 20685.958, -16963.479, -133.151);
	auto o_u_pc_4 = grunk::Feature("o_u_pc_4", "grocc::gp_Pnt", 20651.204, -16963.479, -158.577);
	auto o_u_pc_3 = grunk::Feature("o_u_pc_3", "grocc::gp_Pnt", 20642.328, -16963.479, -184.942);
	//auto o_u_pc_2 = grunk::Feature("o_u_pc_2", "grocc::gp_Pnt", 20642.045, -16963.479, -196.995);
	auto o_u_pc_1 = grunk::Feature("o_u_pc_1", "grocc::gp_Pnt", 20642.041, -16963.479, -197.243);

	auto o_u_profile_curve_points_initial = grunk::vec("o_u_profile_curve_points_initial", o_u_pc_1,
																						o_u_pc_3,
																						o_u_pc_4,
																						o_u_pc_5,
																						o_u_pc_6,
																						o_u_pc_7,
																						o_u_pc_8,
																						o_u_pc_9,
																						o_u_pc_10,
																						o_u_pc_11,
																						o_u_pc_12,
																						o_u_pc_13,
																						o_u_pc_14,
																						o_u_pc_15,
																						o_u_pc_16,
																						o_u_pc_17,
																						o_u_pc_18,
																						o_u_pc_19,
																						o_u_pc_20, 
																						o_u_pc_22);

// introduce a move in y direction parameter
// define the direction vector:
	auto move_vec_o_wing = grunk::Feature("move_vec_o_wing", "grocc::gp_Vec", 0.0, 1.0, 0.0); // original: 0.0, 1.0, 0.0

// define the factor multiplied to this vector for the first move:
	auto move_factor_o_wing = grunk::Feature("move_factor_o_wing", "double", -1500.0); // original: -1500.0

// move the control points 
	auto o_u_profile_curve_points = grunk::action("o_u_profile_curve_points", "geo::move", o_u_profile_curve_points_initial, move_vec_o_wing, move_factor_o_wing).output();

	auto o_u_profile_curve = grunk::action("o_u_profile_curve", "geo::interpolate_points_to_b_spline_curve", o_u_profile_curve_points).output();

// now, define the guiding curves of the wing:

// leading guide curve:
	auto leading_curve_points_arg_1 = grunk::action("leading_curve_points_arg_1", "geo::vector_of_points_at_index", i_u_profile_curve_points, n_0).output();
	auto leading_curve_points_arg_3 = grunk::action("leading_curve_points_arg_3", "geo::vector_of_points_at_index", o_u_profile_curve_points, n_0).output();

	auto leading_curve_points = grunk::vec("leading_curve_points", leading_curve_points_arg_1, m_u_pc_1, leading_curve_points_arg_3);

	auto leading_curve = grunk::action("leading_curve", "geo::interpolate_points_to_b_spline_curve", leading_curve_points).output();

// trailing guide curve:
	auto trailing_curve_points_arg_1 = grunk::action("trailing_curve_points_arg_1", "geo::vector_of_points_at_index", i_u_profile_curve_points, n_19).output();

	auto trailing_curve_points_arg_3 = grunk::action("trailing_curve_points_arg_3", "geo::vector_of_points_at_index", o_u_profile_curve_points, n_19).output();

	auto trailing_curve_points = grunk::vec("trailing_curve_points", trailing_curve_points_arg_1, m_u_pc_22, trailing_curve_points_arg_3);

	auto trailing_curve = grunk::action("trailing_curve", "geo::interpolate_points_to_b_spline_curve", trailing_curve_points).output();

// now, define the upper wing's surface:
	auto list_of_profiles_upper_wing_bspline = grunk::vec("list_of_profiles_upper_wing_bspline", i_u_profile_curve, m_u_profile_curve, o_u_profile_curve);

	auto list_of_guides_upper_wing_bspline = grunk::vec("list_of_guides_upper_wing_bspline", leading_curve, trailing_curve);

	auto list_of_profiles_upper_wing = grunk::action("list_of_profiles_upper_wing", "geo::vector_b_spline_to_geom_curve_converter", list_of_profiles_upper_wing_bspline).output();
	auto list_of_guides_upper_wing = grunk::action("list_of_guides_upper_wing", "geo::vector_b_spline_to_geom_curve_converter", list_of_guides_upper_wing_bspline).output();

	auto upper_wing_surface_tol = grunk::Feature("upper_wing_surface_tol", "double", 1.0);

	auto upper_wing_surface = grunk::script(
		{
			{"geo::interpolate_curve_network", {"upper_wing_surface"}, {list_of_profiles_upper_wing, list_of_guides_upper_wing, upper_wing_surface_tol}},
			{"grocc::Geom_BSplineSurface::ExchangeUV", {}, {"upper_wing_surface"}},
		},
		{"upper_wing_surface"}
	).output();

// lower part of the wing:
// define the inner-lower profile curve:				
	auto i_l_pc_21 = grunk::Feature("i_l_pc_21", "grocc::gp_Pnt", 18815.763, -1875.046, -1454.34);
	auto i_l_pc_20 = grunk::Feature("i_l_pc_20", "grocc::gp_Pnt", 18790.359, -1875.046, -1455.64);
	auto i_l_pc_19 = grunk::Feature("i_l_pc_19", "grocc::gp_Pnt", 18665.042, -1875.046, -1461.521);
	auto i_l_pc_18 = grunk::Feature("i_l_pc_18", "grocc::gp_Pnt", 18429.553, -1875.046, -1477.358);
	auto i_l_pc_17 = grunk::Feature("i_l_pc_17", "grocc::gp_Pnt", 18155.329, -1875.046, -1503.77);
	auto i_l_pc_16 = grunk::Feature("i_l_pc_16", "grocc::gp_Pnt", 17816.545, -1875.046, -1547.528);
	auto i_l_pc_15 = grunk::Feature("i_l_pc_15", "grocc::gp_Pnt", 17421.421, -1875.046, -1611.353);
	auto i_l_pc_14 = grunk::Feature("i_l_pc_14", "grocc::gp_Pnt", 16981.161, -1875.046, -1683.479);
	auto i_l_pc_13 = grunk::Feature("i_l_pc_13", "grocc::gp_Pnt", 16507.719, -1875.046, -1754.291);
	auto i_l_pc_12 = grunk::Feature("i_l_pc_12", "grocc::gp_Pnt", 16014.058, -1875.046, -1809.728);
	auto i_l_pc_11 = grunk::Feature("i_l_pc_11", "grocc::gp_Pnt", 15513.978, -1875.046, -1841.509);
	auto i_l_pc_10 = grunk::Feature("i_l_pc_10", "grocc::gp_Pnt", 15021.479, -1875.046, -1849.277);
	auto i_l_pc_9  = grunk::Feature("i_l_pc_9", "grocc::gp_Pnt", 14549.965, -1875.046, -1835.59); 
	auto i_l_pc_8  = grunk::Feature("i_l_pc_8", "grocc::gp_Pnt", 14112.211, -1875.046, -1801.944); 
	auto i_l_pc_7  = grunk::Feature("i_l_pc_7", "grocc::gp_Pnt", 13719.716, -1875.046, -1750.54); 
	auto i_l_pc_6  = grunk::Feature("i_l_pc_6", "grocc::gp_Pnt", 13383.024, -1875.046, -1682.695); 
	auto i_l_pc_5  = grunk::Feature("i_l_pc_5", "grocc::gp_Pnt", 13108.738, -1875.046, -1602.313); 
	auto i_l_pc_4  = grunk::Feature("i_l_pc_4", "grocc::gp_Pnt", 12909.179, -1875.046, -1489.265); 
	auto i_l_pc_3  = grunk::Feature("i_l_pc_3", "grocc::gp_Pnt", 12772.556, -1875.046, -1381.774); 
	auto i_l_pc_2  = grunk::Feature("i_l_pc_2", "grocc::gp_Pnt", 12742.968, -1875.046, -1289.064); 
	auto i_l_pc_1  = grunk::Feature("i_l_pc_1", "grocc::gp_Pnt", 12743.744, -1875.046, -1242.3); 

	auto i_l_profile_curve_points_initial = grunk::vec("i_l_profile_curve_points_initial", i_l_pc_1,
																					   i_l_pc_2,
																					   i_l_pc_3,
																					   i_l_pc_4,
																					   i_l_pc_5,
																					   i_l_pc_6,
																					   i_l_pc_7,
																					   i_l_pc_8,
																					   i_l_pc_9,
																					   i_l_pc_10,
																					   i_l_pc_11,
																					   i_l_pc_12,
																					   i_l_pc_13,
																					   i_l_pc_14,
																					   i_l_pc_15,
																					   i_l_pc_16,
																					   i_l_pc_17,
																					   i_l_pc_18,
																					   i_l_pc_19,
																					   i_l_pc_20, 
																					   i_l_pc_21);

// move the control points in y direction:
	auto i_l_profile_curve_points = grunk::action("i_l_profile_curve_points", "geo::move", i_l_profile_curve_points_initial, move_vec_i_wing, move_factor_i_wing).output();

	auto i_l_profile_curve = grunk::action("i_l_profile_curve", "geo::interpolate_points_to_b_spline_curve", i_l_profile_curve_points).output();

// define the mid-lower profile curve:					
	auto m_l_pc_1  = grunk::Feature("m_l_pc_1",  "grocc::gp_Pnt", 15081.173, -6340.33, -933.025);
	auto m_l_pc_2  = grunk::Feature("m_l_pc_2",  "grocc::gp_Pnt", 15080.693, -6340.33, -961.953);
	auto m_l_pc_3  = grunk::Feature("m_l_pc_3",  "grocc::gp_Pnt", 15098.996, -6340.33, -1019.304);
	auto m_l_pc_4  = grunk::Feature("m_l_pc_4",  "grocc::gp_Pnt", 15183.511, -6340.33, -1085.797);
	auto m_l_pc_5  = grunk::Feature("m_l_pc_5",  "grocc::gp_Pnt", 15306.958, -6340.33, -1155.729);
	auto m_l_pc_6  = grunk::Feature("m_l_pc_6",  "grocc::gp_Pnt", 15476.631, -6340.33, -1205.453);
	auto m_l_pc_7  = grunk::Feature("m_l_pc_7",  "grocc::gp_Pnt", 15684.909, -6340.33, -1247.422);
	auto m_l_pc_8  = grunk::Feature("m_l_pc_8",  "grocc::gp_Pnt", 15927.706, -6340.33, -1279.221);
	auto m_l_pc_9  = grunk::Feature("m_l_pc_9",  "grocc::gp_Pnt", 16198.501, -6340.33, -1300.034);
	auto m_l_pc_10 = grunk::Feature("m_l_pc_10", "grocc::gp_Pnt", 16490.18,  -6340.33, -1308.501);
	auto m_l_pc_11 = grunk::Feature("m_l_pc_11", "grocc::gp_Pnt", 16794.84,  -6340.33, -1303.695);
	auto m_l_pc_12 = grunk::Feature("m_l_pc_12", "grocc::gp_Pnt", 17104.189, -6340.33, -1284.036);
	auto m_l_pc_13 = grunk::Feature("m_l_pc_13", "grocc::gp_Pnt", 17409.568, -6340.33, -1249.742);
	auto m_l_pc_14 = grunk::Feature("m_l_pc_14", "grocc::gp_Pnt", 17702.439, -6340.33, -1205.938);
	auto m_l_pc_15 = grunk::Feature("m_l_pc_15", "grocc::gp_Pnt", 17974.784, -6340.33, -1161.321);
	auto m_l_pc_16 = grunk::Feature("m_l_pc_16", "grocc::gp_Pnt", 18219.207, -6340.33, -1121.839);
	auto m_l_pc_17 = grunk::Feature("m_l_pc_17", "grocc::gp_Pnt", 18428.779, -6340.33, -1094.77);
	auto m_l_pc_18 = grunk::Feature("m_l_pc_18", "grocc::gp_Pnt", 18598.415, -6340.33, -1078.432);
	auto m_l_pc_19 = grunk::Feature("m_l_pc_19", "grocc::gp_Pnt", 18744.088, -6340.33, -1068.635);
	auto m_l_pc_20 = grunk::Feature("m_l_pc_20", "grocc::gp_Pnt", 18821.609, -6340.33, -1064.997);
	auto m_l_pc_21 = grunk::Feature("m_l_pc_21", "grocc::gp_Pnt", 18837.323, -6340.33, -1064.193);

	auto m_l_profile_curve_points = grunk::vec("m_l_profile_curve_points", m_l_pc_1,  
																		m_l_pc_2,  
																		m_l_pc_3,  
																		m_l_pc_4,  
																		m_l_pc_5,  
																		m_l_pc_6,  
																		m_l_pc_7,  
																		m_l_pc_8,  
																		m_l_pc_9,  
																		m_l_pc_10, 
																		m_l_pc_11, 
																		m_l_pc_12, 
																		m_l_pc_13, 
																		m_l_pc_14, 
																		m_l_pc_15, 
																		m_l_pc_16, 
																		m_l_pc_17, 
																		m_l_pc_18, 
																		m_l_pc_19, 
																		m_l_pc_20, 
																		m_l_pc_21 );

	auto m_l_profile_curve = grunk::action("m_l_profile_curve", "geo::interpolate_points_to_b_spline_curve", m_l_profile_curve_points).output();

// define the outer-lower profile curve:	
	auto o_l_pc_1  = grunk::Feature("o_l_pc_1",  "grocc::gp_Pnt", 20642.041, -16963.479, -197.243);
	auto o_l_pc_2  = grunk::Feature("o_l_pc_2",  "grocc::gp_Pnt", 20641.85,  -16963.479, -208.756);
	auto o_l_pc_3  = grunk::Feature("o_l_pc_3",  "grocc::gp_Pnt", 20649.135, -16963.479, -231.582);
	auto o_l_pc_4  = grunk::Feature("o_l_pc_4",  "grocc::gp_Pnt", 20682.771, -16963.479, -258.046);
	auto o_l_pc_5  = grunk::Feature("o_l_pc_5",  "grocc::gp_Pnt", 20731.903, -16963.479, -285.878);
	auto o_l_pc_6  = grunk::Feature("o_l_pc_6",  "grocc::gp_Pnt", 20799.432, -16963.479, -305.668);
	auto o_l_pc_7  = grunk::Feature("o_l_pc_7",  "grocc::gp_Pnt", 20882.326, -16963.479, -322.372);
	auto o_l_pc_8  = grunk::Feature("o_l_pc_8",  "grocc::gp_Pnt", 20978.958, -16963.479, -335.028);
	auto o_l_pc_9  = grunk::Feature("o_l_pc_9",  "grocc::gp_Pnt", 21086.733, -16963.479, -343.311);
	auto o_l_pc_10 = grunk::Feature("o_l_pc_10", "grocc::gp_Pnt", 21202.82,  -16963.479, -346.681);
	auto o_l_pc_11 = grunk::Feature("o_l_pc_11", "grocc::gp_Pnt", 21324.073, -16963.479, -344.768);
	auto o_l_pc_12 = grunk::Feature("o_l_pc_12", "grocc::gp_Pnt", 21447.193, -16963.479, -336.944);
	auto o_l_pc_13 = grunk::Feature("o_l_pc_13", "grocc::gp_Pnt", 21568.732, -16963.479, -323.295);
	auto o_l_pc_14 = grunk::Feature("o_l_pc_14", "grocc::gp_Pnt", 21685.293, -16963.479, -305.861);
	auto o_l_pc_15 = grunk::Feature("o_l_pc_15", "grocc::gp_Pnt", 21793.686, -16963.479, -288.104);
	auto o_l_pc_16 = grunk::Feature("o_l_pc_16", "grocc::gp_Pnt", 21890.965, -16963.479, -272.39);
	auto o_l_pc_17 = grunk::Feature("o_l_pc_17", "grocc::gp_Pnt", 21974.373, -16963.479, -261.617);
	auto o_l_pc_18 = grunk::Feature("o_l_pc_18", "grocc::gp_Pnt", 22041.888, -16963.479, -255.114);
	auto o_l_pc_19 = grunk::Feature("o_l_pc_19", "grocc::gp_Pnt", 22099.865, -16963.479, -251.215);
	auto o_l_pc_20 = grunk::Feature("o_l_pc_20", "grocc::gp_Pnt", 22130.718, -16963.479, -249.767);
	auto o_l_pc_21 = grunk::Feature("o_l_pc_21", "grocc::gp_Pnt", 22136.972, -16963.479, -249.447);

	auto o_l_profile_curve_points_initial = grunk::vec("o_l_profile_curve_points_initial",  o_l_pc_1,
																						o_l_pc_2,
																						o_l_pc_3,
																						o_l_pc_4,
																						o_l_pc_5,
																						o_l_pc_6,
																						o_l_pc_7,
																						o_l_pc_8,
																						o_l_pc_9,
																						o_l_pc_10,
																						o_l_pc_11,
																						o_l_pc_12,
																						o_l_pc_13,
																						o_l_pc_14,
																						o_l_pc_15,
																						o_l_pc_16,
																						o_l_pc_17,
																						o_l_pc_18,
																						o_l_pc_19,
																						o_l_pc_20,
																						o_l_pc_21);

// introduce a move in y direction parameter
// define the direction vector:

// move the control points 
	auto o_l_profile_curve_points = grunk::action("o_l_profile_curve_points", "geo::move", o_l_profile_curve_points_initial, move_vec_o_wing, move_factor_o_wing).output();

	auto o_l_profile_curve = grunk::action("o_l_profile_curve", "geo::interpolate_points_to_b_spline_curve", o_l_profile_curve_points).output();

// now, define the lower wing's surface:
	auto list_of_profiles_lower_wing_bspline = grunk::vec("list_of_profiles_lower_wing_bspline", i_l_profile_curve, m_l_profile_curve, o_l_profile_curve);

	auto list_of_profiles_lower_wing = grunk::action("list_of_profiles_lower_wing", "geo::vector_b_spline_to_geom_curve_converter", list_of_profiles_lower_wing_bspline).output();

	auto list_of_profiles_lower_wing_tol = grunk::Feature("list_of_profiles_lower_wing_tol", "double", 1.0);

	auto lower_wing_surface = grunk::action("lower_wing_surface", "geo::interpolate_curve_network", list_of_profiles_lower_wing, list_of_guides_upper_wing, list_of_profiles_lower_wing_tol).output();

// now, define the inner cap of the wing:
// the edges are given by
	auto edges_of_inner_wing_cap_bspline = grunk::vec("edges_of_inner_wing_cap_bspline", i_u_profile_curve, i_l_profile_curve);

	auto edges_of_inner_wing_cap = grunk::action("edges_of_inner_wing_cap", "geo::vector_b_spline_to_geom_curve_converter", edges_of_inner_wing_cap_bspline).output();

// define the surface:
	auto inner_wing_cap_surface = grunk::action("inner_wing_cap_surface", "geo::interpolate_curves", edges_of_inner_wing_cap).output();

// write the surface inner_wing_cap_surface to file:
	auto inner_wing_cap_surface_shape = grunk::action("inner_wing_cap_surface_shape", "geo::make_face", inner_wing_cap_surface).output();
	auto inner_wing_cap_surface_name_brep = grunk::Feature("inner_wing_cap_surface_name_brep", "String", "new_inner_wing_cap_surface.brep");
	grunk::action("", "grocc::BRepTools::Write", inner_wing_cap_surface_shape, inner_wing_cap_surface_name_brep).eval();

// now, define the outer cap of the wing:
// the edges are given by
	auto edges_of_outer_wing_cap_bspline = grunk::vec("edges_of_outer_wing_cap_bspline", o_u_profile_curve, o_l_profile_curve);

	auto edges_of_outer_wing_cap = grunk::action("edges_of_outer_wing_cap", "geo::vector_b_spline_to_geom_curve_converter", edges_of_outer_wing_cap_bspline).output();

// define the surface:
	auto outer_wing_cap_surface = grunk::script(
		{
			{"geo::interpolate_curves", {"outer_wing_cap_surface"}, {edges_of_outer_wing_cap}},
			{"grocc::Geom_BSplineSurface::ExchangeUV", {}, {"outer_wing_cap_surface"}},
		},
		{"outer_wing_cap_surface"}
	).output();

// create a solid of the wing:
// create faces of the four wing surfaces:
	auto face_wing_tol = grunk::Feature("face_wing_tol", "double", 1.0);

	auto upper_wing_surface_geom = grunk::action("upper_wing_surface_geom", "geo::convert_to_geom_surface", upper_wing_surface).output();
	auto lower_wing_surface_geom = grunk::action("lower_wing_surface_geom", "geo::convert_to_geom_surface", lower_wing_surface).output();
	auto inner_wing_cap_surface_geom = grunk::action("inner_wing_cap_surface_geom", "geo::convert_to_geom_surface", inner_wing_cap_surface).output();
	auto outer_wing_cap_surface_geom = grunk::action("outer_wing_cap_surface_geom", "geo::convert_to_geom_surface", outer_wing_cap_surface).output();

	auto face_upper_wing = grunk::action("face_upper_wing", "grocc::BRepBuilderAPI_MakeFace", upper_wing_surface_geom, face_wing_tol).output();
	auto face_lower_wing = grunk::action("face_lower_wing", "grocc::BRepBuilderAPI_MakeFace", lower_wing_surface_geom, face_wing_tol).output();
	auto face_inner_cap = grunk::action("face_inner_cap", "grocc::BRepBuilderAPI_MakeFace", inner_wing_cap_surface_geom, face_wing_tol).output();
	auto face_outer_cap = grunk::action("face_outer_cap", "grocc::BRepBuilderAPI_MakeFace", outer_wing_cap_surface_geom, face_wing_tol).output();

// Combine the faces into a shell
// Create a solid from the shell
	auto solid_wing = grunk::script(
		{
			{"grocc::BRep_Builder", {"builder_wing"}, {}},
			{"grocc::TopoDS_Shell", {"shell_wing"}, {}},
			{"grocc::BRep_Builder::MakeShell", {}, {"builder_wing", "shell_wing"}},
			{"grocc::BRep_Builder::Add", {}, {"builder_wing", "shell_wing", face_upper_wing}},
			{"grocc::BRep_Builder::Add", {}, {"builder_wing", "shell_wing", face_lower_wing}},
			{"grocc::BRep_Builder::Add", {}, {"builder_wing", "shell_wing", face_inner_cap}},
			{"grocc::BRep_Builder::Add", {}, {"builder_wing", "shell_wing", face_outer_cap}},
			{"grocc::TopoDS_Solid", {"solid_wing"}, {}},
			{"grocc::BRep_Builder::MakeSolid", {}, {"builder_wing", "solid_wing"}},
			{"grocc::BRep_Builder::Add", {}, {"builder_wing", "solid_wing", "shell_wing"}},
		},
		{"solid_wing"}
	).output();

// write grunk recipe of solid_wing:
	grunk::write("solid_wing.grr", solid_wing);

// write solid_wing as stp file:
	auto solid_wing_name_stp = grunk::Feature("solid_wing_name_stp", "String", "solid_wing.stp");
	auto solid_wing_out = grunk::action("solid_wing_out", "geo::writeGeomEntityToStepFile", solid_wing, solid_wing_name_stp).output();
	solid_wing_out.value();

// create a solid of the fuselage:
// create faces of the five fuselage surfaces:
	auto face_fuselage_tol = grunk::Feature("face_fuselage_tol", "double", 1.0);

	auto fuselage_cylinder_geom = grunk::action("fuselage_cylinder_geom", "geo::convert_to_geom_surface", fuselage_cylinder).output();
	auto rear_surface_geom = grunk::action("rear_surface_geom", "geo::convert_to_geom_surface", rear_surface).output();
	auto nose_ogive_cap_srf_geom = grunk::action("nose_ogive_cap_srf_geom", "geo::convert_to_geom_surface", nose_ogive_cap_srf).output();
	auto nose_ogive_srf_geom = grunk::action("nose_ogive_srf_geom", "geo::convert_to_geom_surface", nose_ogive_srf).output();
	auto rear_surface_cap_geom = grunk::action("rear_surface_cap_geom", "geo::convert_to_geom_surface", rear_surface_cap).output();

	auto face_cylinder_fuselage = grunk::action("face_cylinder_fuselage", "grocc::BRepBuilderAPI_MakeFace", fuselage_cylinder_geom, face_fuselage_tol).output();
	auto face_rear_fuselage = grunk::action("face_rear_fuselage", "grocc::BRepBuilderAPI_MakeFace", rear_surface_geom, face_fuselage_tol).output();
	auto face_nose_cap_fuselage = grunk::action("face_nose_cap_fuselage", "grocc::BRepBuilderAPI_MakeFace", nose_ogive_cap_srf_geom, face_fuselage_tol).output();
	auto face_nose_fuselage = grunk::action("face_nose_fuselage", "grocc::BRepBuilderAPI_MakeFace", nose_ogive_srf_geom, face_fuselage_tol).output();
	auto face_rear_cap_fuselage = grunk::action("face_rear_cap_fuselage", "grocc::BRepBuilderAPI_MakeFace", rear_surface_cap_geom, face_fuselage_tol).output();

// Combine the faces into a shell
// Create a solid from the shell
	auto solid_fuselage = grunk::script(
		{
			{"grocc::BRep_Builder", {"builder_fuselage"}, {}},
			{"grocc::TopoDS_Shell", {"shell_fuselage"}, {}},
			{"grocc::BRep_Builder::MakeShell", {}, {"builder_fuselage", "shell_fuselage"}},
			{"grocc::BRep_Builder::Add", {}, {"builder_fuselage", "shell_fuselage", face_cylinder_fuselage}},
			{"grocc::BRep_Builder::Add", {}, {"builder_fuselage", "shell_fuselage", face_rear_fuselage}},
			{"grocc::BRep_Builder::Add", {}, {"builder_fuselage", "shell_fuselage", face_nose_cap_fuselage}},
			{"grocc::BRep_Builder::Add", {}, {"builder_fuselage", "shell_fuselage", face_nose_fuselage}},
			{"grocc::BRep_Builder::Add", {}, {"builder_fuselage", "shell_fuselage", face_rear_cap_fuselage}},
			{"grocc::TopoDS_Solid", {"solid_fuselage"}, {}},
			{"grocc::BRep_Builder::MakeSolid", {}, {"builder_fuselage", "solid_fuselage"}},
			{"grocc::BRep_Builder::Add", {}, {"builder_fuselage", "solid_fuselage", "shell_fuselage"}},
		},
		{"solid_fuselage"}
	).output();

// write grunk recipe of solid_fuselage:
	grunk::write("solid_fuselage.grr", solid_fuselage);

// write solid_fuselage as stp file:
	auto solid_fuselage_name_stp = grunk::Feature("solid_fuselage_name_stp", "String", "solid_fuselage.stp");
	auto solid_fuselage_out = grunk::action("solid_fuselage_out", "geo::writeGeomEntityToStepFile", solid_fuselage, solid_fuselage_name_stp).output();
	solid_fuselage_out.value();

	auto fused_wing_fuselage = grunk::action("fused_wing_fuselage", "geo::fuse_two_solids", solid_wing, solid_fuselage).output();

// write grunk recipe of fused_wing_fuselage:
	grunk::write("fused_wing_fuselage.grr", fused_wing_fuselage);

// write fused_wing_fuselage as stp file:
	auto fused_wing_fuselage_name_stp = grunk::Feature("fused_wing_fuselage_name_stp", "String", "fused_wing_fuselage.stp");
	auto out = grunk::action("out", "geo::writeGeomEntityToStepFile", fused_wing_fuselage, fused_wing_fuselage_name_stp).output();
	out.value();

std::cout << "end of main" << std::endl;

return 0;
}
