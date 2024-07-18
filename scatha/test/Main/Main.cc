#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <catch2/internal/catch_istream.hpp>
#include <catch2/internal/catch_test_case_registry_impl.hpp>
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
               Opt(options.TestPasses, "passes")["--passes"](
                   "Run pass tests for the end to end test cases") |
               Opt(options.TestIdempotency, "idempotency")["--idempotency"](
                   "Run idempotency tests for the end to end test cases") |
               Opt(options.TestPipeline, "pipeline")["--pipeline"](
                   "Run pass tests for the end to end test cases for the "
                   "specfied pipeline") |
               Opt(options.PrintCodegen, "print codegen")["--print-cg"](
                   "Print codegen pipeline state for failed test cases");

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
    virtual void beginTest([[maybe_unused]] Catch::TestCaseInfo const& testInfo,
                           [[maybe_unused]] double progress) {}
    virtual void endTest([[maybe_unused]] Catch::TestCaseInfo const& testInfo,
                         [[maybe_unused]] double progress) {}
    virtual void endRun([[maybe_unused]] Catch::TestRunStats const& stats) {}
    virtual void assertionFailed() {}

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

    void endTest(Catch::TestCaseInfo const&, double progress) override {
        int newNumDots = (int)(progress * 79);
        printDots(std::max(0, newNumDots - numDots));
        numDots = newNumDots;
    }

    void assertionFailed() override { numDots = 0; }

    void endRun(Catch::TestRunStats const&) override { str << std::endl; }

    int numDots = 0;
};

struct ProgRepConsole: ProgRepBase {
    using ProgRepBase::ProgRepBase;

    ProgRepConsole(std::ostream& str): ProgRepBase(str) {
        tfmt::setTermFormattable(str);
    }

    void beginTest(Catch::TestCaseInfo const& testInfo,
                   double progress) override {
        clearLine();
        printProgressBar(progress, getWidth(), testInfo.name);
    }

    void endRun(Catch::TestRunStats const&) override {
        clearLine();
        printProgressBar(1, getWidth(), "");
        str << "\n";
    }

    void assertionFailed() override { clearLine(); }

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
        return std::clamp(tfmt::getWidth(std::cout).value_or(80), size_t{ 20 },
                          size_t{ 79 });
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
            impl->assertionFailed();
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
