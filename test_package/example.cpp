#include <ios>
#include <iostream>

#include <grunk/plugins/PluginRegistry.h> 
#include <grunk/FunctionRegistry.h>


int main(int argc, char* argv[]) {

    // Create engine and query info about loaded plugins
    grunk::PluginRegistry plugins(argv[1]);

    std::cout << "\n\nUnique plugins " << plugins.count() << ":\n";
    plugins.print_plugins();
    std::cout<<std::endl;

    // set two parameters
    grunk::Feature l("MyDouble", 3.3);
    grunk::Feature r("MyDouble", 2.2);

    std::cout << "l = " << l.GetAs<double>("value")
              << ", r = " << r.GetAs<double>("value") 
              << std::endl;

    // evaluate some functions on the parameters
    std::cout << "a = l + r, " 
              << "b = a + r" << std::endl;

    auto a = grunk::Eval("add", {l, r})->get();
    auto b = grunk::Eval("add", {a, r})->get();

    // nothing should have happened yet
    std::cout << std::boolalpha << "a.is_valid() = " << a.is_valid() << std::endl;
    std::cout << std::boolalpha << "b.is_valid() = " << b.is_valid() << std::endl;

    // querying a should evaulate the first addition
    std::cout << "a = " << a.Value().GetAs<double>("value") << std::endl;

    // a is valid, b is invalid
    std::cout << std::boolalpha << "a.is_valid() = " << a.is_valid() << std::endl;
    std::cout << std::boolalpha << "b.is_valid() = " << b.is_valid() << std::endl;

    // querying b should evaluate second addition
    std::cout << "b = " << b.Value().GetAs<double>("value") << std::endl;

    std::cout << std::boolalpha << "a.is_valid() = " << a.is_valid() << std::endl;
    std::cout << std::boolalpha << "b.is_valid() = " << b.is_valid() << std::endl;

    // reseting a root parameter invalidates feature tree
    l.AccessValue().Set("value", 0.5);
    std::cout << "l = " << l.GetAs<double>("value") << std::endl;

    std::cout << std::boolalpha << "a.is_valid() = " << a.is_valid() << std::endl;
    std::cout << std::boolalpha << "b.is_valid() = " << b.is_valid() << std::endl;

    // querying b should evaluate both additions
    std::cout << "b = " << b.Value().GetAs<double>("value") << std::endl;

    std::cout << std::boolalpha << "a.is_valid() = " << a.is_valid() << std::endl;
    std::cout << std::boolalpha << "b.is_valid() = " << b.is_valid() << std::endl;

    
}
