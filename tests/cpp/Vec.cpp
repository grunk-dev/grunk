#include <gtest/gtest.h>

#include <grunk/grunk.hpp>

namespace {


}

class VecTest : public ::testing::Test
{
public:
    static void SetUpTestCase() {

        grunk::StdPlugin().init();

    }

    static void TearDownTestCase() {
        reflect::get_type_registry().clear();
    } 
};

TEST_F(VecTest, static_mode)
{
    auto x = grunk::Feature("x", 0.1);
    auto y = grunk::Feature("y", 0.2);
    auto z = grunk::vec("z", x, y);

    static_assert(std::is_same_v<std::decay_t<decltype(z.value())>, std::vector<double>>);

    EXPECT_EQ(z.value().size(), 2);
    EXPECT_EQ(z.value()[0], 0.1);
    EXPECT_EQ(z.value()[1], 0.2);

    x.access_value() = -0.1;
    EXPECT_FALSE(z.is_valid());
    EXPECT_EQ(z.value()[0], -0.1);
}

TEST_F(VecTest, dynamic_mode)
{
    auto x = grunk::Feature("x", "double", 0.1);
    auto y = grunk::Feature("y", "double", 0.2);
    auto z = grunk::vec("z", x, y);

    static_assert(std::is_same_v<std::decay_t<decltype(z.value())>, reflect::DynamicObject>);

    auto ret = z.value().as<std::vector<reflect::DynamicObject>>();
    EXPECT_EQ(ret.size(), 2);
    EXPECT_EQ(ret[0].as<double>(), 0.1);
    EXPECT_EQ(ret[1].as<double>(), 0.2);

    x.access_value() = -0.1;
    EXPECT_FALSE(z.is_valid());
    ret = z.value().as<std::vector<reflect::DynamicObject>>();
    EXPECT_EQ(ret[0].as<double>(), -0.1);
}