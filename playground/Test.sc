public fn printDot(x: float) {
         if x < 0.1 { __builtin_putchar(32); }
    else if x < 0.2 { __builtin_putchar(46); }
    else if x < 0.3 { __builtin_putchar(58); }
    else if x < 0.4 { __builtin_putchar(45); }
    else if x < 0.5 { __builtin_putchar(61); }
    else if x < 0.6 { __builtin_putchar(43); }
    else if x < 0.7 { __builtin_putchar(42); }
    else if x < 0.8 { __builtin_putchar(35); }
    else if x < 0.9 { __builtin_putchar(37); }
    else            { __builtin_putchar(64); }
}
