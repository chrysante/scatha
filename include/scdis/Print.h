#ifndef SCDIS_PRINT_H_
#define SCDIS_PRINT_H_

#include <iosfwd>
#include <string_view>

#include <scdis/Disassembly.h>

namespace scdis {

/// Delegate class to enable custom instruction printing, for example to HTML or
/// a UI framework
class PrintDelegate {
public:
    virtual ~PrintDelegate() = default;

    // Primitive rendering hooks
    virtual void instName(std::string_view name) = 0;
    virtual void registerName(size_t index) = 0;
    virtual void immediate(uint64_t value) = 0;
    virtual void label(std::string_view label) = 0;
    virtual void labelName(std::string_view label) = 0;
    virtual void plaintext(std::string_view str) = 0;

    // Optional structural helpers (spacing, punctuation)
    virtual void space() { plaintext(" "); }
    virtual void comma() { plaintext(", "); }
    virtual void plus() { plaintext(" + "); }
    virtual void star() { plaintext(" * "); }
    virtual void leftBracket() { plaintext("["); }
    virtual void rightBracket() { plaintext("]"); }

    // Optional hooks for composition
    virtual void beginInst(Instruction const&) {}
    virtual void endInst() {}
};

/// Generic print function that uses \p delegate to print \p disasm
void print(Disassembly const& disasm, PrintDelegate& delegate);

/// Prints \p disasm to \p ostream
void print(Disassembly const& disasm, std::ostream& ostream,
           bool useColor = false);

} // namespace scdis

#endif // SCDIS_PRINT_H_
