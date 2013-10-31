// test.cpp
#include <UnitTest++.h>
#include <TestRunner.h>
#include <TestReporterStdout.h>
#include <iostream>
#include <string.h>

struct MatchTestName
{
    const char* testName;

    MatchTestName(const char* name) : testName(name)
    {
    }

    bool operator()(const UnitTest::Test* test) const
    {
        return strcmp(testName, test->m_details.testName) == 0;
    }
};


int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "No tests specified" << std::endl;
        return -1;
    }
    else
    {
        std::cout << "Running: " << argv[1] << std::endl;
    }

    MatchTestName predicate(argv[1]);
    UnitTest::TestReporterStdout reporter;
    UnitTest::TestRunner runner(reporter);

    return runner.RunTestsIf(UnitTest::Test::GetTestList(), NULL, predicate, 0);
}