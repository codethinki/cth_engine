#include "cth_engine/src/vulkan/utility/format/cth_vk_format_string.hpp"

#include <gtest/gtest.h>


namespace cth::vk::fmt {

struct TestStruct {
    int i0, i1, i2, i3, i4, i5, i6, i7, i8, i9, i10, i11, i12;
};

struct TestStruct2 {
    int i0, i1, i2, i3, i4, i5, i6, i7, i8, i9;
    int i10, i11, i12, i13, i14, i15, i16, i17, i18, i19;
    int i20, i21, i22, i23, i24, i25, i26, i27, i28, i29;
    int i30, i31, i32, i33, i34, i35, i36, i37, i38, i39;
    int i40, i41, i42, i43, i44, i45, i46, i47, i48, i49;
    int i50, i51, i52, i53, i54, i55, i56, i57, i58, i59;
    int i60, i61, i62, i63, i64, i65, i66, i67, i68, i69;
    int i70, i71, i72, i73, i74, i75, i76, i77, i78, i79;
    int i80, i81, i82, i83, i84, i85, i86, i87, i88, i89;
    int i90, i91, i92, i93, i94, i95, i96, i97, i98, i99;
    int i100, i101;
};

TEST(cth_vk_fmt_format_string, struct12) {
    auto arr = generate_format_string<TestStruct>("TestStruct");
    std::string result{arr.begin(), arr.end() - 1};
    std::string expected = "TestStruct{{{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}, {10}, {11}, {12}}}";

    ASSERT_EQ(0, arr.back());
    ASSERT_EQ(result, expected);
}
TEST(cth_vk_fmt_format_string, tuple102) {
    auto arr = generate_format_string<TestStruct2>("TestStruct2");
    std::string expected = 
        "TestStruct2{{{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}, "
        "{10}, {11}, {12}, {13}, {14}, {15}, {16}, {17}, {18}, {19}, "
        "{20}, {21}, {22}, {23}, {24}, {25}, {26}, {27}, {28}, {29}, "
        "{30}, {31}, {32}, {33}, {34}, {35}, {36}, {37}, {38}, {39}, "
        "{40}, {41}, {42}, {43}, {44}, {45}, {46}, {47}, {48}, {49}, "
        "{50}, {51}, {52}, {53}, {54}, {55}, {56}, {57}, {58}, {59}, "
        "{60}, {61}, {62}, {63}, {64}, {65}, {66}, {67}, {68}, {69}, "
        "{70}, {71}, {72}, {73}, {74}, {75}, {76}, {77}, {78}, {79}, "
        "{80}, {81}, {82}, {83}, {84}, {85}, {86}, {87}, {88}, {89}, "
        "{90}, {91}, {92}, {93}, {94}, {95}, {96}, {97}, {98}, {99}, "
        "{100}, {101}}}";

    std::string result{arr.begin(), arr.end() - 1};
    ASSERT_EQ(0, arr.back());
    ASSERT_EQ(result, expected);
}
}
