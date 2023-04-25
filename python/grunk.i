#pragma SWIG nowarn=317 // Disable warning: Specialization of non-template
%module grunk
%{
     #include <grunk/plugins/PluginRegistry.hpp>
%}
%include <grunk/plugins/PluginRegistry.hpp>