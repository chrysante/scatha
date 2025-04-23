import hashtable;
import std;

fn check(cond: bool, message: *str) {
    if (!cond) { std.print("Test failed: \(message)"); __builtin_trap(); }
}

fn main() -> int {
    var table = hashtable.Hashtable();

    check(table.empty(), "Hashtable should start empty");
    check(table.size() == 0, "Initial size should be 0");

    // Insert some values
    check(table.store(5), "Insert 5");
    check(table.store(15), "Insert 15");
    check(table.store(7), "Insert 7");
    check(table.store(0), "Insert 0");

    check(!table.store(5), "Duplicate insert 5 should fail");
    check(table.contains(5), "Table should contain 5");
    check(table.contains(15), "Table should contain 15");
    check(table.size() == 4, "Size should be 4");

    table.erase(15);
    check(!table.contains(15), "15 should be erased");
    check(table.size() == 3, "Size should be 3 after erase");

    check(table.store(15), "Re-inserting 15 after erase");
    check(table.contains(15), "Table should contain 15 again");
    check(table.size() == 4, "Size should be back to 4");

    // Add more to force rehash
    check(table.store(13), "Insert 13");
    check(table.store(31), "Insert 31");
    check(table.store(11), "Insert 11");

    // Capacity must have increased; check all values are still there
    let active_vals = [5, 7, 0, 15, 13, 31, 11];
    for i = 0; i < active_vals.count; ++i {
        let val = active_vals[i];
        check(table.contains(val), "Post-rehash contains \(val)" as *);
    }

    // Negative numbers are invalid input
    check(!table.store(-1), "Should not allow negative insert");
    check(!table.store(-100), "Should not allow negative insert");
    check(!table.contains(-1), "Should not contain -1");
    check(!table.erase(-1), "Should not erase -1");

    // Test removing something that doesn't exist
    check(!table.erase(999), "Erasing non-existent value should fail");

    // Final count check
    check(table.size() == 7, "Final size check");

    std.print("All tests passed");

    return 0;
}
