#include <scatha/Invocation/CompilerInvocation.h>
#include <svm/VirtualMachine.h>
#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace scatha;

static svm::VirtualMachine makeLoadedVM(std::string source) {
    CompilerInvocation inv(TargetType::Executable, "bench");
    inv.setInputs({ SourceFile::make(std::move(source)) });
    inv.setOptLevel(1);
    auto target = inv.run();
    if (!target) {
        throw std::runtime_error("Compilation failed");
    }
    svm::VirtualMachine vm;
    vm.loadBinary(target->binary().data());
    return vm;
}

#define RUN(VM)                                                                \
    BENCHMARK("Jump threaded") {                                               \
        VM.execute({});                                                        \
    };                                                                         \
    BENCHMARK("No jump threading") {                                           \
        VM.executeNoJumpThread({});                                            \
    }

TEST_CASE("Count") {
    auto VM = makeLoadedVM(R"(
fn count(n: int) -> int {
    var i = 0;
    while i < n { ++i; }
    return i;
}
fn main() -> int {
    return count(1000000);
})");
    RUN(VM);
}

TEST_CASE("Sort tree") {
    auto VM = makeLoadedVM(R"(
struct Node {
    fn new(&mut this, n: int) {
        this.value = n;
    }
    
    var value: int;
    var left: *unique mut Node;
    var right: *unique mut Node;
}

fn insert(root: mut *unique mut Node,
          newNode: mut *unique mut Node) {
    var y: *mut Node = null;
    var x: *mut Node = root as *mut;
    while x != null {
        y = x;
        if newNode.value < x.value {
            x = x.left as *mut;
        }
        else {
            x = x.right as *mut;
        }
     }
     if y == null {
        return move newNode;
     }
     else if newNode.value < y.value {
        y.left = move newNode;
        return move root;
     }
     else {
        y.right = move newNode;
        return move root;
     }
}

fn extractResult(root: *Node, data: &mut [int]) {
    var index = 0;
    extractResultImpl(root, data, index);
}

fn extractResultImpl(node: *Node, data: &mut [int], index: &mut int) -> void {
    if node == null {
        return;
    }
    extractResultImpl(node.left as *, data, index);
    data[index++] = node.value;
    extractResultImpl(node.right as *, data, index);
}

fn isSorted(data: &[int]) {
    for i = 0; i < data.count - 1; ++i {
        if data[i] > data[i + 1] {
            return false;
        }
    }
    return true;
}

fn main() {
    var root: *unique mut Node = null;
    let data = [
        23, 35, 73,  4, 92, 14, 27,  96, 22, 48,
        46, 25, 87, 76, 21, 77, 28,  37, 18, 82,
        36, 65, 39,  2, 69, 86, 59,  91, 95, 43,
        34,  9, 42, 58,  6, 74, 19,  93, 41, 26,
        12, 50, 53, 72, 75, 45, 54,   3,  5, 89,
         7, 79, 24, 97, 44, 66, 57,  64, 68, 83,
        47, 63, 61, 99, 38, 20, 52,  67, 85, 71,
        49, 70, 29, 84, 31,  8, 88,  94, 16, 78,
        60, 32, 62, 30, 17, 80, 90,  55, 13, 33,
        15, 51, 40, 81, 98,  1, 11, 100, 10, 56
    ];
    for i = 0; i < data.count; ++i {
        root = insert(move root, unique Node(data[i]));
    }
    var result: [int, data.count];
    extractResult(root as *, result);
    return isSorted(result);
}
)");
    RUN(VM);
}

TEST_CASE("Sort") {
    auto VM = makeLoadedVM(R"(
fn main() -> bool {
    var data = [
        15,   50,   82,   57,    7,   42,   86,   23,   60,   51,
        17,   19,   80,   33,   49,   35,   79,   98,   89,   27,
        92,   45,   43,    5,   87,    2,   58,   75,   22,   18,
        30,   41,   70,   40,    3,   84,   63,   39,   56,   97,
        81,    6,   64,   47,   90,   20,   77,   12,   74,   55,
        78,    4,   28,   52,   61,   85,   32,   37,   95,   83,
        87,   54,   77,   72,    9,   65,   11,   31,   10,    1,
        25,   73,   44,   71,   68,    8,   67,   13,   91,   24,
        62,   21,   66,   48,   99,   94,   69,   46,  100,   38,
        16,   53,   96,   34,   59,   14,   29,   93,   36,   26
    ];
    sort(data);
    return isSorted(data);
}

fn sort(data: &mut [int]) -> void {
    if data.empty {
        return;
    }
    let p = partition(data);
    sort(data[0 : p]);
    sort(data[p + 1 : data.count]);
}

fn partition(data: &mut [int]) -> int {
    var i = 0;
    var j = data.count - 2;
    let pivot = data.back;
    while i < j {
        while i < j && data[i] <= pivot {
            ++i;
        }
        while j > i && data[j] > pivot {
            --j;
        }
        if data[i] > data[j] {
            swap(data[i], data[j]);
        }
    }
    if data[i] <= pivot {
        return data.count - 1; // index of pivot
    }
    swap(data[i], data.back);
    return i;
}

fn swap(a: &mut int, b: &mut int) {
    let tmp = a;
    a = b;
    b = tmp;
}

fn isSorted(data: &[int]) -> bool {
    for i = 0; i < data.count - 1; ++i {
        if data[i] > data[i + 1] {
            return false;
        }
    }
    return true;
}
)");
    RUN(VM);
}
