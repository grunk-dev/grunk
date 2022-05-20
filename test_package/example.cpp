#include <iostream>

#include <grunk/plugins/PluginRegistry.h> 
#include <grunk/RuntimeObject.h>


int main(int argc, char* argv[]) {

    // Create engine and query info about loaded plugins
    grunk::PluginRegistry plugins(argv[1]);

    std::cout << "\n\nUnique plugins " << plugins.count() << ":\n";
    plugins.print_plugins();
    std::cout<<std::endl;

    // Create a dynamic object
    grunk::RuntimeObject x = grunk::make_rto("MyDouble", 0.5);
    std::cout<< x.Get("value").cast<double>() << std::endl;

    // setting and getting with runtiomeobjects
    grunk::RuntimeObject y = 0.25;
    x.Set("value", y);
    std::cout<< x.Get("value").cast<double>() << std::endl;

    // setting and getting by conversion with other types
    x.Set("value", 3.14);
    std::cout<< x.Get("value").cast<double>() << std::endl;

    // calling member functions
    x.Invoke("multiply", 2);
    std::cout<< x.Get("value").cast<double>() << std::endl;

    grunk::RuntimeObject z = 3;
    x.Invoke("multiply", z);
    std::cout<< x.Get("value").cast<double>() << std::endl;
}
