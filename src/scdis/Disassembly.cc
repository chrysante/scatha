#include "scdis/Disassembly.h"

using namespace scdis;

Value scdis::makeRegisterIndex(size_t index) {
    return { ValueType::RegisterIndex, index };
}

Value scdis::makeAddress(uint8_t baseRegIdx, uint8_t offsetRegIdx,
                         uint8_t offsetFactor, uint8_t offsetTerm) {
    struct Addr {
        uint8_t baseRegIdx;
        uint8_t offsetRegIdx;
        uint8_t offsetFactor;
        uint8_t offsetTerm;
    };
    return { ValueType::Address, std::bit_cast<uint32_t>(Addr{
                                     baseRegIdx,
                                     offsetRegIdx,
                                     offsetFactor,
                                     offsetTerm,
                                 }) };
}

Value scdis::makeAddress(uint32_t value) {
    return { ValueType::Address, std::bit_cast<uint32_t>(value) };
}

Value scdis::makeValue8(uint64_t value) { return { ValueType::Value8, value }; }

Value scdis::makeValue16(uint64_t value) {
    return { ValueType::Value16, value };
}

Value scdis::makeValue32(uint64_t value) {
    return { ValueType::Value32, value };
}

Value scdis::makeValue64(uint64_t value) {
    return { ValueType::Value64, value };
}

std::optional<size_t> IpoIndexMap::ipoToIndex(
    InstructionPointerOffset ipo) const {
    auto itr = _ipoToIndex.find(ipo);
    if (itr != _ipoToIndex.end()) return itr->second;
    return std::nullopt;
}

/// \Returns the instruction pointer offset of the instruction at index \p index
InstructionPointerOffset IpoIndexMap::indexToIpo(size_t index) const {
    assert(index < _indexToIpo.size());
    return _indexToIpo[index];
}

void IpoIndexMap::insertAtBack(InstructionPointerOffset ipo) {
    _ipoToIndex.insert({ ipo, _indexToIpo.size() });
    _indexToIpo.push_back(ipo);
}
