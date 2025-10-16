# create a library and add source files
add_library(grunk_recipe INTERFACE)
add_library(grunk::recipe ALIAS grunk_recipe)