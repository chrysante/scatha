#ifndef SCATHA_ASSEMBLY_OPTIONS_H_
#define SCATHA_ASSEMBLY_OPTIONS_H_

namespace scatha::Asm {

/// Options for `Asm::link()`
struct LinkerOptions {
    /// Instructs the linker to search the host executable for missing foreign
    /// functions
    bool searchHost = false;
};

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_OPTIONS_H_
