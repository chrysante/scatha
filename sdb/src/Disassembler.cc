#include "Disassembler.h"

using namespace sdb;
using namespace svm;

Disassembly sdb::disassemble(uint8_t const* program) {
    Disassembly result;
    ProgramView const p(program);
    auto text = p.text;
    for (size_t i = 0; i < text.size();) {
        size_t binOffset = i + p.header.textOffset - sizeof(ProgramHeader);
        OpCode const opcode = static_cast<OpCode>(text[i]);

        result.offsetIndexMap.insert({ binOffset, result.insts.size() });
        result.insts.push_back({ opcode });

        auto const opcodeClass = classify(opcode);
        switch (opcodeClass) {
            using enum OpCodeClass;
        case RR:
            break;
        case RV64:
            break;
        case RV32:
            break;
        case RV8:
            break;
        case RM:
            break;
        case MR:
            break;
        case R:
            break;
        case Jump:
            break;
        case Other:
            switch (opcode) {
            case OpCode::lincsp:
                break;
            case OpCode::call:
                break;
            case OpCode::icallr:
                break;
            case OpCode::icallm:
                break;
            case OpCode::ret:
                break;
            case OpCode::terminate:
                break;
            case OpCode::callExt:
                break;
            default:
                assert(false);
            }
            break;
        case _count:
            assert(false);
        }
        i += codeSize(opcode);
    }
    return result;
}
