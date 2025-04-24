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
    void notify(std::invocable<State&> auto setter) {
        std::unique_lock lock(mutex);
        std::invoke(setter, state);
        lock.unlock();
        cv.notify_one();
    }

    template <typename T, typename U = T>
    void notify(T(State::* member), U&& value) {
        notify([&](State& state) { state.*member = std::forward<U>(value); });
    }

    [[nodiscard]] State wait(std::predicate<State const&> auto condition) {
        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return std::invoke(condition, state); });
        return std::exchange(state, State{});
    }

private:
    std::condition_variable cv;
    std::mutex mutex;
    State state;
};

struct CommState {
    bool terminated = false, killed = false;
    std::optional<sdb::BreakEvent> breakEvent;

    bool haveBreakEvent() const { return breakEvent.has_value(); }
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
    auto messenger = sdb::Messenger::Make(callback);
    auto notifier = std::make_unique<Notifier<CommState>>();
    messenger->listen([notifier = notifier.get()](sdb::ProcessTerminated) {
        notifier->notify(&CommState::terminated, true);
    });
    messenger->listen([notifier = notifier.get()](sdb::ProcessKilled) {
        notifier->notify(&CommState::killed, true);
    });
    messenger->listen([notifier = notifier.get()](sdb::BreakEvent event) {
        notifier->notify(&CommState::breakEvent, event);
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

static sdb::SourceLocation getSourceLoc(sdb::Model const& model,
                                        scdis::InstructionPointerOffset ipo) {
    return model.sourceDebug().sourceMap().toSourceLoc(ipo).value();
}

static uint32_t getLine(sdb::Model const& model,
                        scdis::InstructionPointerOffset ipo) {
    return getSourceLoc(model, ipo).line.line;
}

TEST_CASE("Print test", "[model]") {
    auto [notifier, messenger] = makeComm();
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
        auto state = notifier->wait(&CommState::finished);
        CHECK(state.terminated);
        CHECK(!state.killed);
        CHECK(model.standardout().str().starts_with("Hello World\n"));
    }
    SECTION("Breakpoints") {
        model.toggleSourceBreakpoint({ .file = 0, .line = 4 });
        model.startExecution();
        auto state = notifier->wait(&CommState::haveBreakEvent);
        CHECK(state.breakEvent->state == sdb::BreakState::Paused);
        CHECK(!state.breakEvent->exception.hasValue());
        CHECK(model.standardout().str() == "Hello");
        model.stepSourceLine();
        state = notifier->wait(&CommState::haveBreakEvent);
        CHECK(model.standardout().str() == "Hello World");
        model.toggleExecution();
        state = notifier->wait(&CommState::finished);
        CHECK(state.terminated);
        CHECK(!state.killed);
        CHECK(model.standardout().str().starts_with("Hello World\n"));
    }
}

TEST_CASE("Memory error test", "[model]") {
    auto [notifier, messenger] = makeComm();
    auto source = R"(
/* 2 */ fn main() -> int {
/* 3 */     let p: *int = null;
/* 4 */     return *p;
/* 5 */ }
)";
    auto model = makeModel(messenger, { { "test-file.sc", source } });
    model.startExecution();
    auto state = notifier->wait(&CommState::haveBreakEvent);
    auto* memError = state.breakEvent->exception.as<svm::MemoryAccessError>();
    REQUIRE(memError);
    CHECK(memError->reason() == svm::MemoryAccessError::MemoryNotAllocated);
    model.stopExecution();
    state = notifier->wait(&CommState::finished);
    CHECK(state.killed);
}

TEST_CASE("Multifile breakpoints", "[model]") {
    auto [notifier, messenger] = makeComm();
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
    model.toggleSourceBreakpoint({ .file = 0, .line = 4 });
    model.toggleSourceBreakpoint({ .file = 1, .line = 3 });

    model.startExecution();
    auto state = notifier->wait(&CommState::haveBreakEvent);
    CHECK(state.breakEvent->state == sdb::BreakState::Paused);
    auto sourceLoc = getSourceLoc(model, state.breakEvent->ipo);
    CHECK(sourceLoc.line == sdb::SourceLine{ .file = 1, .line = 3 });
    CHECK(model.standardout().str().empty());

    model.toggleExecution();
    state = notifier->wait(&CommState::haveBreakEvent);
    CHECK(state.breakEvent->state == sdb::BreakState::Paused);
    sourceLoc = getSourceLoc(model, state.breakEvent->ipo);
    CHECK(sourceLoc.line == sdb::SourceLine{ .file = 0, .line = 4 });
    CHECK(model.standardout().str() == "Start\n");

    model.toggleExecution();
    state = notifier->wait(&CommState::haveBreakEvent);
    CHECK(state.breakEvent->state == sdb::BreakState::Paused);
    sourceLoc = getSourceLoc(model, state.breakEvent->ipo);
    CHECK(sourceLoc.line == sdb::SourceLine{ .file = 1, .line = 3 });
    CHECK(model.standardout().str() == "Start\n");

    model.toggleExecution();
    state = notifier->wait(&CommState::haveBreakEvent);
    CHECK(state.breakEvent->state == sdb::BreakState::Paused);
    sourceLoc = getSourceLoc(model, state.breakEvent->ipo);
    CHECK(sourceLoc.line == sdb::SourceLine{ .file = 1, .line = 3 });
    CHECK(model.standardout().str() == "Start\nContinue\n");

    model.toggleExecution();
    state = notifier->wait(&CommState::finished);
    CHECK(state.terminated);
    CHECK(model.standardout().str().starts_with("Start\nContinue\nDone\n"));
}

static std::atomic_bool* g_host_callback_called = nullptr;

extern "C" void live_patching_host_callback() {
    g_host_callback_called->store(true);
}

TEST_CASE("Live patching breakpoints", "[model]") {
    auto [notifier, messenger] = makeComm();
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
    std::atomic_bool host_callback_called = false;
    g_host_callback_called = &host_callback_called;
    utl::scope_guard reset = [] { g_host_callback_called = nullptr; };

    model.startExecution();
    waitWithTimeout([&] { return host_callback_called.load(); });
    sdb::SourceLine callLine = { .file = 0, .line = 5 };
    model.toggleSourceBreakpoint(callLine);
    auto state = notifier->wait(&CommState::haveBreakEvent);
    CHECK(state.breakEvent->state == sdb::BreakState::Paused);
    CHECK(getLine(model, state.breakEvent->ipo) == 5);

    model.toggleSourceBreakpoint(callLine);
    model.toggleExecution();
    waitWithTimeout([&] { return host_callback_called.load(); });
    model.stopExecution();
    state = notifier->wait(&CommState::finished);
    CHECK(state.killed);
}

TEST_CASE("Stepping out of root function", "[model]") {
    auto [notifier, messenger] = makeComm();
    auto source = R"(
/* 2 */ extern "C" fn live_patching_host_callback() -> void;
/* 3 */ fn main() {
/* 4 */     while true { live_patching_host_callback(); }
/* 5 */ }
)";
    auto model = makeModel(messenger, { { "test-file.sc", source } },
                           scatha::Asm::LinkerOptions{ .searchHost = true });
    std::atomic_bool host_callback_called = false;
    g_host_callback_called = &host_callback_called;
    model.startExecution();
    waitWithTimeout([&] { return host_callback_called.load(); });
    model.toggleExecution();
    (void)notifier->wait(&CommState::haveBreakEvent);
    host_callback_called.store(false);
    model.stepOut();
    waitWithTimeout([&] { return host_callback_called.load(); });
    model.toggleExecution();
    (void)notifier->wait(&CommState::haveBreakEvent);
    model.stopExecution();
    auto state = notifier->wait(&CommState::finished);
    CHECK(state.killed);
}

TEST_CASE("Recursive line stepping test", "[model]") {
    auto [notifier, messenger] = makeComm();
    auto source = R"(
/*  2 */ fn rec(recurse: bool) -> void {
/*  3 */     if (!recurse) { return; }
/*  4 */     rec(false);
/*  5 */     rec(false);
/*  6 */     rec(false);
/*  7 */ }
/*  8 */ fn main() {
/*  9 */     rec(true);
/* 10 */ }
)";
    auto model = makeModel(messenger, { { "test-file.sc", source } },
                           scatha::Asm::LinkerOptions{ .searchHost = true });
    model.toggleSourceBreakpoint({ .file = 0, .line = 4 });

    model.startExecution();
    auto state = notifier->wait(&CommState::haveBreakEvent);
    REQUIRE(getLine(model, state.breakEvent->ipo) == 4);
    model.toggleSourceBreakpoint({ .file = 0, .line = 4 });

    model.stepSourceLine();
    state = notifier->wait(&CommState::haveBreakEvent);
    REQUIRE(getLine(model, state.breakEvent->ipo) == 5);

    model.toggleSourceBreakpoint({ .file = 0, .line = 3 });
    model.stepSourceLine();
    state = notifier->wait(&CommState::haveBreakEvent);
    REQUIRE(getLine(model, state.breakEvent->ipo) == 3);
    model.toggleSourceBreakpoint({ .file = 0, .line = 3 });

    model.stepSourceLine();
    state = notifier->wait(&CommState::haveBreakEvent);
    REQUIRE(getLine(model, state.breakEvent->ipo) == 6);

    model.toggleExecution();
    state = notifier->wait(&CommState::finished);
    CHECK(state.terminated);
}

TEST_CASE("Recursive stepping out test", "[model]") {
    auto [notifier, messenger] = makeComm();
    auto source = R"(
/*  2 */ 
/*  3 */ fn rec(n: int) -> void {
/*  4 */     if n > 0 { 
/*  5 */         rec(n - 1); 
/*  6 */         __builtin_puti64(n);    
/*  7 */     }
/*  8 */ }
/*  9 */ fn main() {
/* 10 */     rec(3);
/* 11 */     return;
/* 12 */ }
)";
    auto model = makeModel(messenger, { { "test-file.sc", source } },
                           scatha::Asm::LinkerOptions{ .searchHost = true });

    SECTION("Step out of one layer of recursion") {
        model.toggleSourceBreakpoint({ .file = 0, .line = 5 });
        model.startExecution();
        auto state = notifier->wait(&CommState::haveBreakEvent);
        REQUIRE(getLine(model, state.breakEvent->ipo) == 5);
        CHECK(model.standardout().str() == "");
        model.clearBreakpoints();

        model.stepOut();
        state = notifier->wait(&CommState::haveBreakEvent);
        REQUIRE(getLine(model, state.breakEvent->ipo) == 11);
        CHECK(model.standardout().str() == "123");

        model.stepOut();
        state = notifier->wait(&CommState::finished);
        CHECK(state.terminated);
    }

    SECTION("Step out of three layers of recursion") {
        model.toggleSourceBreakpoint({ .file = 0, .line = 5 });
        for (int i = 0; i < 3; ++i) {
            i == 0 ? model.startExecution() : model.toggleExecution();
            auto state = notifier->wait(&CommState::haveBreakEvent);
            REQUIRE(getLine(model, state.breakEvent->ipo) == 5);
        }
        model.clearBreakpoints();

        model.stepOut();
        auto state = notifier->wait(&CommState::haveBreakEvent);
        REQUIRE(getLine(model, state.breakEvent->ipo) == 6);
        CHECK(model.standardout().str() == "1");

        model.stepOut();
        state = notifier->wait(&CommState::haveBreakEvent);
        REQUIRE(getLine(model, state.breakEvent->ipo) == 6);
        CHECK(model.standardout().str() == "12");

        model.stepOut();
        state = notifier->wait(&CommState::haveBreakEvent);
        REQUIRE(getLine(model, state.breakEvent->ipo) == 11);
        CHECK(model.standardout().str() == "123");
    }
}
