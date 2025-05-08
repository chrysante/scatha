#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <catch2/internal/catch_istream.hpp>
#include <catch2/internal/catch_test_case_registry_impl.hpp>
#include <catch2/reporters/catch_reporter_console.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/reporters/catch_reporter_streaming_base.hpp>
#include <range/v3/numeric.hpp>
#include <termfmt/termfmt.h>

#include "Main/Options.h"

using namespace scatha;
using namespace test;
using namespace tfmt::modifiers;

static Options options{};

Options const& test::getOptions() { return options; }

int main(int argc, char* argv[]) {
    Catch::Session session;

    using namespace Catch::Clara;
    auto cli = session.cli() |
               Opt(options.TestPasses)["--passes"](
                   "Run pass tests for the end to end test cases") |
               Opt(options.TestIdempotency)["--idempotency"](
                   "Run idempotency tests for the end to end test cases") |
               Opt(options.TestPipeline, "pipeline")["--pipeline"](
                   "Run pass tests for the end to end test cases for the "
                   "specfied pipeline") |
               Opt(options.PrintCodegen)["--print-cg"](
                   "Print codegen pipeline state for failed test cases") |
               Opt(options.NoJumpThreading)["--no-jump-threading"](
                   "Run the interpreter without jump threading");

    session.cli(cli);
    int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0) {
        return returnCode;
    }
    return session.run();
}

namespace {

struct ProgRepBase {
    ProgRepBase(std::ostream& str): str(str) {}

    virtual ~ProgRepBase() = default;
    virtual void beginTest(Catch::TestCaseInfo const& testInfo,
                           double progress) = 0;
    virtual void endTest(Catch::TestCaseInfo const& testInfo,
                         double progress) = 0;
    virtual void endRun(Catch::TestRunStats const& stats) = 0;
    virtual void assertionFailed(Catch::AssertionStats const& stats) = 0;

    std::ostream& str;
};

struct ProgRepSimple: ProgRepBase {
    using ProgRepBase::ProgRepBase;

    void printDots(int count) const {
        for (int i = 0; i < count; ++i) {
            str << '.';
        }
        str << std::flush;
    }

    void beginTest(Catch::TestCaseInfo const&, double) override {}

    void endTest(Catch::TestCaseInfo const&, double progress) override {
        int newNumDots = (int)(progress * 79);
        printDots(std::max(0, newNumDots - numDots));
        numDots = newNumDots;
    }

    void endRun(Catch::TestRunStats const&) override { str << std::endl; }

    void assertionFailed(Catch::AssertionStats const&) override { numDots = 0; }

    int numDots = 0;
};

struct ProgRepConsole: ProgRepBase {
    Catch::TestCaseInfo const* m_currentTestInfo = nullptr;
    int numFailedAssertionsInCurrentTest = 0;
    double m_progress = 0;

    using ProgRepBase::ProgRepBase;

    ProgRepConsole(std::ostream& str): ProgRepBase(str) {
        tfmt::setTermFormattable(str);
    }

    void beginTest(Catch::TestCaseInfo const& testInfo,
                   double progress) override {
        clearLine();
        printProgressBar(progress, getWidth(), testInfo.name);
        m_currentTestInfo = &testInfo;
        m_progress = progress;
        numFailedAssertionsInCurrentTest = 0;
    }

    void endTest(Catch::TestCaseInfo const&, double progress) override {
        m_currentTestInfo = nullptr;
        m_progress = progress;
    }

    void endRun(Catch::TestRunStats const&) override {
        clearLine();
        printProgressBar(1, getWidth(), "");
        str << "\n";
    }

    void assertionFailed(Catch::AssertionStats const& stats) override {
        clearLine();
        assert(m_currentTestInfo);
        if (numFailedAssertionsInCurrentTest == 0)
            str << tfmt::format(Red | Bold, "Failure in test case: ")
                << tfmt::format(Bold, m_currentTestInfo->name) << "\n";
        else
            str << "\n";
        ++numFailedAssertionsInCurrentTest;
        auto& result = stats.assertionResult;
        auto expr = result.getExpression();
        auto expandedExpr = result.getExpandedExpression();
        str << "    " << result.getTestMacroName() << "("
            << tfmt::format(Bold, expr) << ")\n";
        if (expr != expandedExpr) {
            str << "    "
                << tfmt::format(BrightGrey | Italic, "with expansion:\n");
            str << "    " << result.getTestMacroName() << "("
                << tfmt::format(Bold, expandedExpr) << ")\n";
        }
        str << "    " << tfmt::format(BrightGrey | Italic, "with message:")
            << "\n";
        for (auto& message: stats.infoMessages) {
            str << "        " << message.message << "\n";
        }
        tfmt::formatScope(BrightGrey | Italic, str, [&] {
            auto sourceInfo = result.getSourceInfo();
            str << "    In file "
                << tfmt::format(Reset, "\"", sourceInfo.file, "\"") << "\n";
            str << "    On line " << sourceInfo.line << "\n";
        });
        printProgressBar(m_progress, getWidth(), m_currentTestInfo->name);
    }

    void printProgressBar(double progress, size_t width,
                          std::string_view name) {
        std::stringstream sstr;
        sstr << std::setw(4) << (int)(progress * 100) << "% " << name;
        auto progStr = std::move(sstr).str();
        size_t barEnd = (size_t)(progress * width);
        static auto const InBarFmt = BGGreen | BrightWhite;
        static auto const OutBarFmt = BGGrey | BrightWhite;
        for (size_t i = 0; i < width; ++i) {
            bool inBar = i <= barEnd;
            auto& fmt = inBar ? InBarFmt : OutBarFmt;
            tfmt::FormatGuard guard(fmt, str);
            if (i < progStr.size()) {
                str << progStr[i];
            }
            else {
                str << ' ';
            }
        }
        str << "\n";
    }

    void clearLine() const { str << "\33[1A\33[2K\r"; }

    size_t getWidth() const {
        size_t width = tfmt::getWidth(std::cout).value_or(80);
        return std::max(size_t{ 20 }, width);
    }
};

static size_t getNumTestCases(Catch::IConfig const& config) {
    auto& allTests = Catch::getAllTestCasesSorted(config);
    auto matches = config.testSpec().matchesByFilter(allTests, config);
    size_t numTests =
        ranges::accumulate(matches, size_t{ 0 }, ranges::plus(),
                           [](auto& match) { return match.tests.size(); });
    if (numTests > 0) {
        return numTests;
    }
    return allTests.size();
}

struct ProgressReporter: Catch::StreamingReporterBase {
    using StreamingReporterBase::StreamingReporterBase;

    void testRunStarting(Catch::TestRunInfo const&) override {
        numTestsTotal = getNumTestCases(*m_config);
        if (tfmt::isTerminal(std::cout)) {
            impl = std::make_unique<ProgRepConsole>(m_stream);
        }
        else {
            impl = std::make_unique<ProgRepSimple>(m_stream);
        }
    }

    static std::string getDescription() { return "Progress reporter"; }

    void testCaseStarting(Catch::TestCaseInfo const& info) override {
        impl->beginTest(info, (double)numTestsRun / numTestsTotal);
        ++numTestsRun;
    }

    void testCaseEnded(Catch::TestCaseStats const& stats) override {
        impl->endTest(*stats.testInfo, (double)numTestsRun / numTestsTotal);
    }

    void assertionEnded(Catch::AssertionStats const& stats) override {
        if (!stats.assertionResult.isOk()) {
            impl->assertionFailed(stats);
        }
    }

    void testRunEnded(Catch::TestRunStats const& stats) override {
        impl->endRun(stats);
    }

    std::unique_ptr<ProgRepBase> impl;
    size_t numTestsRun = 0;
    size_t numTestsTotal = 0;
};

} // namespace

CATCH_REGISTER_REPORTER("progress", ProgressReporter)
