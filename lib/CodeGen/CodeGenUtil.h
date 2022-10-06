#ifndef SCATHA_CODEGEN_CODEGENUTIL_H_
#define SCATHA_CODEGEN_CODEGENUTIL_H_

#include "Assembly/Assembly.h"
#include "IC/ThreeAddressStatement.h"

namespace scatha::codegen {

assembly::Instruction mapOperation(ic::Operation);

assembly::Instruction mapComparison(ic::Operation);

assembly::Instruction mapComparisonStore(ic::Operation);

assembly::Instruction mapConditionalJump(ic::Operation);

} // namespace scatha::codegen

#endif // SCATHA_CODEGEN_CODEGENUTIL_H_
