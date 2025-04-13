#ifndef SCDIS_DISASSEMBLER_H_
#define SCDIS_DISASSEMBLER_H_

#include <cstdint>
#include <span>

#include <scatha/DebugInfo/DebugInfo.h>

#include <scdis/Disassembly.h>

namespace scdis {

/// Disasembles the binary representation \p program into a structered list of
/// instructions
/// \p debugInfo may be empty
Disassembly disassemble(std::span<uint8_t const> program,
                        scatha::DebugInfoMap const& debugInfo);

} // namespace scdis

#endif // SCDIS_DISASSEMBLER_H_
