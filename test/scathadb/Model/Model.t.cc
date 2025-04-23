#include <chrono>
#include <tuple>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm.hpp>
#include <scatha/Invocation/CompilerInvocation.h>
#include <svm/Exceptions.h>

#include <scathadb/Model/Events.h>
#include <scathadb/Model/Model.h>
#include <scathadb/Util/Messenger.h>

namespace {

template <typename Dur = std::chrono::high_resolution_clock::duration>
static void waitWithTimeout(std::predicate auto condition,
                            Dur duration = std::chrono::seconds(1)) {
    auto start = std::chrono::high_resolution_clock::now();
    auto elapsed = [&] {
        return std::chrono::high_resolution_clock::now() - start;
    };
    while (true) {
        if (std::invoke(condition)) return;
        if (elapsed() > duration) throw std::runtime_error("Timeout");
        std::this_thread::yield();
    }
}

template <typename State>
struct Notifier {
    std::condition_variable cv;
    std::mutex mutex;
    State state;

    void notify(std::invocable<State&> auto setter) {
        std::unique_lock lock(mutex);
        std::invoke(setter, state);
        lock.unlock();
        cv.notify_one();
    }

    void waitForAndReset(std::predicate<State const&> auto condition) {
        waitFor(condition);
        state = {};
    }

    void waitFor(std::predicate<State const&> auto condition) {
        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return std::invoke(condition, state); });
    }
};

struct CommState {
    bool terminated = false, killed = false;
    bool haveEvent = false;

    bool finished() const { return terminated || killed; }
};

struct Comm {
    std::unique_ptr<Notifier<CommState>> notifier;
    std::shared_ptr<sdb::Messenger> messenger;
};

struct MockSourceFile {
    // This doesn't need to be an existing path, it's just a name for a mocked
    // file
    std::filesystem::path path;

    // Contents of the mocked source file
    std::string contents;
};

} // namespace

static Comm makeComm() {
    auto callback = [](sdb::Messenger& messenger) { messenger.flush(); };
    auto messenger = std::make_shared<sdb::Messenger>(callback);
    auto notifier = std::make_unique<Notifier<CommState>>();
    messenger->listen([notifier = notifier.get()](sdb::ProcessTerminated) {
        notifier->notify([](CommState& state) { state.terminated = true; });
    });
    messenger->listen([notifier = notifier.get()](sdb::ProcessKilled) {
        notifier->notify([](CommState& state) { state.killed = true; });
    });
    return { std::move(notifier), std::move(messenger) };
}

static scatha::Target makeTarget(
    std::span<MockSourceFile const> sourceFileList,
    scatha::Asm::LinkerOptions linkerOptions = {}) {
    scatha::CompilerInvocation inv(scatha::TargetType::Executable,
                                   "test-program");
    for (auto& [path, contents]: sourceFileList)
        inv.addInput(scatha::SourceFile::make(contents, path));
    inv.setFrontend(scatha::FrontendType::Scatha);
    inv.generateDebugInfo(true);
    inv.setLinkerOptions(linkerOptions);
    auto result = inv.run();
    if (!result) throw std::runtime_error("Compilation failed");
    return std::move(result).value();
}

static sdb::Model makeModel(std::shared_ptr<sdb::Messenger> messenger,
                            std::vector<MockSourceFile> sourceFileList,
                            scatha::Asm::LinkerOptions linkerOptions = {}) {
    auto target = makeTarget(sourceFileList, linkerOptions);
    auto sourceFileLoader = [&](std::filesystem::path const& path) {
        auto itr = ranges::find(sourceFileList, path, &MockSourceFile::path);
        if (itr == sourceFileList.end())
            throw std::invalid_argument("Cannot find source file");
        return sdb::SourceFile(path, itr->contents);
    };
    sdb::Model model(std::move(messenger));
    model.loadProgram(target.binary(), {}, target.debugInfo(),
                      sourceFileLoader);
    return model;
}

TEST_CASE("Print test", "[model]") {
    auto [notifier, messenger] = makeComm();
    auto& state = notifier->state;
    auto source = R"(
/* 2 */ fn main() {
/* 3 */     __builtin_putstr("Hello");
/* 4 */     __builtin_putstr(" World");
/* 5 */     __builtin_putstr("\n");
/* 6 */ }
)";
    auto model = makeModel(messenger, { { "test-file.sc", source } });
    SECTION("Run uninterrupted") {
        model.startExecution();
        notifier->waitFor(&CommState::finished);
        CHECK(state.terminated);
        CHECK(!state.killed);
        CHECK(model.standardout().str().starts_with("Hello World\n"));
    }
    SECTION("Breakpoints") {
        sdb::BreakEvent lastBreakEvent;
        messenger->listen([&](sdb::BreakEvent event) {
            lastBreakEvent = event;
            notifier->notify([](CommState& state) { state.haveEvent = true; });
        });
        model.toggleSourceBreakpoint({ .file = 0, .line = 4 });
        model.startExecution();
        notifier->waitForAndReset(&CommState::haveEvent);
        CHECK(lastBreakEvent.state == sdb::BreakState::Paused);
        CHECK(!lastBreakEvent.exception.hasValue());
        CHECK(model.standardout().str() == "Hello");
        model.stepSourceLine();
        notifier->waitForAndReset(&CommState::haveEvent);
        CHECK(model.standardout().str() == "Hello World");
        model.toggleExecution();
        notifier->waitFor(&CommState::finished);
        CHECK(state.terminated);
        CHECK(!state.killed);
        CHECK(model.standardout().str().starts_with("Hello World\n"));
    }
}

TEST_CASE("Memory error test", "[model]") {
    auto [notifier, messenger] = makeComm();
    auto& state = notifier->state;
    auto source = R"(
/* 2 */ fn main() -> int {
/* 3 */     let p: *int = null;
/* 4 */     return *p;
/* 5 */ }
)";
    auto model = makeModel(messenger, { { "test-file.sc", source } });
    sdb::BreakEvent lastBreakEvent;
    messenger->listen([&](sdb::BreakEvent event) {
        lastBreakEvent = event;
        notifier->notify([](CommState& state) { state.haveEvent = true; });
    });
    model.startExecution();
    notifier->waitForAndReset(&CommState::haveEvent);
    auto* memError = lastBreakEvent.exception.as<svm::MemoryAccessError>();
    REQUIRE(memError);
    CHECK(memError->reason() == svm::MemoryAccessError::MemoryNotAllocated);
    model.stopExecution();
    notifier->waitFor(&CommState::finished);
    CHECK(state.killed);
}

TEST_CASE("Multifile breakpoints", "[model]") {
    auto [notifier, messenger] = makeComm();
    auto& state = notifier->state;
    auto sourceMain = R"(
/* 2 */ fn main() {
/* 3 */     myPrint("Start");
/* 4 */     myPrint("Continue");
/* 5 */     myPrint("Done");
/* 6 */ }
)";
    auto sourcePrint = R"(
/* 2 */ fn myPrint(text: *str) {
/* 3 */     __builtin_putln(text);
/* 4 */ }
)";
    auto model = makeModel(messenger, { { "main.sc", sourceMain },
                                        { "print.sc", sourcePrint } });
    sdb::BreakEvent lastBreakEvent;
    messenger->listen([&](sdb::BreakEvent event) {
        lastBreakEvent = event;
        notifier->notify([](CommState& state) { state.haveEvent = true; });
    });
    model.toggleSourceBreakpoint({ .file = 0, .line = 4 });
    model.toggleSourceBreakpoint({ .file = 1, .line = 3 });

    model.startExecution();
    notifier->waitForAndReset(&CommState::haveEvent);
    CHECK(lastBreakEvent.state == sdb::BreakState::Paused);
    auto sourceLoc =
        model.sourceDebug().sourceMap().toSourceLoc(lastBreakEvent.ipo);
    CHECK(sourceLoc.value().line == sdb::SourceLine{ .file = 1, .line = 3 });
    CHECK(model.standardout().str().empty());

    model.toggleExecution();
    notifier->waitForAndReset(&CommState::haveEvent);
    CHECK(lastBreakEvent.state == sdb::BreakState::Paused);
    sourceLoc = model.sourceDebug().sourceMap().toSourceLoc(lastBreakEvent.ipo);
    CHECK(sourceLoc.value().line == sdb::SourceLine{ .file = 0, .line = 4 });
    CHECK(model.standardout().str() == "Start\n");

    model.toggleExecution();
    notifier->waitForAndReset(&CommState::haveEvent);
    CHECK(lastBreakEvent.state == sdb::BreakState::Paused);
    sourceLoc = model.sourceDebug().sourceMap().toSourceLoc(lastBreakEvent.ipo);
    CHECK(sourceLoc.value().line == sdb::SourceLine{ .file = 1, .line = 3 });
    CHECK(model.standardout().str() == "Start\n");

    model.toggleExecution();
    notifier->waitForAndReset(&CommState::haveEvent);
    CHECK(lastBreakEvent.state == sdb::BreakState::Paused);
    sourceLoc = model.sourceDebug().sourceMap().toSourceLoc(lastBreakEvent.ipo);
    CHECK(sourceLoc.value().line == sdb::SourceLine{ .file = 1, .line = 3 });
    CHECK(model.standardout().str() == "Start\nContinue\n");

    model.toggleExecution();
    notifier->waitFor(&CommState::finished);
    CHECK(state.terminated);
    CHECK(model.standardout().str().starts_with("Start\nContinue\nDone\n"));
}

static std::atomic_bool* g_host_callback_called = nullptr;

extern "C" void live_patching_host_callback() {
    g_host_callback_called->store(true);
}

TEST_CASE("Live patching breakpoints", "[model]") {
    auto [notifier, messenger] = makeComm();
    auto& state = notifier->state;
    auto source = R"(
/* 2 */ extern "C" fn live_patching_host_callback() -> void;
/* 3 */ fn main() -> int {
/* 4 */     while true {
/* 5 */         live_patching_host_callback();
/* 6 */     }
/* 7 */     __builtin_putstr("Unreachable");
/* 8 */ }
)";
    auto model = makeModel(messenger, { { "test-file.sc", source } },
                           scatha::Asm::LinkerOptions{ .searchHost = true });
    sdb::BreakEvent lastBreakEvent;
    messenger->listen([&](sdb::BreakEvent event) {
        lastBreakEvent = event;
        notifier->notify([](CommState& state) { state.haveEvent = true; });
    });
    std::atomic_bool host_callback_called = false;
    g_host_callback_called = &host_callback_called;
    utl::scope_guard reset = [] { g_host_callback_called = nullptr; };
    model.startExecution();
    waitWithTimeout([&] { return host_callback_called.load(); });
    sdb::SourceLine callLine = { .file = 0, .line = 5 };
    model.toggleSourceBreakpoint(callLine);
    notifier->waitForAndReset(&CommState::haveEvent);
    CHECK(lastBreakEvent.state == sdb::BreakState::Paused);
    auto sourceLoc =
        model.sourceDebug().sourceMap().toSourceLoc(lastBreakEvent.ipo);
    CHECK(sourceLoc.value().line.line == 5);
    model.toggleSourceBreakpoint(callLine);
    model.toggleExecution();
    waitWithTimeout([&] { return host_callback_called.load(); });
    model.stopExecution();
    notifier->waitFor(&CommState::finished);
    CHECK(state.killed);
}
