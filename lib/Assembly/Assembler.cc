#include "Assembly/Assembler.h"

#include <array>
#include <iostream>
#include <ostream>
#include <span>

#include <utl/bit.hpp>
#include <utl/hashmap.hpp>
#include <utl/scope_guard.hpp>
#include <utl/utility.hpp>

#include "Assembly/AssemblerIssue.h"
#include "Assembly/AssemblyUtil.h"
#include "Basic/Memory.h"

#include "VM/OpCode.h"

namespace scatha::assembly {

using namespace vm;

Assembler::Assembler(AssemblyStream const& str): stream(str) {}

Program Assembler::assemble(AssemblerOptions opt) {
    program = nullptr;
    labels.clear();
    jumpsites.clear();

    Program result;
    program = &result;

    StreamIterator itr(stream);
    for (Element elem = itr.next(); elem.marker() != Marker::EndOfProgram; elem = itr.next()) {
        switch (elem.marker()) {
        case Marker::Instruction: {
            auto const instruction = elem.get<Instruction>();
            processInstruction(instruction, itr);
            break;
        }
        case Marker::Label: {
            auto const label = elem.get<Label>();
            registerLabel(label);
            if (label.functionID == opt.mainID && label.index == Label::functionBeginIndex) {
                result.start = currentPosition();
            }
            break;
        }
        default: throw UnexpectedElement(elem, itr.currentLine());
        }
    }

    postProcess();

    return result;
}

void Assembler::processInstruction(Instruction i, StreamIterator& itr) {
    switch (i) {
        using enum Instruction;
    case enterFn:
        put(OpCode::enterFn);
        put(itr.nextAs<Value8>());
        return;

    case setBrk:
        put(OpCode::setBrk);
        put(itr.nextAs<RegisterIndex>());
        return;

    case call:
        registerJumpsite(itr);
        put(OpCode::call);
        put(LabelPlaceholder{});
        put(itr.nextAs<Value8>());
        return;

    case ret: put(OpCode::ret); return;

    case terminate: put(OpCode::terminate); return;

    case callExt:
        put(OpCode::callExt);
        put(itr.nextAs<Value8>());
        put(itr.nextAs<Value8>());
        put(itr.nextAs<Value16>());
        return;

    case alloca_:
        put(OpCode::alloca_);
        put(itr.nextAs<RegisterIndex>());
        put(itr.nextAs<RegisterIndex>());
        return;
        
    case itest: [[fallthrough]];
    case utest: [[fallthrough]];
    case sete: [[fallthrough]];
    case setne: [[fallthrough]];
    case setl: [[fallthrough]];
    case setle: [[fallthrough]];
    case setg: [[fallthrough]];
    case setge: [[fallthrough]];
    case lnt: [[fallthrough]];
    case bnt: processUnaryInstruction(i, itr); return;

    case mov: [[fallthrough]];
    case ucmp: [[fallthrough]];
    case icmp: [[fallthrough]];
    case fcmp: [[fallthrough]];
    case add: [[fallthrough]];
    case sub: [[fallthrough]];
    case mul: [[fallthrough]];
    case div: [[fallthrough]];
    case idiv: [[fallthrough]];
    case rem: [[fallthrough]];
    case irem: [[fallthrough]];
    case fadd: [[fallthrough]];
    case fsub: [[fallthrough]];
    case fmul: [[fallthrough]];
    case fdiv:
    case sl:
    case sr:
    case And:
    case Or:
    case XOr: processBinaryInstruction(i, itr); return;

    case jmp: [[fallthrough]];
    case je: [[fallthrough]];
    case jne: [[fallthrough]];
    case jl: [[fallthrough]];
    case jle: [[fallthrough]];
    case jg: [[fallthrough]];
    case jge: processJump(i, itr); return;

    case _count: SC_DEBUGFAIL();
    }
}

void Assembler::processUnaryInstruction(Instruction i, StreamIterator& itr) {
    auto const arg1   = itr.next();
    auto const opcode = mapUnaryInstruction(i);
    if (opcode == OpCode::_count) {
        throw InvalidArguments(i, arg1, {}, itr.currentLine());
    }
    put(opcode);
    put(arg1);
}

void Assembler::processBinaryInstruction(Instruction i, StreamIterator& itr) {
    auto const arg1   = itr.next();
    auto const arg2   = itr.next();
    auto const opcode = mapBinaryInstruction(i, arg1, arg2);
    if (opcode == OpCode::_count) {
        throw InvalidArguments(i, arg1, arg2, itr.currentLine());
    }
    put(opcode);
    put(arg1);
    put(arg2);
}

void Assembler::processJump(Instruction i, StreamIterator& itr) {
    registerJumpsite(itr);
    put(mapUnaryInstruction(i));
    put(LabelPlaceholder{});
    return;
}

void Assembler::registerLabel(Label label) {
    labels.insert({ label, currentPosition() });
}

void Assembler::registerJumpsite(StreamIterator& itr) {
    jumpsites.push_back({ program->instructions.size(), itr.currentLine(), itr.nextAs<Label>() });
}

void Assembler::postProcess() {
    for (auto const& [position, line, label]: jumpsites) {
        auto const itr = labels.find(label);
        if (itr == labels.end()) {
            throw UseOfUndeclaredLabel(label, line);
        }
        size_t const target = itr->second;
        SC_ASSERT(position >= 1, "Position must be positive");
        auto const instruction = read<OpCode>(&program->instructions[position]);
        SC_ASSERT(isJump(instruction), "Before a label should be a jump or call statement at this stage.");
        store(&program->instructions[position + 1], i32(i64(target) - i64(position)));
    }
}

/// MARK: put()
void Assembler::put(vm::OpCode o) {
    program->instructions.push_back(utl::to_underlying(o));
}

void Assembler::put(LabelPlaceholder) {
    for (int i = 0; i < 4; ++i) {
        program->instructions.push_back(0);
    }
}

void Assembler::put(Element const& elem) {
    // clang-format off
    std::visit(utl::visitor{
                   [this](auto const& x) { put(x); },
                   [](Instruction) {
                       SC_DEBUGFAIL(); // Probably a bug because we should not
                                       // put an assembly::Instruction into the
                                       // program, only vm::OpCode.
                   },
                   [](Label) {
                       SC_DEBUGFAIL(); // Same here, labels do not exist in an
                                       // assembled program.
                   },
               }, elem);
    // clang-format on
}

void Assembler::put(RegisterIndex r) {
    program->instructions.push_back(r.index);
}

void Assembler::put(MemoryAddress m) {
    program->instructions.push_back(m.ptrRegIdx);
    program->instructions.push_back(m.offset);
    program->instructions.push_back(m.offsetShift);
}

void Assembler::put(Value8 v) {
    for (auto byte: decompose(v.value)) {
        program->instructions.push_back(byte);
    }
}

void Assembler::put(Value16 v) {
    for (auto byte: decompose(v.value)) {
        program->instructions.push_back(byte);
    }
}

void Assembler::put(Value32 v) {
    for (auto byte: decompose(v.value)) {
        program->instructions.push_back(byte);
    }
}

void Assembler::put(Value64 v) {
    for (auto byte: decompose(v.value)) {
        program->instructions.push_back(byte);
    }
}

} // namespace scatha::assembly
