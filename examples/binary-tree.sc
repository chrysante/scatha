
struct Node {
    fn new(&mut this, level: int) {
        this.level = level;
        __builtin_putstr("Construct level ");
        __builtin_puti64(level);
        __builtin_putstr("\n");
        if level > 0 {
            this.left = unique Node(level - 1); 
            this.right = unique Node(level - 1); 
        }
    }

    fn delete(&mut this) {
        __builtin_putstr("Delete level "); 
        __builtin_puti64(this.level);
        __builtin_putstr("\n"); 
    }
    
    var left: *unique mut Node;
    var right: *unique mut Node;
    var level: int;
}

fn main(args: &[*str]) {
    var level = 3;
    if args.count == 1 {
        if !__builtin_strtos64(level, args[0], 10) {
            __builtin_putstr("Failed to parse argument\n");
            return;
        }
    }
    else if args.count > 1 {
        __builtin_putstr("Too many arguments\n");
    }
    let root = unique Node(level);
}
