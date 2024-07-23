from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout,CMakeDeps, CMakeToolchain
from conan.tools.files import load
import re

def get_version(conanfile = None):
    try:
        content = load(conanfile, "CMakeLists.txt")
        version = re.search(r"project\(grunk VERSION (.*)\)", content).group(1)
        return version.strip()
    except Exception as e:
        return None

class GrunkConan(ConanFile):
    name = "grunk"
    version = get_version()
    license = "<Put the package license here>"
    author = "<Put your name here> <And your email here>"
    url = "<Package recipe repository url here, for issues about the package>"
    description = "<Description of Grunk here>"
    topics = ("<Put some tag here>", "<here>", "<and here>")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True], "fPIC": [True, False]}
    default_options = {"shared": True, "fPIC": True}
    requires = "yaml-cpp/0.8.0", "boost/1.78.0", "muparser/2.3.4", "parametric/0.3.4", "reflect/0.2.0"
    exports_sources = "grunk*", "CMakeLists.txt", "docs*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        self.options["boost"].shared = False
        self.options["boost"].header_only = True

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        cmake = CMakeDeps(self)
        cmake.generate()
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.includedirs.append("include/grunk")
        self.cpp_info.libs = ["grunk"]
