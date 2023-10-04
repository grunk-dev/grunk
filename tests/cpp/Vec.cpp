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
    EXPECT_EQ(z.value().size(), 2);
    EXPECT_EQ(z.value()[0], 0.1);
    EXPECT_EQ(z.value()[1], 0.2);

    x.access_value() = -0.1;
    EXPECT_FALSE(z.is_valid());
    EXPECT_EQ(z.value()[0], -0.1);
}