#include <gtest/gtest.h>
#include "hal/storage.h"

using sigurdos::hal::detail::buffer_is_erased;

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(StorageTest, EmptyBufferIsErased)
{
    EXPECT_TRUE(buffer_is_erased(nullptr, 0));
}

TEST(StorageTest, AllFFBufferIsErased)
{
    const uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_TRUE(buffer_is_erased(data, sizeof(data)));
}

TEST(StorageTest, AnyProgrammedByteIsNotErased)
{
    const uint8_t data[] = {0xFF, 0xFF, 0x00, 0xFF};
    EXPECT_FALSE(buffer_is_erased(data, sizeof(data)));
}

TEST(StorageTest, NullNonEmptyBufferIsNotErased)
{
    EXPECT_FALSE(buffer_is_erased(nullptr, 1));
}
