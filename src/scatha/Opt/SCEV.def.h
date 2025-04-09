// ===--------------------------------------------------------------------=== //
// === List of all scalar evolution expression nodes ---------------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SC_OPT_SCEV_DEF
#define SC_OPT_SCEV_DEF(...)
#endif

SC_OPT_SCEV_DEF(ScevExpr, NoParent, Abstract)
SC_OPT_SCEV_DEF(ScevNullaryExpr, ScevExpr, Abstract)
SC_OPT_SCEV_DEF(ScevConstExpr, ScevNullaryExpr, Concrete)
SC_OPT_SCEV_DEF(ScevUnknownExpr, ScevNullaryExpr, Concrete)
SC_OPT_SCEV_DEF(ScevArithmeticExpr, ScevExpr, Abstract)
SC_OPT_SCEV_DEF(ScevAddExpr, ScevArithmeticExpr, Concrete)
SC_OPT_SCEV_DEF(ScevMulExpr, ScevArithmeticExpr, Concrete)

#undef SC_OPT_SCEV_DEF
