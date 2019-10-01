/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#include <AzTest/AzTest.h>
#include <AzTest/Utils.h>

class AzTestRunnerTest
    : public ::testing::Test
{
};

TEST_F(AzTestRunnerTest, Sanity_Pass)
{
    ASSERT_EQ(1, 1);
}

//-------------------------------------------------------------------------------------------------
struct EndsWithParam
{
    EndsWithParam(std::string arg, std::string ending, bool expected)
        : m_arg(arg)
        , m_ending(ending)
        , m_expected(expected)
    {
    }

    const std::string m_arg;
    const std::string m_ending;
    const bool m_expected;
};

void PrintTo(const EndsWithParam& p, std::ostream* os)
{
    *os << "arg:" << p.m_arg << ", ending:" << p.m_ending << ", expected:" << p.m_expected;
}

class EndsWithTest
    : public ::testing::TestWithParam<EndsWithParam>
{
};

TEST_P(EndsWithTest, CallEndsWith)
{
    EndsWithParam p = GetParam();
    bool b = AZ::Test::EndsWith(p.m_arg, p.m_ending);
    ASSERT_EQ(p.m_expected, b);
}

INSTANTIATE_TEST_CASE_P(All, EndsWithTest, ::testing::Values(
    EndsWithParam("foo.dll", ".dll", true),
    EndsWithParam("foo.dll", ".dxx", false),
    EndsWithParam("abcdef", "bcd", false), // value found in middle
    EndsWithParam("a", "bcd", false), // pattern too long
    EndsWithParam("abc", "", true), // empty pattern
    EndsWithParam("", "abc", false), // empty value
    EndsWithParam("", "", true) // both empty
));


//-------------------------------------------------------------------------------------------------
struct RemoveParam
{
    std::vector<std::string> before;
    int startIndex;
    int endIndex;
    std::vector<std::string> expected;
};

std::ostream& operator<< (std::ostream& os, const RemoveParam& param)
{
    os << "before:{";
    bool first = true;
    for (auto b : param.before)
    {
        if (!first)
        {
            os << ", ";
        }
        os << b;
        first = false;
    }
    os << "}, startIndex:" << param.startIndex << ", endIndex:" << param.endIndex << ", expected:{";
    first = true;
    for (auto e : param.expected)
    {
        if (!first)
        {
            os << ", ";
        }
        os << e;
        first = false;
    }
    os << "}";
    return os;
}

class RemoveParametersTest 
    : public ::testing::TestWithParam<RemoveParam>
{};

TEST_P(RemoveParametersTest, Foo)
{
    const RemoveParam& p = GetParam();

    // create "main"-like parameters
    int argc = (int)p.before.size();
    char** argv = new char*[argc];
    for (int i = 0; i < argc; i++)
    {
        argv[i] = const_cast<char*>(p.before[i].c_str());
    }

    AZ::Test::RemoveParameters(argc, argv, p.startIndex, p.endIndex);

    ASSERT_EQ(p.expected.size(), argc);
    for (int i = 0; i < p.expected.size(); i++)
    {
        std::string actual(argv[i]);
        EXPECT_EQ(p.expected[i], actual);
    }

    // everything beyond the end of the original data is nulled out if it was removed
    for (int j = (int)p.expected.size(); j < p.before.size(); j++)
    {
        EXPECT_TRUE(nullptr == argv[j]);
    }

    delete[] argv;
}

INSTANTIATE_TEST_CASE_P(All, RemoveParametersTest, ::testing::Values(
    RemoveParam { { "a", "b" }, 0, 0, { "b" } } // remove from start
    ,RemoveParam { { "a", "b" }, 1, 1, { "a" } } // remove from end
    ,RemoveParam { { "a", "b", "c" }, 1, 1, { "a", "c" } } // remove from middle
    ,RemoveParam { { "a", "b", "c" }, 1, 99, { "a" } } // remove beyond end
    ,RemoveParam { { "a", "b", "c" }, -99, 1, { "c" } } // remove before begin
    ,RemoveParam { { "a", "b", "c" }, -99, 99, { } } // remove all
    ,RemoveParam { { "a", "b", "c" }, 2, 0, {"a", "b", "c"} } // inverted range doesn't remove anything
));


class AzTestRunnnerGlobalParamsTest
    : public ::testing::Test
{
public:
    void SetUp() override
    {
        ::testing::Test::SetUp();
        m_priorFilter = ::testing::GTEST_FLAG(filter);
        ::testing::GTEST_FLAG(filter) = "*"; // emulate no command line filter args
    }

    void TearDown() override
    {
        ::testing::GTEST_FLAG(filter) = m_priorFilter;
        ::testing::Test::TearDown();
    }

    std::string m_priorFilter; // std::string to emulate same environment as gtest, not AZ
};

// saves args into compatible with other platforms format (clang is picky)
// but also restores GTEST_FLAG(filter) before and after applying changes to them, 
// allowing us to test the effect params have on GTEST_FLAG(filter)
class ScopedArgs
{
public:
    ScopedArgs(int argc, const char** const argv)
    {
        m_savedParams = ::testing::GTEST_FLAG(filter);
        m_argc = argc;
        m_argv = new char*[argc];
        for (int i = 0; i < m_argc; i++)
        {
            m_argv[i] = const_cast<char*>(argv[i]);
        }
    }
    ~ScopedArgs()
    {
        delete[] m_argv;
        ::testing::GTEST_FLAG(filter) = m_savedParams;
    }

    int m_argc = 0;
    char **m_argv = nullptr;
    std::string m_savedParams; // intentional std::string, exists outside of memory alloc area.

private:
    ScopedArgs() = delete;
    AZ_DISABLE_COPY_MOVE(ScopedArgs);
};

TEST_F(AzTestRunnnerGlobalParamsTest, ApplyGlobalParameters_NothingSpecial_RemainsUnchanged)
{
    const char* argv[] = { "hello", "--world", "test" };
    ScopedArgs args(static_cast<int>(AZ_ARRAY_SIZE(argv)), argv );

    AZ::Test::ApplyGlobalParameters(&args.m_argc, args.m_argv);
    EXPECT_EQ(args.m_argc, 3);
}


TEST_F(AzTestRunnnerGlobalParamsTest, ApplyGlobalParameters_WithSuite_AddsSuiteToFilter)
{
    const char* argv[] = {"hello", "--suite", "MySuite", "world"};
    ScopedArgs args(static_cast<int>(AZ_ARRAY_SIZE(argv)), argv );
    AZ::Test::ApplyGlobalParameters(&args.m_argc, args.m_argv);

    ASSERT_EQ(args.m_argc, 2);
    EXPECT_STREQ(args.m_argv[0], "hello");
    EXPECT_STREQ(args.m_argv[1], "world");

    EXPECT_STREQ(::testing::GTEST_FLAG(filter).c_str(), "Suite_MySuite*");
}

TEST_F(AzTestRunnnerGlobalParamsTest, ApplyGlobalParameters_NoSuite_AddsExclusion)
{
    const  char* argv[] = {"hello", "world"};
    ScopedArgs args(static_cast<int>(AZ_ARRAY_SIZE(argv)), argv );
    AZ::Test::ApplyGlobalParameters(&args.m_argc, args.m_argv);

    ASSERT_EQ(args.m_argc, 2);
    EXPECT_STREQ(args.m_argv[0], "hello");
    EXPECT_STREQ(args.m_argv[1], "world");

    EXPECT_STREQ(::testing::GTEST_FLAG(filter).c_str(), "-Suite_*");
}

TEST_F(AzTestRunnnerGlobalParamsTest, ApplyGlobalParameters_PreExistingFilter_WithSuite_AddsSuiteToFilter)
{
    const char* argv[] = {"hello", "--suite", "MySuite", "world"};
    ScopedArgs args(static_cast<int>(AZ_ARRAY_SIZE(argv)), argv );
    
    ::testing::GTEST_FLAG(filter) = "*CertainTestsOnly*";
    AZ::Test::ApplyGlobalParameters(&args.m_argc, args.m_argv);

    ASSERT_EQ(args.m_argc, 2);
    EXPECT_STREQ(args.m_argv[0], "hello");
    EXPECT_STREQ(args.m_argv[1], "world");

    EXPECT_STREQ(::testing::GTEST_FLAG(filter).c_str(), "Suite_MySuite*:*CertainTestsOnly*");
}

TEST_F(AzTestRunnnerGlobalParamsTest, ApplyGlobalParameters_PreExistingFilter_NoSuite_AddsExclusion)
{
    const char* argv[] = {"hello", "world"};

    ScopedArgs args(static_cast<int>(AZ_ARRAY_SIZE(argv)), argv );

    ::testing::GTEST_FLAG(filter) = "*CertainTestsOnly*";
    AZ::Test::ApplyGlobalParameters(&args.m_argc, args.m_argv);

    ASSERT_EQ(args.m_argc, 2);
    EXPECT_STREQ(args.m_argv[0], "hello");
    EXPECT_STREQ(args.m_argv[1], "world");

    EXPECT_STREQ(::testing::GTEST_FLAG(filter).c_str(), "*CertainTestsOnly*:-Suite_*");
}


// make sure our built in param parsing works as expected
TEST_F(AzTestRunnnerGlobalParamsTest, ApplyGlobalParameters_PreExistingExclusionFilter_NoSuite_AddsExclusion)
{
    const char* argv[] = {"hello", "world"};

    ScopedArgs args(static_cast<int>(AZ_ARRAY_SIZE(argv)), argv );

    ::testing::GTEST_FLAG(filter) = "-*CertainTestsOnly*";

    AZ::Test::ApplyGlobalParameters(&args.m_argc, args.m_argv);

    ASSERT_EQ(args.m_argc, 2);
    EXPECT_STREQ(args.m_argv[0], "hello");
    EXPECT_STREQ(args.m_argv[1], "world");

    EXPECT_STREQ(::testing::GTEST_FLAG(filter).c_str(), "-*CertainTestsOnly*:Suite_*");
}


TEST_F(AzTestRunnnerGlobalParamsTest, ApplyGlobalParameters_PreExistingExclusionFilter_WithSuite_AddsSuite)
{
    const char* argv[] = {"hello", "--suite", "MySuite", "world"};

    ScopedArgs args(static_cast<int>(AZ_ARRAY_SIZE(argv)), argv );

    ::testing::GTEST_FLAG(filter) = "-*CertainTestsOnly*";
    AZ::Test::ApplyGlobalParameters(&args.m_argc, args.m_argv);

    ASSERT_EQ(args.m_argc, 2);
    EXPECT_STREQ(args.m_argv[0], "hello");
    EXPECT_STREQ(args.m_argv[1], "world");

    EXPECT_STREQ(::testing::GTEST_FLAG(filter).c_str(), "Suite_MySuite*:-*CertainTestsOnly*");
}