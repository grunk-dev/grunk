from conans import ConanFile, CMake


class GrunkConan(ConanFile):
    name = "grunk"
    version = "0.1"
    license = "<Put the package license here>"
    author = "<Put your name here> <And your email here>"
    url = "<Package recipe repository url here, for issues about the package>"
    description = "<Description of Grunk here>"
    topics = ("<Put some tag here>", "<here>", "<and here>")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True], "fPIC": [True, False]}
    default_options = {"shared": True, "fPIC": True}
    generators = "cmake_find_package"
    requires = "jsoncpp/1.9.5", "boost/1.78.0", "parametric/0.1@paradigms/testing", "reflect/0.1@paradigms/testing"
    exports_sources = "*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        self.options["boost"].shared = False
        self.options["boost"].header_only = True

    def build(self):
        cmake = CMake(self)
        cmake.configure(source_folder=".")
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.configure(source_folder=".")
        cmake.install()

    def package_info(self):
        self.cpp_info.includedirs.append("include/grunk")
        self.cpp_info.libs = ["grunk"]
