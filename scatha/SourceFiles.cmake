
set(scatha_headers
  include/scatha/AST/Fwd.h
  include/scatha/AST/Lists.def.h
  include/scatha/AST/Print.h

  include/scatha/Assembly/Assembler.h
  include/scatha/Assembly/AssemblyStream.h
  include/scatha/Assembly/Fwd.h
  include/scatha/Assembly/Options.h

  include/scatha/CodeGen/CodeGen.h
  include/scatha/CodeGen/Logger.h
  include/scatha/CodeGen/Passes.h
  include/scatha/CodeGen/SelectionDAG.h
  include/scatha/CodeGen/SelectionNode.h

  include/scatha/Common/APMathFwd.h
  include/scatha/Common/APFloat.h
  include/scatha/Common/APInt.h
  include/scatha/Common/Allocator.h
  include/scatha/Common/Base.h
  include/scatha/Common/DebugInfo.h
  include/scatha/Common/Dyncast.h
  include/scatha/Common/Expected.h
  include/scatha/Common/FFI.h
  include/scatha/Common/Graph.h
  include/scatha/Common/List.h
  include/scatha/Common/Logging.h
  include/scatha/Common/Metadata.h
  include/scatha/Common/Ranges.h
  include/scatha/Common/SourceFile.h
  include/scatha/Common/SourceLocation.h
  include/scatha/Common/UniquePtr.h
  include/scatha/Common/Utility.h

  include/scatha/IR/CFG/BasicBlock.h
  include/scatha/IR/CFG/Constant.h
  include/scatha/IR/CFG/Constants.h
  include/scatha/IR/CFG/Function.h
  include/scatha/IR/CFG/Global.h
  include/scatha/IR/CFG/GlobalVariable.h
  include/scatha/IR/CFG/Instruction.h
  include/scatha/IR/CFG/Instructions.h
  include/scatha/IR/CFG/Iterator.h
  include/scatha/IR/CFG/User.h
  include/scatha/IR/CFG/Value.h
  include/scatha/IR/CFG.h
  include/scatha/IR/Context.h
  include/scatha/IR/Fwd.h
  include/scatha/IR/Graphviz.h
  include/scatha/IR/IRParser.h
  include/scatha/IR/Lists.def.h
  include/scatha/IR/Module.h
  include/scatha/IR/Parser/IRIssue.h
  include/scatha/IR/Parser/IRSourceLocation.h
  include/scatha/IR/Parser/IRToken.h
  include/scatha/IR/Pass.h
  include/scatha/IR/PassManager.h
  include/scatha/IR/Pipeline.h
  include/scatha/IR/PipelineError.h
  include/scatha/IR/Print.h
  include/scatha/IR/UniqueName.h
  include/scatha/IR/ValueRef.h

  include/scatha/IRGen/IRGen.h

  include/scatha/Invocation/CompilerInvocation.h
  include/scatha/Invocation/ExecutableWriter.h
  include/scatha/Invocation/Target.h

  include/scatha/Issue/Issue.h
  include/scatha/Issue/IssueHandler.h
  include/scatha/Issue/IssueSeverity.h
  include/scatha/Issue/Message.h
  include/scatha/Issue/SourceStructure.h

  include/scatha/MIR/Context.h
  include/scatha/MIR/Fwd.h
  include/scatha/MIR/Lists.def.h
  include/scatha/MIR/Module.h
  include/scatha/MIR/Print.h

  include/scatha/Opt/SCCCallGraph.h
  include/scatha/Opt/Passes.h

  include/scatha/Parser/Lexer.h
  include/scatha/Parser/LexicalIssue.h
  include/scatha/Parser/Parser.h
  include/scatha/Parser/SyntaxIssue.h
  include/scatha/Parser/Token.def.h
  include/scatha/Parser/Token.h

  include/scatha/Sema/AnalysisResult.h
  include/scatha/Sema/Analyze.h
  include/scatha/Sema/Conversion.def.h
  include/scatha/Sema/Entity.h
  include/scatha/Sema/Fwd.h
  include/scatha/Sema/LifetimeMetadata.h
  include/scatha/Sema/LifetimeOperation.h
  include/scatha/Sema/Lists.def.h
  include/scatha/Sema/NameMangling.h
  include/scatha/Sema/Print.h
  include/scatha/Sema/QualType.h
  include/scatha/Sema/SemaIssues.def.h
  include/scatha/Sema/SemaIssues.h
  include/scatha/Sema/Serialize.h
  include/scatha/Sema/SymbolTable.h
  include/scatha/Sema/ThinExpr.h
  include/scatha/Sema/VTable.h
)

set(scatha_sources
    lib/AST/AST.cc
    lib/AST/AST.h
    lib/AST/Fwd.cc
    lib/AST/Print.cc
    lib/AST/SpecifierList.h

    lib/Assembly/AsmWriter.cc
    lib/Assembly/AsmWriter.h
    lib/Assembly/Assembler.cc
    lib/Assembly/AssemblyStream.cc
    lib/Assembly/Block.cc
    lib/Assembly/Block.h
    lib/Assembly/Common.cc
    lib/Assembly/Common.h
    lib/Assembly/Instruction.cc
    lib/Assembly/Instruction.h
    lib/Assembly/Linker.cc
    lib/Assembly/Lists.def.h
    lib/Assembly/Map.cc
    lib/Assembly/Map.h
    lib/Assembly/Print.cc
    lib/Assembly/Print.h
    lib/Assembly/Value.cc
    lib/Assembly/Value.h

    lib/CodeGen/CodeGen.cc
    lib/CodeGen/CommonSubexpressionElimination.cc
    lib/CodeGen/CopyCoalescing.cc
    lib/CodeGen/DataFlow.cc
    lib/CodeGen/DeadCodeElim.cc
    lib/CodeGen/DestroySSA.cc
    lib/CodeGen/ISel.cc
    lib/CodeGen/ISel.h
    lib/CodeGen/ISelCommon.cc
    lib/CodeGen/ISelCommon.h
    lib/CodeGen/InstSimplify.cc
    lib/CodeGen/InterferenceGraph.cc
    lib/CodeGen/InterferenceGraph.h
    lib/CodeGen/JumpElision.cc
    lib/CodeGen/Logger.cc
    lib/CodeGen/LowerToASM.cc
    lib/CodeGen/LowerToMIR.cc
    lib/CodeGen/RegisterAllocator.cc
    lib/CodeGen/Resolver.cc
    lib/CodeGen/Resolver.h
    lib/CodeGen/SDMatch.cc
    lib/CodeGen/SDMatch.h
    lib/CodeGen/SelectionDAG.cc
    lib/CodeGen/SelectionNode.cc
    lib/CodeGen/TargetInfo.cc
    lib/CodeGen/TargetInfo.h
    lib/CodeGen/Utility.cc
    lib/CodeGen/Utility.h
    lib/CodeGen/ValueMap.cc
    lib/CodeGen/ValueMap.h

    lib/Common/Allocator.cc
    lib/Common/Base.cc
    lib/Common/Builtin.cc
    lib/Common/Builtin.h
    lib/Common/DebugInfo.cc
    lib/Common/EscapeSequence.cc
    lib/Common/EscapeSequence.h
    lib/Common/FFI.cc
    lib/Common/FileHandling.cc
    lib/Common/FileHandling.h
    lib/Common/Logging.cc
    lib/Common/Metadata.cc
    lib/Common/PrintUtil.cc
    lib/Common/PrintUtil.h
    lib/Common/SourceFile.cc
    lib/Common/SourceLocation.cc
    lib/Common/TreeFormatter.cc
    lib/Common/TreeFormatter.h

    lib/Debug/DebugGraphviz.cc
    lib/Debug/DebugGraphviz.h

    lib/IR/Attributes.h
    lib/IR/Attributes.cc
    lib/IR/BinSerialize.cc
    lib/IR/BinSerialize.h
    lib/IR/Builder.cc
    lib/IR/Builder.h
    lib/IR/CFG/BasicBlock.cc
    lib/IR/CFG/Constant.cc
    lib/IR/CFG/Constants.cc
    lib/IR/CFG/Function.cc
    lib/IR/CFG/Global.cc
    lib/IR/CFG/GlobalVariable.cc
    lib/IR/CFG/Instruction.cc
    lib/IR/CFG/Instructions.cc
    lib/IR/CFG/Iterator.cc
    lib/IR/CFG/User.cc
    lib/IR/CFG/Value.cc
    lib/IR/Clone.cc
    lib/IR/Clone.h
    lib/IR/Context.cc
    lib/IR/DataFlow.cc
    lib/IR/DataFlow.h
    lib/IR/Dominance.cc
    lib/IR/Dominance.h
    lib/IR/ForEach.cc
    lib/IR/ForEach.h
    lib/IR/Fwd.cc
    lib/IR/Graphviz.cc
    lib/IR/InvariantSetup.cc
    lib/IR/InvariantSetup.h
    lib/IR/Loop.cc
    lib/IR/Loop.h
    lib/IR/Module.cc
    lib/IR/Parser/IRIssue.cc
    lib/IR/Parser/IRLexer.cc
    lib/IR/Parser/IRLexer.h
    lib/IR/Parser/IRParser.cc
    lib/IR/Pass.cc
    lib/IR/PassManager.cc
    lib/IR/PassRegistry.h
    lib/IR/Pipeline.cc
    lib/IR/PipelineError.cc
    lib/IR/PipelineNodes.cc
    lib/IR/PipelineNodes.h
    lib/IR/PipelineParser.cc
    lib/IR/PipelineParser.h
    lib/IR/PointerInfo.cc
    lib/IR/PointerInfo.h
    lib/IR/Print.cc
    lib/IR/Type.cc
    lib/IR/Type.h
    lib/IR/UniqueName.cc
    lib/IR/Validate.cc
    lib/IR/Validate.h
    lib/IR/ValueRef.cc
    lib/IR/VectorHash.h

    lib/IRGen/CallingConvention.cc
    lib/IRGen/CallingConvention.h
    lib/IRGen/FuncGenContextBase.cc
    lib/IRGen/FuncGenContextBase.h
    lib/IRGen/FunctionGeneration.cc
    lib/IRGen/FunctionGeneration.h
    lib/IRGen/GlobalDecls.cc
    lib/IRGen/GlobalDecls.h
    lib/IRGen/IRGen.cc
    lib/IRGen/LibImport.cc
    lib/IRGen/LibImport.h
    lib/IRGen/LoweringContext.h
    lib/IRGen/Maps.cc
    lib/IRGen/Maps.h
    lib/IRGen/Metadata.cc
    lib/IRGen/Metadata.h
    lib/IRGen/Utility.cc
    lib/IRGen/Utility.h
    lib/IRGen/Value.cc
    lib/IRGen/Value.h

    lib/Invocation/CompilerInvocation.cc
    lib/Invocation/ExecutableWriter.cc
    lib/Invocation/Target.cc
    lib/Invocation/TargetNames.h

    lib/Issue/Format.cc
    lib/Issue/Format.h
    lib/Issue/Issue.cc
    lib/Issue/IssueHandler.cc

    lib/MIR/CFG.cc
    lib/MIR/CFG.h
    lib/MIR/Clone.cc
    lib/MIR/Clone.h
    lib/MIR/Context.cc
    lib/MIR/Fwd.cc
    lib/MIR/Hash.h
    lib/MIR/Instruction.cc
    lib/MIR/Instruction.h
    lib/MIR/Instructions.cc
    lib/MIR/Instructions.h
    lib/MIR/LiveInterval.cc
    lib/MIR/LiveInterval.h
    lib/MIR/Module.cc
    lib/MIR/Print.cc
    lib/MIR/Register.cc
    lib/MIR/Register.h
    lib/MIR/RegisterSet.cc
    lib/MIR/RegisterSet.h
    lib/MIR/Value.cc
    lib/MIR/Value.h

    lib/Opt/AccessTree.cc
    lib/Opt/AccessTree.h
    lib/Opt/AllocaPromotion.cc
    lib/Opt/AllocaPromotion.h
    lib/Opt/Common.cc
    lib/Opt/Common.h
    lib/Opt/ConstantPropagation.cc
    lib/Opt/DCE.cc
    lib/Opt/DefaultPass.cc
    lib/Opt/GlobalDCE.cc
    lib/Opt/GlobalValueNumbering.cc
    lib/Opt/HelloWorld.cc
    lib/Opt/InlineCallsite.cc
    lib/Opt/InlineCallsite.h
    lib/Opt/InlineCost.cc
    lib/Opt/InlineCost.h
    lib/Opt/Inliner.cc
    lib/Opt/InstCombine.cc
    lib/Opt/InvariantPropagation.cc
    lib/Opt/LoopPass.cc
    lib/Opt/LoopRankView.cc
    lib/Opt/LoopRankView.h
    lib/Opt/LoopRotate.cc
    lib/Opt/LoopUnroll.cc
    lib/Opt/MemToReg.cc
    lib/Opt/MemberTree.cc
    lib/Opt/MemberTree.h
    lib/Opt/Optimizer.cc
    lib/Opt/PointerAnalysis.cc
    lib/Opt/Rematerialize.cc
    lib/Opt/ScalarEvolution.cc
    lib/Opt/ScalarEvolution.h
    lib/Opt/SCEV.def.h
    lib/Opt/SCCCallGraph.cc
    lib/Opt/SROA.cc
    lib/Opt/SimplifyCFG.cc
    lib/Opt/TailRecElim.cc
    lib/Opt/UnifyReturns.cc

    lib/Parser/Bracket.cc
    lib/Parser/Bracket.h
    lib/Parser/BracketCorrection.cc
    lib/Parser/BracketCorrection.h
    lib/Parser/Lexer.cc
    lib/Parser/LexerUtil.cc
    lib/Parser/LexerUtil.h
    lib/Parser/LexicalIssue.cc
    lib/Parser/Panic.cc
    lib/Parser/Panic.h
    lib/Parser/Parser.cc
    lib/Parser/SyntaxIssue.cc
    lib/Parser/Token.cc
    lib/Parser/TokenStream.cc
    lib/Parser/TokenStream.h

    lib/Sema/Analysis/AnalysisContext.cc
    lib/Sema/Analysis/AnalysisContext.h
    lib/Sema/Analysis/Analyze.cc
    lib/Sema/Analysis/ConstantExpressions.cc
    lib/Sema/Analysis/ConstantExpressions.h
    lib/Sema/Analysis/Conversion.cc
    lib/Sema/Analysis/Conversion.h
    lib/Sema/Analysis/ExpressionAnalysis.cc
    lib/Sema/Analysis/ExpressionAnalysis.h
    lib/Sema/Analysis/GatherNames.cc
    lib/Sema/Analysis/GatherNames.h
    lib/Sema/Analysis/Instantiation.cc
    lib/Sema/Analysis/Instantiation.h
    lib/Sema/Analysis/ProtocolConformance.cc
    lib/Sema/Analysis/ProtocolConformance.h
    lib/Sema/Analysis/OverloadResolution.cc
    lib/Sema/Analysis/OverloadResolution.h
    lib/Sema/Analysis/StatementAnalysis.cc
    lib/Sema/Analysis/StatementAnalysis.h
    lib/Sema/Analysis/StructDependencyGraph.cc
    lib/Sema/Analysis/StructDependencyGraph.h
    lib/Sema/Analysis/Utility.cc
    lib/Sema/Analysis/Utility.h
    lib/Sema/CleanupStack.h
    lib/Sema/Entity.cc
    lib/Sema/Format.cc
    lib/Sema/Format.h
    lib/Sema/Fwd.cc
    lib/Sema/LifetimeFunctionAnalysis.cc
    lib/Sema/LifetimeFunctionAnalysis.h
    lib/Sema/LifetimeMetadata.cc
    lib/Sema/NameMangling.cc
    lib/Sema/Print.cc
    lib/Sema/QualType.cc
    lib/Sema/SemaIssues.cc
    lib/Sema/Serialize.cc
    lib/Sema/SerializeFields.def.h
    lib/Sema/SymbolTable.cc
    lib/Sema/ThinExpr.cc
    lib/Sema/VTable.cc
)

set(scatha_test_sources
    test/Assembly/Assembler.t.cc

    test/CodeGen/DataFlow.t.cc

    test/Common/Allocator.t.cc
    test/Common/Expected.t.cc

    test/EndToEndTests/BitwiseOperations.t.cc
    test/EndToEndTests/BooleanOperations.t.cc
    test/EndToEndTests/Builtins.t.cc
    test/EndToEndTests/CodeGenerator.t.cc
    test/EndToEndTests/Conditions.t.cc
    test/EndToEndTests/Constructors.t.cc
    test/EndToEndTests/Conversion.t.cc
    test/EndToEndTests/DynamicCalls.t.cc
    test/EndToEndTests/GlobalVariables.t.cc
    test/EndToEndTests/Inlining.t.cc
    test/EndToEndTests/Library.t.cc
    test/EndToEndTests/LocalScopes.t.cc
    test/EndToEndTests/Loops.t.cc
    test/EndToEndTests/Math.t.cc
    test/EndToEndTests/MemberFunctions.t.cc
    test/EndToEndTests/Modules.t.cc
    test/EndToEndTests/Overloading.t.cc
    test/EndToEndTests/PassTesting.cc
    test/EndToEndTests/PassTesting.h
    test/EndToEndTests/References.t.cc
    test/EndToEndTests/Regression.t.cc
    test/EndToEndTests/SimpleRecursion.t.cc
    test/EndToEndTests/StaticData.t.cc
    test/EndToEndTests/Strings.t.cc
    test/EndToEndTests/Structures.t.cc

    test/Invocation/CompilerInvocation.t.cc

    test/IR/Clone.t.cc
    test/IR/DataFlow.t.cc
    test/IR/Dominance.t.cc
    test/IR/Equal.cc
    test/IR/Equal.h
    test/IR/EqualityTestHelper.cc
    test/IR/EqualityTestHelper.h
    test/IR/IR.t.cc
    test/IR/IRParser.t.cc
    test/IR/Iterator.t.cc
    test/IR/LoopNestingForest.t.cc
    test/IR/Types.t.cc
    test/IR/UniqueName.t.cc

    test/IRGen/Arrays.t.cc
    test/IRGen/ConditionalExpr.t.cc
    test/IRGen/ConversionExpr.t.cc
    test/IRGen/FunctionCalls.t.cc
    test/IRGen/MemberAccess.t.cc
    test/IRGen/ObjectConstruction.t.cc
    test/IRGen/ParameterGeneration.t.cc
    test/IRGen/Return.t.cc
    test/IRGen/Subscript.t.cc
    test/IRGen/UniquePtr.t.cc
    test/IRGen/Variables.t.cc

    test/Issue/Format.t.cc

    test/Lexer/Lexer.t.cc
    test/Lexer/LexerUtil.t.cc

    test/Main/Options.h
    test/Main/Main.cc

    test/Opt/Common.t.cc
    test/Opt/ConstantPropagation.t.cc
    test/Opt/GlobalValueNumbering.t.cc
    test/Opt/Inliner.t.cc
    test/Opt/InstCombine.t.cc
    test/Opt/LoopRotate.t.cc
    test/Opt/MemToReg.t.cc
    test/Opt/PassTest.cc
    test/Opt/PassTest.h
    test/Opt/PointerAnalysis.t.cc
    test/Opt/SROA.t.cc

    test/Parser/BracketCorrection.t.cc
    test/Parser/Parser.t.cc
    test/Parser/SimpleParser.cc
    test/Parser/SimpleParser.h
    test/Parser/SyntaxIssues.t.cc

    test/Sema/Conversion.t.cc
    test/Sema/FunctionNameCollision.t.cc
    test/Sema/FunctionSignature.t.cc
    test/Sema/Lifetime.t.cc
    test/Sema/NameMangling.t.cc
    test/Sema/OverloadResolution.t.cc
    test/Sema/OverloadSet.t.cc
    test/Sema/SemaUtil.cc
    test/Sema/SemaUtil.h
    test/Sema/SemanticAnalysis.t.cc
    test/Sema/SemanticErrors.t.cc
    test/Sema/SimpleAnalzyer.cc
    test/Sema/SimpleAnalzyer.h
    test/Sema/SymbolTable.t.cc
    test/Sema/SymbolTableSerialize.t.cc

    test/Util/CoutRerouter.cc
    test/Util/CoutRerouter.h
    test/Util/FrontendWrapper.cc
    test/Util/FrontendWrapper.h
    test/Util/IRTestUtils.cc
    test/Util/IRTestUtils.h
    test/Util/IssueHelper.cc
    test/Util/IssueHelper.h
    test/Util/LibUtil.cc
    test/Util/LibUtil.h
    test/Util/Set.h
)

set(scatha_benchmark_sources
  benchmark/Benchmark.cc
)
