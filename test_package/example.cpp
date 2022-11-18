#include <ios>
#include <iostream>

#include <grunk/grunk.h>

double add(double const& l, double const& r) {
    std::cout << "Adding " << l << " and " << r << std::endl;
    return l + r;
}

int main(int argc, char* argv[]) {

    {
        grunk::Feature x("x", 1.2);
        grunk::Feature y("y", 15.2);
        grunk::Feature z("z", 25.6);

        auto a = grunk::eval("a", &add, x, y)->get();
        auto b = grunk::eval("b", &add, a, z)->get();

        std::cout << "Until here, nothing has happened" << std::endl;

        // calculations are performed when the output is queried
        std::cout << b.value() << std::endl;

        // when the output is queried again, the result is retrieved from cache
        std::cout << b.value() << std::endl;

        // changing z only requires the re-calculation of b, not of a
        std::cout << "Changing z ...\n";
        z.access_value() = 24.2;
        std::cout  << b.value() << std::endl;

        // changing x only requires the re-calculation of both a and b
        std::cout << "Changing x ...\n";
        x.access_value() = 2.6;
        std::cout  << b.value() << std::endl;
    }

    // load plugins
    auto& plugins = grunk::get_plugin_registry();
    plugins.prepend_path(argv[1]);
    plugins.load_all();

    {
        std::cout << "\n\nUnique plugins " << plugins.count() << ":\n";
        plugins.print_plugins();
        std::cout<<std::endl;

        // use plugin types
        grunk::Feature x("x", "MyDouble", 4.3);
        grunk::Feature y("y", "MyDouble", 3.3);
        grunk::Feature z("z", "MyDouble", 2.0);

        // use plugin functions
        auto a = grunk::eval("a", "add", x, y)->get();
        auto b = grunk::eval("b", "multiply", a, z)->get();

        auto b_result = reflect::cast<double>(b.value().get("value"));
        std::cout << b_result << std::endl;

        // write to grunk file
        grunk::write("simple.gk", b);
    }

    {
        std::cout << "Reading from file ...\n";
        auto features = grunk::read("simple.gk");
        auto b = features.at("b");
        auto b_result = reflect::cast<double>(b.value().get("value"));
        std::cout << b_result << std::endl;
    }

}
