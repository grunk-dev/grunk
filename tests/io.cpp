#include <gtest/gtest.h>

#include <grunk/grunk.h>

using namespace grunk;

namespace {
    std::string double2str(double const& v) {
        return std::to_string(v);
    }
}

class IOTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {
        register_type<double>("double")
        .AddConstructor<double>()
        .AddMemberFunction(&double2str, "serialize");

        register_function(
            [](double const& l, double const& r){ return l+r;}, 
            "plus"
        );
    } 

    static void TearDownTestCase() {
        Reflect::GetTypeRegistry().clear();
    } 
};


TEST_F(IOTest, simple)
{
    auto a = Feature("a", 0.2);
    auto b = Feature("b", 0.1);
    auto c = eval("c", "plus", a, b)->get();
    auto d = eval("d", "plus", c, a)->get();

    std::string res = serialize(a,d,c,b);
    std::cout << res << std::endl;
}