import std;

struct Node {
    fn new(&mut this, level: int) {
        this.level = level;
        std.print("Construct level \(level)");
        if level > 0 {
            this.left = unique Node(level - 1); 
            this.right = unique Node(level - 1); 
        }
    }

    fn delete(&mut this) {
        std.print("Delete level \(this.level)");
    }
    
    var left: *unique mut Node;
    var right: *unique mut Node;
    var level: int;
}

fn main(args: &[*str]) {
    var level = 3;
    if args.count == 1 {
        if !__builtin_strtos64(level, args[0], 10) {
            std.print("Failed to parse argument");
            return;
        }
    }
    else if args.count > 1 {
        std.print("Too many arguments");
    }
    let root = unique Node(level);
}
