//===--- InstructionVisitor.cpp - SIL to CAst Translator -----------------===//
//
// This source file is part of the SWAN open source project
//
// Copyright (c) 2019 Maple @ University of Alberta
// All rights reserved. This program and the accompanying materials (unless
// otherwise specified by a license inside of the accompanying material)
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
//
//===---------------------------------------------------------------------===//
///
/// This file implements the InstructionVisitor class, which inherits
/// the SILInstructionVisitor template class (part of the Swift compiler).
/// The SILInstructionVisitor gathers all necessary information for translation
/// into CAst (WALA AST IR), which is then translated by the Java side.
///
/// There are many nuances, so if anything looks confusing, please refer to
/// https://github.com/apple/swift/blob/master/docs/SIL.rst.
///
//===---------------------------------------------------------------------===//

// TODO: PRINT INFORMATION NEEDS TO BE COMPLETED. i.e. "[ ... ]"

#include "InstructionVisitor.h"
#include "swift/AST/Module.h"
#include "swift/AST/Types.h"
#include "swift/Demangling/Demangle.h"
#include "swift/SIL/SILModule.h"
#include <fstream>
#include <memory>

#define MAKE_NODE(x) Instance->CAst->makeNode(x)
#define MAKE_NODE2(x, y) Instance->CAst->makeNode(x, y)
#define MAKE_NODE3(x, y, z) Instance->CAst->makeNode(x, y, z)
#define MAKE_NODE4(x, y, z, z2) Instance->CAst->makeNode(x, y, z, z2)
#define MAKE_CONST(x) Instance->CAst->makeConstant(x)
#define MAKE_ARRAY(x) Instance->CAst->makeArray(x)
#define ADD_PROP(x) currentInstruction->addProperty(x)

using namespace swan;

//===------------------- MODULE/FUNCTION/BLOCK VISITORS ------------------===//

void InstructionVisitor::visitSILModule(SILModule *M) {

  for (auto it = M->getWitnessTables().begin(); it != M->getWitnessTables().end(); ++it) {
    std::string protocolName = it->getProtocol()->getName().str();
    if (witnessTable.find(protocolName) == witnessTable.end()) {
      witnessTable.insert({protocolName, std::vector<string>()});
    }
    for (auto it1 = it->getEntries().begin(); it1 != it->getEntries().end(); ++it1) {
      if (it1->getKind() == SILWitnessTable::WitnessKind::Method)
      witnessTable.find(protocolName)->second.push_back(
        Demangle::demangleSymbolAsString(it1->getMethodWitness().Witness->getName()));
    }
  }

  for (SILFunction &F: *M) {
    if (F.empty()) { // Most likely a builtin or library (external) function, so we ignore it.
      // Can't get information like arguments from functions without a body :(
      if (SWAN_PRINT) {
          llvm::outs() << "Skipping " << Demangle::demangleSymbolAsString(F.getName()) << "\n";
      }
      continue;
    }
    swift::DeclContext* declContext = F.getDeclContext();
    swift::SourceFile* sourceFile = (declContext != nullptr) ? declContext->getParentSourceFile() : nullptr;
    std::string file = (sourceFile != nullptr) ? sourceFile->getFilename() : "NO_SOURCE";
    Instance->setSource(file);
    visitSILFunction(&F);
    Instance->addSourceFunction(file, currentFunction.get()->make());
  }
}

void InstructionVisitor::visitSILFunction(SILFunction *F) {
  std::string const &demangledFunctionName = Demangle::demangleSymbolAsString(F->getName());
  functionInfo = std::make_unique<SILFunctionInfo>(F->getName(), demangledFunctionName);
  currentFunction = std::make_unique<RootFunctionInfo>(Instance->CAst);
  currentFunction->functionName = demangledFunctionName;

  // Set function source information.
  unsigned fl = 0, fc = 0, ll = 0, lc = 0;
  // Swift compiler doesn't seem to have a way of getting the specific location of a param.
  if (!F->getLocation().isNull()) {
    SourceManager const &srcMgr = F->getModule().getSourceManager();
    SourceRange const &srcRange = F->getLocation().getSourceRange();
    SourceLoc const &srcStart = srcRange.Start;
    SourceLoc const &srcEnd = srcRange.End;
    if (srcStart.isInvalid() || srcEnd.isInvalid()) {
      if (SWAN_PRINT) {
        llvm::outs() << "WARNING: Source information is invalid for function: " << demangledFunctionName;
      }
    } else {
      auto startLineCol = srcMgr.getLineAndColumn(srcStart);
      fl = startLineCol.first;
      fc = startLineCol.second;
      auto endLineCol = srcMgr.getLineAndColumn(srcEnd);
      ll = endLineCol.first;
      lc = endLineCol.second;
    }
  } else {
    // "main" does not have source information for obvious reasons.
    if (SWAN_PRINT) {
      llvm::outs() << "WARNING: Source information is null for function: " << demangledFunctionName << "\n";
    }
  }
  currentFunction->setFunctionSourceInfo(fl, fc, ll, lc);

  // Handle function arguments.
  for (SILArgument *arg: F->getArguments()) {
    // Currently the arguments do not have a specific position.
    currentFunction->addArgument(addressToString(static_cast<ValueBase*>(arg)), arg->getType().getAsString(),
      fl, fc, fl, fc);
  }

  // Set function result type.
  if (F->getLoweredFunctionType()->getNumResults() == 1) {
    currentFunction->returnType = F->getLoweredFunctionType()->getSingleResult().getSILStorageType().getAsString();
  } else if (F->getLoweredFunctionType()->getNumResults() == 0) {
    currentFunction->returnType = "void";
  } else {
    currentFunction->returnType = "MultiResultType"; // SHOULD NOT HAPPEN IN CURRENT SWIFT VERSION!
  }

  if (SWAN_PRINT) {
    llvm::outs() << "SILFunction: " << "ADDR: " << F << " , NAME: " << demangledFunctionName << "\n";
    for (auto arg : F->getArguments()) {
      llvm::outs() << "[ARG]: " << addressToString(static_cast<ValueBase*>(arg)) << "\n";
    }
  }
  // Note: Single file mode will print the SIL anyways.
  if (PRINT_SIL) {
    llvm::outs() << "<RAW SIL BEGIN> \n\n";
    F->print(llvm::outs(), true);
    llvm::outs() << "\n</RAW SIL END> \n\n";
  }

  // Finally, visit every basic block of the function.
  for (auto &BB: *F) {
    visitSILBasicBlock(&BB);
  }
}

void InstructionVisitor::visitSILBasicBlock(SILBasicBlock *BB) {
  if (SWAN_PRINT) {
    llvm::outs() << "Basic Block: " << BB << "\n";
    llvm::outs() << "Parent SILFunction: " << BB->getParent() << "\n";
  }

  // Clear information from previous basic block.
  InstructionCounter = 0;
  currentBasicBlock = std::make_unique<RootBasicBlockInfo>(Instance->CAst);

  for (auto arg : BB->getArguments()) {
    currentBasicBlock->addArg(MAKE_NODE3(CAstWrapper::PRIMITIVE,
      MAKE_CONST(addressToString(static_cast<ValueBase*>(arg)).c_str()), MAKE_CONST(arg->getType().getAsString().c_str())));
  }

  // Visit every instruction of the basic block.
  for (auto &I: *BB) {
    currentInstruction = std::make_unique<RootInstructionInfo>(Instance->CAst);
    visit(&I);
    currentInstruction->instructionName = getSILInstructionName(I.getKind());
    currentInstruction->setInstructionSourceInfo(instrInfo->startLine, instrInfo->startCol,
      instrInfo->endLine, instrInfo->endCol);
    currentBasicBlock->addInstruction(currentInstruction.get());
  }

  currentFunction->addBlock(currentBasicBlock.get());
}

//===----------- INSTRUCTION SOURCE INFORMATION PROCESSING ---------------===//

void InstructionVisitor::beforeVisit(SILInstruction *I) {
  // Set instruction source information.
  instrInfo = std::make_unique<SILInstructionInfo>();
  SourceManager &srcMgr = I->getModule().getSourceManager();
  SILLocation const &debugLoc = I->getDebugLocation().getLocation();
  SILLocation::DebugLoc const &debugInfo = debugLoc.decodeDebugLoc(srcMgr);
  // Set filename.
  instrInfo->Filename = debugInfo.Filename;
  if (debugInfo.Filename != "") {
    Instance->setSource(debugInfo.Filename);
  }
  // Set position.
  if (!I->getLoc().isNull()) {
    SourceRange const &srcRange = I->getLoc().getSourceRange();
    SourceLoc const &srcStart = srcRange.Start;
    SourceLoc const &srcEnd = srcRange.End;

    if (srcStart.isInvalid() && srcEnd.isInvalid()) {
      // This can happen if the instruction doesn't have corresponding Swift
      // code, most likely because it is a low level (usually memory related)
      // instruction. e.g. begin_access
      if (SWAN_PRINT) {
        llvm::outs() << "\t NOTE: Source information is invalid\n";
      }
    } else {
      if (srcStart.isValid()) {
        instrInfo->srcType = SILSourceType::STARTONLY;
        auto startLineCol = srcMgr.getLineAndColumn(srcStart);
        instrInfo->startLine = startLineCol.first;
        instrInfo->startCol = startLineCol.second;
      }
      if (srcEnd.isValid()) {
        auto endLineCol = srcMgr.getLineAndColumn(srcEnd);
        instrInfo->endLine = endLineCol.first;
        instrInfo->endCol = endLineCol.second;
        instrInfo->srcType = SILSourceType::FULL;
      }
    }
  }
  // Set memory behaviour.
  instrInfo->memBehavior = I->getMemoryBehavior();
  instrInfo->relBehavior = I->getReleasingBehavior();

  // Set other properties.
  instrInfo->num = InstructionCounter++;
  instrInfo->modInfo = moduleInfo.get();
  instrInfo->funcInfo = functionInfo.get();
  instrInfo->instrKind = I->getKind();

  // Set instruction operands.
  std::vector<void *> vals;
  for (const auto &op: I->getAllOperands()) {
    vals.push_back(op.get().getOpaqueValue());
  }
  instrInfo->ops = llvm::ArrayRef<void *>(vals);

  if (SWAN_PRINT) {
    if (SWAN_PRINT_SOURCE) {
      printSILInstructionInfo();
    }
    llvm::outs() << "<< " << getSILInstructionName(I->getKind()) << " >>\n";
  }
}

void InstructionVisitor::printSILInstructionInfo() {
  // llvm::outs() << "\t\t [INSTR] #" << instrInfo->num;
  // llvm::outs() << ", [OPNUM] " << instrInfo->id << "\n";
  if (SWAN_PRINT_FILE_AND_MEMORY) {
    llvm::outs() << "\t\t --> File: " << instrInfo->Filename << "\n";
    if (instrInfo->srcType == SILSourceType::INVALID) {
      llvm::outs() << "\t\t **** No source information. \n";
    } else { // Has at least start information.
      llvm::outs() << "\t\t\t ++++ Start - Line " << instrInfo->startLine << ":"
                   << instrInfo->startCol << "\n";
    }
    // Has end information.
    if (instrInfo->srcType == SILSourceType::FULL) {
      llvm::outs() << "\t\t\t ---- End - Line " << instrInfo->endLine;
      llvm::outs() << ":" << instrInfo->endCol << "\n";
    }
    // Memory Behavior.
    switch (instrInfo->memBehavior) {
      case SILInstruction::MemoryBehavior::None: {
        break;
      }
      case SILInstruction::MemoryBehavior::MayRead: {
        llvm::outs() << "\t\t +++ [MEM-R]: May read from memory. \n";
        break;
      }
      case SILInstruction::MemoryBehavior::MayWrite: {
        llvm::outs() << "\t\t +++ [MEM-W]: May write to memory. \n";
        break;
      }
      case SILInstruction::MemoryBehavior::MayReadWrite: {
        llvm::outs() << "\t\t +++ [MEM-RW]: May read or write memory. \n";
        break;
      }
      case SILInstruction::MemoryBehavior::MayHaveSideEffects: {
        llvm::outs() << "\t\t +++ [MEM-F]: May have side effects. \n";
      }
    }
    // Releasing Behavior.
    switch (instrInfo->relBehavior) {
      case SILInstruction::ReleasingBehavior::DoesNotRelease: {
        llvm::outs() << "\t\t [REL]: Does not release memory. \n";
        break;
      }
      case SILInstruction::ReleasingBehavior::MayRelease: {
        llvm::outs() << "\t\t [REL]: May release memory. \n";
        break;
      }
    }
  }
}

//===------------------------- UTLITY FUNCTIONS ----------------------------===//

void InstructionVisitor::handleSimpleInstr(SILInstruction *UIB) {
  std::list<jobject> operands;
  for (unsigned int i = 0; i < UIB->getNumOperands(); ++i) {
      auto InstOperand = UIB->getOperand(i);
      std::string OperandName = addressToString(InstOperand.getOpaqueValue());
      std::string OperandType = InstOperand->getType().getAsString();
      if (SWAN_PRINT) {
        llvm::outs() << "\t [OPER" << i << " NAME]: " << OperandName << "\n";
        llvm::outs() << "\t [OPER" << i << " TYPE]: " << OperandType << "\n";
      }
      operands.push_back(MAKE_NODE3(
        CAstWrapper::PRIMITIVE,
        MAKE_CONST(OperandName.c_str()),
        MAKE_CONST(OperandType.c_str())));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&operands)));
  std::list<jobject> results;
  for (unsigned int i = 0; i < UIB->getNumResults(); ++i) {
    auto InstResult = UIB->getResult(i);
    std::string ResultName = addressToString(InstResult.getOpaqueValue());
    std::string ResultType = InstResult->getType().getAsString();
    if (SWAN_PRINT) {
      llvm::outs() << "\t [RESULT" << i << " NAME]: " << ResultName << "\n";
      llvm::outs() << "\t [RESULT" << i << " TYPE]: " << ResultType << "\n";
    }
    results.push_back(MAKE_NODE3(
      CAstWrapper::PRIMITIVE,
      MAKE_CONST(ResultName.c_str()),
      MAKE_CONST(ResultType.c_str())));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&results)));
}

//===-------------------SPECIFIC INSTRUCTION VISITORS ----------------------===//

/*******************************************************************************/
/*                         ALLOCATION AND DEALLOCATION                         */
/*******************************************************************************/

void InstructionVisitor::visitAllocStackInst(AllocStackInst *ASI) {
  handleSimpleInstr(ASI);
}

void InstructionVisitor::visitAllocRefInst(AllocRefInst *ARI) {
  handleSimpleInstr(ARI);
}

void InstructionVisitor::visitAllocRefDynamicInst(AllocRefDynamicInst *ARDI) {
  handleSimpleInstr(ARDI);
}

void InstructionVisitor::visitAllocBoxInst(AllocBoxInst *ABI){
  handleSimpleInstr(ABI);
}

void InstructionVisitor::visitAllocValueBufferInst(AllocValueBufferInst *AVBI) {
  handleSimpleInstr(AVBI);
}

void InstructionVisitor::visitAllocGlobalInst(AllocGlobalInst *AGI) {
  SILGlobalVariable *Var = AGI->getReferencedGlobal();
  std::string VarName = Demangle::demangleSymbolAsString(Var->getName());
  std::string VarType = Var->getLoweredType().getAsString();
  if (SWAN_PRINT) {
    llvm::outs() << "\t [ALLOC NAME]:" << VarName << "\n";
    llvm::outs() << "\t [ALLOC TYPE]:" << VarType << "\n";
  }
  ADD_PROP(MAKE_CONST(VarName.c_str()));
  ADD_PROP(MAKE_CONST(VarType.c_str()));
}

void InstructionVisitor::visitDeallocStackInst(DeallocStackInst *DSI) {
  handleSimpleInstr(DSI);
}

void InstructionVisitor::visitDeallocBoxInst(DeallocBoxInst *DBI) {
  handleSimpleInstr(DBI);
}

void InstructionVisitor::visitProjectBoxInst(ProjectBoxInst *PBI) {
  handleSimpleInstr(PBI);
  ADD_PROP(MAKE_CONST(static_cast<int>(PBI->getFieldIndex())));
}

void InstructionVisitor::visitDeallocRefInst(DeallocRefInst *DRI) {
  handleSimpleInstr(DRI);
}

void InstructionVisitor::visitDeallocPartialRefInst(DeallocPartialRefInst *DPRI) {
  handleSimpleInstr(DPRI);
}

void InstructionVisitor::visitDeallocValueBufferInst(DeallocValueBufferInst *DVBI) {
  handleSimpleInstr(DVBI);
}

void InstructionVisitor::visitProjectValueBufferInst(ProjectValueBufferInst *PVBI) {
  handleSimpleInstr(PVBI);
}

/*******************************************************************************/
/*                        DEBUG INFROMATION                                    */
/*******************************************************************************/

void InstructionVisitor::visitDebugValueInst(DebugValueInst *DBI) {
  handleSimpleInstr(DBI);
}

void InstructionVisitor::visitDebugValueAddrInst(DebugValueAddrInst *DVAI) {
  handleSimpleInstr(DVAI);
}

/*******************************************************************************/
/*                        Accessing Memory                                     */
/*******************************************************************************/

void InstructionVisitor::visitLoadInst(LoadInst *LI) {
  handleSimpleInstr(LI);
}

void InstructionVisitor::visitStoreInst(StoreInst *SI) {
  std::string SourceName = addressToString(SI->getSrc().getOpaqueValue());
  std::string DestName = addressToString(SI->getDest().getOpaqueValue());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [SRC ADDR]: " << SourceName << "\n";
    llvm::outs() << "\t [DEST ADDR]: " << DestName << "\n";
  }
  ADD_PROP(MAKE_CONST(SourceName.c_str()));
  ADD_PROP(MAKE_CONST(DestName.c_str()));
}

void InstructionVisitor::visitStoreBorrowInst(StoreBorrowInst *SBI)
{
  // It seems the result of this instruction is never used, but it does
  // have one, unlike store. There is nothing in SIL.rst on this inst.
  std::string SourceName = addressToString(SBI->getSrc().getOpaqueValue());
  std::string DestName = addressToString(SBI->getDest().getOpaqueValue());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [SRC ADDR]: " << SourceName << "\n";
    llvm::outs() << "\t [DEST ADDR]: " << DestName << "\n";
  }
  ADD_PROP(MAKE_CONST(SourceName.c_str()));
  ADD_PROP(MAKE_CONST(DestName.c_str()));
}

void InstructionVisitor::visitLoadBorrowInst(LoadBorrowInst *LBI) {
  handleSimpleInstr(LBI);
}

void InstructionVisitor::visitBeginBorrowInst(BeginBorrowInst *BBI) {
  handleSimpleInstr(BBI);
}

void InstructionVisitor::visitEndBorrowInst(EndBorrowInst *EBI) {
  handleSimpleInstr(EBI);
}

void InstructionVisitor::visitAssignInst(AssignInst *AI) {
  std::string SourceName = addressToString(AI->getSrc().getOpaqueValue());
  std::string DestName = addressToString(AI->getDest().getOpaqueValue());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [SRC ADDR]: " << SourceName << "\n";
    llvm::outs() << "\t [DEST ADDR]: " << DestName << "\n";
  }
  ADD_PROP(MAKE_CONST(SourceName.c_str()));
  ADD_PROP(MAKE_CONST(DestName.c_str()));
}

void InstructionVisitor::visitAssignByWrapperInst(AssignByWrapperInst *ABWI) {
  handleSimpleInstr(ABWI);
}

void InstructionVisitor::visitMarkUninitializedInst(MarkUninitializedInst *MUI) {
  handleSimpleInstr(MUI);
}

void InstructionVisitor::visitMarkFunctionEscapeInst(MarkFunctionEscapeInst *MFEI) {
  handleSimpleInstr(MFEI);
}

void InstructionVisitor::visitCopyAddrInst(CopyAddrInst *CAI) {
  std::string SourceName = addressToString(CAI->getSrc().getOpaqueValue());
  std::string DestName = addressToString(CAI->getDest().getOpaqueValue());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [SRC ADDR]: " << SourceName << "\n";
    llvm::outs() << "\t [DEST ADDR]: " << DestName << "\n";
  }
  ADD_PROP(MAKE_CONST(SourceName.c_str()));
  ADD_PROP(MAKE_CONST(DestName.c_str()));
}

void InstructionVisitor::visitDestroyAddrInst(DestroyAddrInst *DAI) {
  handleSimpleInstr(DAI);
}

void InstructionVisitor::visitIndexAddrInst(IndexAddrInst *IAI) {
  handleSimpleInstr(IAI);
}

void InstructionVisitor::visitTailAddrInst(TailAddrInst *TAI) {
  handleSimpleInstr(TAI);
  std::string BaseName = addressToString(TAI->getBase().getOpaqueValue());
  std::string IndexName = addressToString(TAI->getIndex().getOpaqueValue());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [BASE NAME: " << BaseName << "\n";
    llvm::outs() << "\t [DEST ADDR]: " << IndexName << "\n";
  }
  ADD_PROP(MAKE_CONST(BaseName.c_str()));
  ADD_PROP(MAKE_CONST(IndexName.c_str()));
}

void InstructionVisitor::visitIndexRawPointerInst(IndexRawPointerInst *IRPI) {
  handleSimpleInstr(IRPI);
  std::string BaseName = addressToString(IRPI->getBase().getOpaqueValue());
  std::string IndexName = addressToString(IRPI->getIndex().getOpaqueValue());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [BASE NAME: " << BaseName << "\n";
    llvm::outs() << "\t [DEST ADDR]: " << IndexName << "\n";
  }
  ADD_PROP(MAKE_CONST(BaseName.c_str()));
  ADD_PROP(MAKE_CONST(IndexName.c_str()));
}

void InstructionVisitor::visitBindMemoryInst(BindMemoryInst *BMI) {
  std::string BaseName = addressToString(BMI->getBase().getOpaqueValue());
  std::string IndexName = addressToString(BMI->getIndex().getOpaqueValue());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [BASE NAME: " << BaseName << "\n";
    llvm::outs() << "\t [DEST ADDR]: " << IndexName << "\n";
  }
  ADD_PROP(MAKE_CONST(BaseName.c_str()));
  ADD_PROP(MAKE_CONST(IndexName.c_str()));
}

void InstructionVisitor::visitBeginAccessInst(BeginAccessInst *BAI) {
  handleSimpleInstr(BAI);
}

void InstructionVisitor::visitEndAccessInst(EndAccessInst *EAI) {
  handleSimpleInstr(EAI);
}

void InstructionVisitor::visitBeginUnpairedAccessInst(BeginUnpairedAccessInst *BUI) {
  handleSimpleInstr(BUI);
  std::string SourceName = addressToString(BUI->getSource().getOpaqueValue());
  std::string BufferName = addressToString(BUI->getBuffer().getOpaqueValue());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [SOURCE NAME]: " << SourceName << "\n";
    llvm::outs() << "\t [BUFFER NAME]: " << BufferName << "\n";
  }
  ADD_PROP(MAKE_CONST(SourceName.c_str()));
  ADD_PROP(MAKE_CONST(BufferName.c_str()));
}

void InstructionVisitor::visitEndUnpairedAccessInst(EndUnpairedAccessInst *EUAI) {
  handleSimpleInstr(EUAI);
}

/*******************************************************************************/
/*                        Reference Counting                                   */
/*******************************************************************************/

void InstructionVisitor::visitStrongRetainInst(StrongRetainInst *SRTI) {
  handleSimpleInstr(SRTI);
}

void InstructionVisitor::visitStrongReleaseInst(StrongReleaseInst *SRLI) {
  handleSimpleInstr(SRLI);
}

void InstructionVisitor::visitSetDeallocatingInst(SetDeallocatingInst *SDI)  {
  handleSimpleInstr(SDI);
}

void InstructionVisitor::visitCopyUnownedValueInst(CopyUnownedValueInst *CUVI) {
  handleSimpleInstr(CUVI);
}

void InstructionVisitor::visitStrongRetainUnownedInst(StrongRetainUnownedInst *SRUI) {
  handleSimpleInstr(SRUI);
}

void InstructionVisitor::visitUnownedRetainInst(UnownedRetainInst *URTI) {
  handleSimpleInstr(URTI);
}

void InstructionVisitor::visitUnownedReleaseInst(UnownedReleaseInst *URLI) {
  handleSimpleInstr(URLI);
}

void InstructionVisitor::visitLoadWeakInst(LoadWeakInst *LWI) {
  handleSimpleInstr(LWI);
}

void InstructionVisitor::visitStoreWeakInst(StoreWeakInst *SWI) {
  std::string SourceName = addressToString(SWI->getSrc().getOpaqueValue());
  std::string DestName = addressToString(SWI->getDest().getOpaqueValue());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [SRC ADDR]: " << SourceName << "\n";
    llvm::outs() << "\t [DEST ADDR]: " << DestName << "\n";
  }
  ADD_PROP(MAKE_CONST(SourceName.c_str()));
  ADD_PROP(MAKE_CONST(DestName.c_str()));
}

void InstructionVisitor::visitLoadUnownedInst(LoadUnownedInst *LUI) {
  handleSimpleInstr(LUI);
}

void InstructionVisitor::visitStoreUnownedInst(StoreUnownedInst *SUI) {
  std::string SourceName = addressToString(SUI->getSrc().getOpaqueValue());
  std::string DestName = addressToString(SUI->getDest().getOpaqueValue());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [SRC ADDR]: " << SourceName << "\n";
    llvm::outs() << "\t [DEST ADDR]: " << DestName << "\n";
  }
  ADD_PROP(MAKE_CONST(SourceName.c_str()));
  ADD_PROP(MAKE_CONST(DestName.c_str()));
}

void InstructionVisitor::visitFixLifetimeInst(FixLifetimeInst *FLI) {
  handleSimpleInstr(FLI);
}

void InstructionVisitor::visitEndLifetimeInst(EndLifetimeInst *ELI) {
  handleSimpleInstr(ELI);
}

void InstructionVisitor::visitMarkDependenceInst(MarkDependenceInst *MDI) {
  handleSimpleInstr(MDI);
  std::string BaseName = addressToString(MDI->getBase().getOpaqueValue());
  std::string ValueName = addressToString(MDI->getValue().getOpaqueValue());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [BASE NAME]:" << BaseName << "\n";
    llvm::outs() << "\t [VALUE NAME]:" << ValueName << "\n";
  }
  ADD_PROP(MAKE_CONST(BaseName.c_str()));
  ADD_PROP(MAKE_CONST(ValueName.c_str()));
}

void InstructionVisitor::visitIsUniqueInst(IsUniqueInst *IUI) {
  handleSimpleInstr(IUI);
}

void InstructionVisitor::visitIsEscapingClosureInst(IsEscapingClosureInst *IECI) {
  handleSimpleInstr(IECI);
}

void InstructionVisitor::visitCopyBlockInst(CopyBlockInst *CBI) {
  handleSimpleInstr(CBI);
}

void InstructionVisitor::visitCopyBlockWithoutEscapingInst(CopyBlockWithoutEscapingInst *CBWEI) {
  handleSimpleInstr(CBWEI);
}

/*******************************************************************************/
/*                         Literals                                            */
/*******************************************************************************/

void InstructionVisitor::visitFunctionRefInst(FunctionRefInst *FRI) {
  handleSimpleInstr(FRI);
  SILFunction *referencedFunction = FRI->getReferencedFunctionOrNull();
  std::string FuncName = Demangle::demangleSymbolAsString(referencedFunction->getName());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [FUNC NAME]:" << FuncName << "\n";
  }
  ADD_PROP(MAKE_CONST(FuncName.c_str()));
}

void InstructionVisitor::visitDynamicFunctionRefInst(DynamicFunctionRefInst *DFRI) {
  handleSimpleInstr(DFRI);
  SILFunction *referencedFunction = DFRI->getReferencedFunctionOrNull();
  std::string FuncName = Demangle::demangleSymbolAsString(referencedFunction->getName());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [FUNC NAME]:" << FuncName << "\n";
  }
  ADD_PROP(MAKE_CONST(FuncName.c_str()));
}

void InstructionVisitor::visitPreviousDynamicFunctionRefInst(PreviousDynamicFunctionRefInst *PDFRI) {
  handleSimpleInstr(PDFRI);
  SILFunction *referencedFunction = PDFRI->getReferencedFunctionOrNull();
  std::string FuncName = Demangle::demangleSymbolAsString(referencedFunction->getName());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [FUNC NAME]:" << FuncName << "\n";
  }
  ADD_PROP(MAKE_CONST(FuncName.c_str()));
}

void InstructionVisitor::visitGlobalAddrInst(GlobalAddrInst *GAI) {
  handleSimpleInstr(GAI);
  SILGlobalVariable *Var = GAI->getReferencedGlobal();
  std::string VarName = Demangle::demangleSymbolAsString(Var->getName());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [GLOBAL NAME]:" << VarName << "\n";
  }
  ADD_PROP(MAKE_CONST(VarName.c_str()));
}

void InstructionVisitor::visitGlobalValueInst(GlobalValueInst *GVI) {
  handleSimpleInstr(GVI);
  SILGlobalVariable *Var = GVI->getReferencedGlobal();
  std::string VarName = Demangle::demangleSymbolAsString(Var->getName());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [GLOBAL NAME]:" << VarName << "\n";
  }
  ADD_PROP(MAKE_CONST(VarName.c_str()));
}

void InstructionVisitor::visitIntegerLiteralInst(IntegerLiteralInst *ILI) {
  handleSimpleInstr(ILI);
  APInt Value = ILI->getValue();
  jobject Node = nullptr;
  if (Value.isNegative()) {
    if (Value.getMinSignedBits() <= 32) {
      Node = MAKE_CONST(static_cast<int>(Value.getSExtValue()));
    } else if (Value.getMinSignedBits() <= 64) {
      Node = MAKE_CONST(static_cast<long>(Value.getSExtValue()));
    }
  } else {
    if (Value.getActiveBits() <= 32) {
      Node = MAKE_CONST(static_cast<int>(Value.getZExtValue()));
    } else if (Value.getActiveBits() <= 64) {
      Node = MAKE_CONST(static_cast<long>(Value.getZExtValue()));
    }
  }
  if (Node != nullptr) {
    if (SWAN_PRINT) {
       llvm::outs() << "\t [VALUE]:" << Value.getZExtValue() << "\n";
    }
  } else {
    llvm::outs() << "\t ERROR: Undefined integer_literal behaviour\n";
  }
  ADD_PROP(Node);
}

void InstructionVisitor::visitFloatLiteralInst(FloatLiteralInst *FLI) {
  handleSimpleInstr(FLI);
  APFloat Value = FLI->getValue();
  jobject Node = nullptr;
  if (&Value.getSemantics() == &APFloat::IEEEsingle()) {
    Node = MAKE_CONST(Value.convertToFloat());
    if (SWAN_PRINT) {
      llvm::outs() << "\t [VALUE]:" << static_cast<double>(Value.convertToFloat()) << "\n";
    }
  }
  else if (&Value.getSemantics() == &APFloat::IEEEdouble()) {
    Node = MAKE_CONST(Value.convertToDouble());
    if (SWAN_PRINT) {
      llvm::outs() << "\t [VALUE]:" << Value.convertToDouble() << "\n";
    }
  }
  else if (Value.isFinite()) {
    SmallVector<char, 128> buf;
    Value.toString(buf);
    jobject BigDecimal = Instance->makeBigDecimal(buf.data(), static_cast<int>(buf.size()));
    Node = MAKE_CONST(BigDecimal);
    if (SWAN_PRINT) {
      llvm::outs() << "\t [VALUE]:" << buf << "\n";
    }
  }
  else {
    bool APFLosesInfo;
    Value.convert(APFloat::IEEEdouble(), APFloat::rmNearestTiesToEven, &APFLosesInfo);
    Node = MAKE_CONST(Value.convertToDouble());
    if (SWAN_PRINT) {
      llvm::outs() << "\t [VALUE]:" << Value.convertToDouble() << "\n";
    }
  }
  ADD_PROP(Node);
}

void InstructionVisitor::visitStringLiteralInst(StringLiteralInst *SLI) {
  handleSimpleInstr(SLI);
  std::string Value = SLI->getValue();
  if (SWAN_PRINT) {
    llvm::outs() << "\t [VALUE]: " << Value << "\n";
  }
  ADD_PROP(MAKE_CONST(Value.c_str()));
}

/*******************************************************************************/
/*                         Dynamic Dispatch                                    */
/*******************************************************************************/

void InstructionVisitor::visitClassMethodInst(ClassMethodInst *CMI) {
  handleSimpleInstr(CMI);
  std::string FunctionName = Demangle::demangleSymbolAsString(CMI->getMember().mangle());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [CLASS METHOD]: " << FunctionName << "\n";
  }
  ADD_PROP(MAKE_CONST(FunctionName.c_str()));
}

void InstructionVisitor::visitObjCMethodInst(ObjCMethodInst *AMI) {
  handleSimpleInstr(AMI);
  std::string FunctionName = Demangle::demangleSymbolAsString(AMI->getMember().mangle());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [METHOD NAME]: " << FunctionName << "\n";
  }
  ADD_PROP(MAKE_CONST(FunctionName.c_str()));
}

void InstructionVisitor::visitSuperMethodInst(SuperMethodInst *SMI) {
  handleSimpleInstr(SMI);
  std::string FunctionName = Demangle::demangleSymbolAsString(SMI->getMember().mangle());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [CLASS METHOD]: " << FunctionName << "\n";
  }
  ADD_PROP(MAKE_CONST(FunctionName.c_str()));
}

void InstructionVisitor::visitObjCSuperMethodInst(ObjCSuperMethodInst *ASMI) {
  handleSimpleInstr(ASMI);
  std::string FunctionName = Demangle::demangleSymbolAsString(ASMI->getMember().mangle());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [CLASS METHOD]: " << FunctionName << "\n";
  }
  ADD_PROP(MAKE_CONST(FunctionName.c_str()));
}

// Interesting code related to protocols
// https://github.com/apple/swift/blob/c8be802c430ebfa6c38546f626ebe72fe2a07452/lib/SIL/Linker.cpp#L206
void InstructionVisitor::visitWitnessMethodInst(WitnessMethodInst *WMI) {
  handleSimpleInstr(WMI);
  std::string FunctionName = Demangle::demangleSymbolAsString(WMI->getMember().mangle());
  std::string protocolName = WMI->getLookupProtocol()->getName().str();
  if (SWAN_PRINT) {
    llvm::outs() << "\t [Function]: " << FunctionName << "\n";
    llvm::outs() << "\t [PROTOCOL]: " << protocolName << "\n";
  }
  std::list<jobject> functions;
  if (witnessTable.find(protocolName) == witnessTable.end()) {
    return;
  }
  for (auto it = witnessTable.find(protocolName)->second.begin(); it != witnessTable.find(protocolName)->second.end(); ++it) {
    if ((*it).find(FunctionName) != std::string::npos) {
      functions.push_back(MAKE_CONST((*it).c_str()));
    }
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&functions)));
}

/*******************************************************************************/
/*                         Function Application                                */
/*******************************************************************************/

void InstructionVisitor::visitApplyInst(ApplyInst *AI) {
  handleSimpleInstr(AI);
  auto *Callee = AI->getReferencedFunctionOrNull();
  std::list<jobject> arguments;
  for (auto arg : AI->getArguments()) {
    arguments.push_back(MAKE_CONST(addressToString(arg.getOpaqueValue()).c_str()));
  }
  ADD_PROP(MAKE_CONST(addressToString(AI->getOperand(0).getOpaqueValue()).c_str()));
  if (SWAN_PRINT) {
    llvm::outs() << "\t [FUNC REF ADDR]: " << AI->getOperand(0).getOpaqueValue() << "\n";
  }
  if (SWAN_PRINT) {
    for (auto arg : arguments) {
      llvm::outs() << "\t\t [ARG]: " << Instance->CAst->getConstantValue(arg) << "\n";
    }
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&arguments)));
  if (Callee) {
    auto *FD = Callee->getLocation().getAsASTNode<FuncDecl>();
    if (FD && (FD->isUnaryOperator() || FD->isBinaryOperator())) {
      jobject OperatorNode = MAKE_CONST(FD->getName().str().str().c_str());
      arguments.push_front(OperatorNode);
      if (SWAN_PRINT) {
        llvm::outs() << "\t [OPERATOR NAME]:" << Instance->CAst->getConstantValue(OperatorNode) << "\n";
      }
      if (FD->isUnaryOperator()) {
        ADD_PROP(MAKE_NODE2(CAstWrapper::UNARY_EXPR, MAKE_ARRAY(&arguments)));
      } else if (FD->isBinaryOperator()) {
        ADD_PROP(MAKE_NODE2(CAstWrapper::BINARY_EXPR, MAKE_ARRAY(&arguments)));
      }
    }
  }
}

void InstructionVisitor::visitBeginApplyInst(BeginApplyInst *BAI) {
  auto yieldedValues = BAI->getYieldedValues();
  std::list<jobject> results;
  for (auto result : yieldedValues) {
    results.push_back(MAKE_NODE3(CAstWrapper::PRIMITIVE,
      MAKE_CONST(addressToString(result.getOpaqueValue()).c_str()),
      MAKE_CONST(result->getType().getAsString().c_str())));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&results)));
  auto *Callee = BAI->getReferencedFunctionOrNull();
  std::list<jobject> arguments;
  for (auto arg : BAI->getArguments()) {
    arguments.push_back(MAKE_CONST(addressToString(arg.getOpaqueValue()).c_str()));
  }
  ADD_PROP(MAKE_CONST(addressToString(BAI->getTokenResult().getOpaqueValue()).c_str()));
  ADD_PROP(MAKE_CONST(addressToString(BAI->getOperand(0).getOpaqueValue()).c_str()));
  if (SWAN_PRINT) {
    llvm::outs() << "\t [FUNC REF ADDR]: " << BAI->getOperand(0).getOpaqueValue() << "\n";
  }
  if (SWAN_PRINT) {
    for (auto arg : arguments) {
      llvm::outs() << "\t\t [ARG]: " << Instance->CAst->getConstantValue(arg) << "\n";
    }
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&arguments)));
  if (Callee) {
    auto *FD = Callee->getLocation().getAsASTNode<FuncDecl>();
    if (FD && (FD->isUnaryOperator() || FD->isBinaryOperator())) {
      jobject OperatorNode = MAKE_CONST(FD->getName().str().str().c_str());
      arguments.push_front(OperatorNode);
      if (SWAN_PRINT) {
        llvm::outs() << "\t [OPERATOR NAME]:" << Instance->CAst->getConstantValue(OperatorNode) << "\n";
      }
      if (FD->isUnaryOperator()) {
        ADD_PROP(MAKE_NODE2(CAstWrapper::UNARY_EXPR, MAKE_ARRAY(&arguments)));
      } else if (FD->isBinaryOperator()) {
        ADD_PROP(MAKE_NODE2(CAstWrapper::BINARY_EXPR, MAKE_ARRAY(&arguments)));
      }
    }
  }
}

void InstructionVisitor::visitAbortApplyInst(AbortApplyInst *AAI) {
  handleSimpleInstr(AAI);
}

void InstructionVisitor::visitEndApplyInst(EndApplyInst *EAI) {
  handleSimpleInstr(EAI);
}

void InstructionVisitor::visitPartialApplyInst(PartialApplyInst *PAI) {
  handleSimpleInstr(PAI);
  auto *Callee = PAI->getReferencedFunctionOrNull();
  std::list<jobject> arguments;
  for (auto arg : PAI->getArguments()) {
    arguments.push_back(MAKE_CONST(addressToString(arg.getOpaqueValue()).c_str()));
  }
  ADD_PROP(MAKE_CONST(addressToString(PAI->getOperand(0).getOpaqueValue()).c_str()));
  if (SWAN_PRINT) {
    llvm::outs() << "\t [FUNC REF ADDR]: " << PAI->getOperand(0).getOpaqueValue() << "\n";
  }
  if (SWAN_PRINT) {
    for (auto arg : arguments) {
      llvm::outs() << "\t\t [ARG]: " << Instance->CAst->getConstantValue(arg) << "\n";
    }
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&arguments)));
  if (Callee) {
    auto *FD = Callee->getLocation().getAsASTNode<FuncDecl>();
    if (FD && (FD->isUnaryOperator() || FD->isBinaryOperator())) {
      jobject OperatorNode = MAKE_CONST(FD->getName().str().str().c_str());
      arguments.push_front(OperatorNode);
      if (SWAN_PRINT) {
        llvm::outs() << "\t [OPERATOR NAME]:" << Instance->CAst->getConstantValue(OperatorNode) << "\n";
      }
      if (FD->isUnaryOperator()) {
        ADD_PROP(MAKE_NODE2(CAstWrapper::UNARY_EXPR, MAKE_ARRAY(&arguments)));
      } else if (FD->isBinaryOperator()) {
        ADD_PROP(MAKE_NODE2(CAstWrapper::BINARY_EXPR, MAKE_ARRAY(&arguments)));
      }
    }
  }
}

void InstructionVisitor::visitBuiltinInst(BuiltinInst *BI) {
  handleSimpleInstr(BI);
  std::list<jobject> arguments;
  std::string CalleeName = BI->getName().str();
  currentFunction->addCalledFunction(CalleeName);
  if (SWAN_PRINT) {
    llvm::outs() << "\t [CALLEE NAME]:" << CalleeName << "\n";
  }
  for (auto arg : BI->getArguments()) {
    llvm::outs() << "\t\t [ARG]: " << arg.getOpaqueValue() << "\n";
    arguments.push_back(MAKE_CONST(addressToString(arg.getOpaqueValue()).c_str()));
  }
  ADD_PROP(MAKE_CONST(CalleeName.c_str()));
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&arguments)));

}

/*******************************************************************************/
/*                          Metatypes                                          */
/*******************************************************************************/

void InstructionVisitor::visitMetatypeInst(MetatypeInst *MI) {
  handleSimpleInstr(MI);
}

void InstructionVisitor::visitValueMetatypeInst(ValueMetatypeInst *VMI) {
  handleSimpleInstr(VMI);
}

void InstructionVisitor::visitExistentialMetatypeInst(ExistentialMetatypeInst *EMI) {
  handleSimpleInstr(EMI);
}

void InstructionVisitor::visitObjCProtocolInst(ObjCProtocolInst *OPI) {
  handleSimpleInstr(OPI);
  std::string ProtocolName = OPI->getProtocol()->getName().str();
  if (SWAN_PRINT) {
    llvm::outs() << "\t [PROTOCOL NAME]:" << ProtocolName << "\n";
  }
  ADD_PROP(MAKE_CONST(ProtocolName.c_str()));
}

/*******************************************************************************/
/*                          Aggregate Types                                    */
/*******************************************************************************/

void InstructionVisitor::visitRetainValueInst(RetainValueInst *RVI) {
  handleSimpleInstr(RVI);
}

void InstructionVisitor::visitRetainValueAddrInst(RetainValueAddrInst *RVAI) {
  handleSimpleInstr(RVAI);
}

void InstructionVisitor::visitUnmanagedRetainValueInst(UnmanagedRetainValueInst *URVI) {
  handleSimpleInstr(URVI);
}

void InstructionVisitor::visitCopyValueInst(CopyValueInst *CVI) {
  handleSimpleInstr(CVI);
}

void InstructionVisitor::visitReleaseValueInst(ReleaseValueInst *REVI) {
  handleSimpleInstr(REVI);
}

void InstructionVisitor::visitReleaseValueAddrInst(ReleaseValueAddrInst *REVAI) {
  handleSimpleInstr(REVAI);
}

void InstructionVisitor::visitUnmanagedReleaseValueInst(UnmanagedReleaseValueInst *UREVI) {
  handleSimpleInstr(UREVI);
}

void InstructionVisitor::visitDestroyValueInst(DestroyValueInst *DVI) {
  handleSimpleInstr(DVI);
}

void InstructionVisitor::visitAutoreleaseValueInst(AutoreleaseValueInst *AREVI) {
  handleSimpleInstr(AREVI);
}

void InstructionVisitor::visitTupleInst(TupleInst *TI) {
  handleSimpleInstr(TI);
  std::list<jobject> Fields;
  for (Operand &op : TI->getElementOperands()) {
    SILValue opValue = op.get();
    if (SWAN_PRINT) {
      llvm::outs() << "\t\t\t [FIELD VALUE]: " << opValue.getOpaqueValue() << "\n";
      llvm::outs() << "\t\t\t [FIELD TYPE]: " << opValue->getType().getAsString() << "\n";
    }
    Fields.push_back(MAKE_NODE3(CAstWrapper::PRIMITIVE,
      MAKE_CONST(addressToString(opValue.getOpaqueValue()).c_str()),
      MAKE_CONST(opValue->getType().getAsString().c_str())));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Fields)));
}

void InstructionVisitor::visitTupleExtractInst(TupleExtractInst *TEI) {
  handleSimpleInstr(TEI);
  std::string TupleValueName = addressToString(TEI->getOperand().getOpaqueValue());
  unsigned int FieldName = TEI->getFieldNo();
  if (SWAN_PRINT) {
      llvm::outs() << "\t [TUPLE NAME]: " << TupleValueName << "\n";
      llvm::outs() << "\t [FIELD NO]: " << FieldName << "\n";
  }
  ADD_PROP(MAKE_CONST(TupleValueName.c_str()));
  ADD_PROP(MAKE_CONST(static_cast<int>(FieldName)));
}

void InstructionVisitor::visitTupleElementAddrInst(TupleElementAddrInst *TEAI) {
  handleSimpleInstr(TEAI);
  std::string TupleValueName = addressToString(TEAI->getOperand().getOpaqueValue());
  unsigned int FieldName = TEAI->getFieldNo();
  if (SWAN_PRINT) {
      llvm::outs() << "\t [TUPLE NAME]: " << TupleValueName << "\n";
      llvm::outs() << "\t [FIELD NO]: " << FieldName << "\n";
  }
  ADD_PROP(MAKE_CONST(TupleValueName.c_str()));
  ADD_PROP(MAKE_CONST(static_cast<int>(FieldName)));
}

void InstructionVisitor::visitDestructureTupleInst(DestructureTupleInst *DTI) {
  handleSimpleInstr(DTI);
}

void InstructionVisitor::visitStructInst(StructInst *SI) {
  handleSimpleInstr(SI);
  std::list<jobject> Fields;
  auto property = SI->getStructDecl()->getStoredProperties().begin();
  for (Operand &op : SI->getElementOperands()) {
    assert(property != SI->getStructDecl()->getStoredProperties().end());
    VarDecl *field = *property;
    SILValue opValue = op.get();
    if (SWAN_PRINT) {
      llvm::outs() << "\t\t\t [FIELD]: " << field->getNameStr() << "\n";
      llvm::outs() << "\t\t\t [VALUE]: " << opValue.getOpaqueValue() << "\n";
    }
    Fields.push_back(MAKE_NODE3(CAstWrapper::PRIMITIVE,
      MAKE_CONST(field->getNameStr().str().c_str()),
      MAKE_CONST(addressToString(opValue.getOpaqueValue()).c_str())));
    ++property;
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Fields)));
}

void InstructionVisitor::visitStructExtractInst(StructExtractInst *SEI) {
  handleSimpleInstr(SEI);
  std::string StructValueName = addressToString(SEI->getOperand().getOpaqueValue());
  std::string FieldName = SEI->getField()->getNameStr().str();
  if (SWAN_PRINT) {
      llvm::outs() << "\t [STRUCT NAME]: " << StructValueName << "\n";
      llvm::outs() << "\t [FIELD]: " << FieldName << "\n";
  }
  ADD_PROP(MAKE_CONST(StructValueName.c_str()));
  ADD_PROP(MAKE_CONST(FieldName.c_str()));
}

void InstructionVisitor::visitStructElementAddrInst(StructElementAddrInst *SEAI) {
  handleSimpleInstr(SEAI);
  std::string StructValueName = addressToString(SEAI->getOperand().getOpaqueValue());
  std::string FieldName = SEAI->getField()->getNameStr().str();
  if (SWAN_PRINT) {
      llvm::outs() << "\t [STRUCT NAME]: " << StructValueName << "\n";
      llvm::outs() << "\t [FIELD]: " << FieldName << "\n";
  }
  ADD_PROP(MAKE_CONST(StructValueName.c_str()));
  ADD_PROP(MAKE_CONST(FieldName.c_str()));
}

void InstructionVisitor::visitDestructureStructInst(DestructureStructInst *DSI) {
  handleSimpleInstr(DSI);
}

void InstructionVisitor::visitObjectInst(ObjectInst *OI) {
  handleSimpleInstr(OI);
  // Unclear if getType() is actually the object type here.
  std::string ObjectType = OI->getType().getAsString();
  if (SWAN_PRINT) {
    llvm::outs() << "\t\t [OBJECT TYPE]:" << ObjectType << "\n";
  }
  ADD_PROP(MAKE_CONST(ObjectType.c_str()));
}

void InstructionVisitor::visitRefElementAddrInst(RefElementAddrInst *REAI) {
  handleSimpleInstr(REAI);
  std::string FieldName = REAI->getField()->getNameStr().str();
  if (SWAN_PRINT) {
    llvm::outs() << "\t [FIELD NAME]: " << FieldName << "\n";
  }
  ADD_PROP(MAKE_CONST(FieldName.c_str()));
}

void InstructionVisitor::visitRefTailAddrInst(RefTailAddrInst *RTAI) {
  handleSimpleInstr(RTAI);
  std::string TailType = RTAI->getTailType().getAsString();
  if (SWAN_PRINT) {
    llvm::outs() << "\t [TAIL TYPE]: " << TailType << "\n";
  }
  ADD_PROP(MAKE_CONST(TailType.c_str()));
}

/*******************************************************************************/
/*                          Enums                                              */
/*******************************************************************************/

void InstructionVisitor::visitEnumInst(EnumInst *EI) {
  handleSimpleInstr(EI);
  std::string EnumName = EI->getElement()->getParentEnum()->getName().str();
  std::string CaseName = EI->getElement()->getNameStr();
  if (SWAN_PRINT) {
    llvm::outs() << "\t [ENUM NAME]: " << EnumName << "\n";
    llvm::outs() << "\t [CASE NAME]: " << CaseName << "\n";
  }
  ADD_PROP(MAKE_CONST(EnumName.c_str()));
  ADD_PROP(MAKE_CONST(CaseName.c_str()));
}

void InstructionVisitor::visitUncheckedEnumDataInst(UncheckedEnumDataInst *UEDI) {
  handleSimpleInstr(UEDI);
  std::string EnumName = UEDI->getElement()->getParentEnum()->getName().str();
  std::string CaseName = UEDI->getElement()->getNameStr();
  if (SWAN_PRINT) {
    llvm::outs() << "\t [ENUM NAME]: " << EnumName << "\n";
    llvm::outs() << "\t [CASE NAME]: " << CaseName << "\n";
  }
  ADD_PROP(MAKE_CONST(EnumName.c_str()));
  ADD_PROP(MAKE_CONST(CaseName.c_str()));
}

void InstructionVisitor::visitInitEnumDataAddrInst(InitEnumDataAddrInst *UDAI) {
  handleSimpleInstr(UDAI);
  std::string EnumName = UDAI->getElement()->getParentEnum()->getName().str();
  std::string CaseName = UDAI->getElement()->getNameStr();
  if (SWAN_PRINT) {
    llvm::outs() << "\t [ENUM NAME]: " << EnumName << "\n";
    llvm::outs() << "\t [CASE NAME]: " << CaseName << "\n";
  }
  ADD_PROP(MAKE_CONST(EnumName.c_str()));
  ADD_PROP(MAKE_CONST(CaseName.c_str()));
}

void InstructionVisitor::visitInjectEnumAddrInst(InjectEnumAddrInst *IUAI) {
  handleSimpleInstr(IUAI);
  std::string EnumName = IUAI->getElement()->getParentEnum()->getName().str();
  std::string CaseName = IUAI->getElement()->getNameStr();
  if (SWAN_PRINT) {
    llvm::outs() << "\t [ENUM NAME]: " << EnumName << "\n";
    llvm::outs() << "\t [CASE NAME]: " << CaseName << "\n";
  }
  ADD_PROP(MAKE_CONST(EnumName.c_str()));
  ADD_PROP(MAKE_CONST(CaseName.c_str()));
}

void InstructionVisitor::visitUncheckedTakeEnumDataAddrInst(UncheckedTakeEnumDataAddrInst *UDAI) {
  handleSimpleInstr(UDAI);
  std::string EnumName = UDAI->getElement()->getParentEnum()->getName().str();
  std::string CaseName = UDAI->getElement()->getNameStr();
  if (SWAN_PRINT) {
    llvm::outs() << "\t [ENUM NAME]: " << EnumName << "\n";
    llvm::outs() << "\t [CASE NAME]: " << CaseName << "\n";
  }
  ADD_PROP(MAKE_CONST(EnumName.c_str()));
  ADD_PROP(MAKE_CONST(CaseName.c_str()));
}

void InstructionVisitor::visitSelectEnumInst(SelectEnumInst *SEI) {
  handleSimpleInstr(SEI);
  std::string EnumName = addressToString(SEI->getOperand().getOpaqueValue());
  ADD_PROP(MAKE_CONST(EnumName.c_str()));
  std::list<jobject> Cases;
  for (unsigned int i = 0; i < SEI->getNumCases(); ++i) {
    auto Case = SEI->getCase(i);
    EnumElementDecl *CaseDecl = Case.first;
    std::string CaseName = CaseDecl->getNameStr().str();
    std::string CaseValue = addressToString(Case.second);
    std::list<jobject> Fields;
    Fields.push_back(MAKE_CONST(CaseName.c_str()));
    Fields.push_back(MAKE_CONST(CaseValue.c_str()));
    Cases.push_back(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Fields)));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Cases)));
  if (SEI->hasDefault()) {
    std::string DefaultValue = addressToString(SEI->getDefaultResult().getOpaqueValue());
    ADD_PROP(MAKE_CONST(DefaultValue.c_str()));
  }
}

void InstructionVisitor::visitSelectEnumAddrInst(SelectEnumAddrInst *SEAI) {
  handleSimpleInstr(SEAI);
  std::string EnumName = addressToString(SEAI->getOperand().getOpaqueValue());
  ADD_PROP(MAKE_CONST(EnumName.c_str()));
  std::list<jobject> Cases;
  for (unsigned int i = 0; i < SEAI->getNumCases(); ++i) {
    auto Case = SEAI->getCase(i);
    EnumElementDecl *CaseDecl = Case.first;
    std::string CaseName = CaseDecl->getNameStr().str();
    std::string CaseValue = addressToString(Case.second);
    std::list<jobject> Fields;
    Fields.push_back(MAKE_CONST(CaseName.c_str()));
    Fields.push_back(MAKE_CONST(CaseValue.c_str()));
    Cases.push_back(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Fields)));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Cases)));
  if (SEAI->hasDefault()) {
    std::string DefaultValue = addressToString(SEAI->getDefaultResult().getOpaqueValue());
    ADD_PROP(MAKE_CONST(DefaultValue.c_str()));
  }
}

/*******************************************************************************/
/*                          Protocol and Protocol Composition Types            */
/*******************************************************************************/

void InstructionVisitor::visitInitExistentialAddrInst(InitExistentialAddrInst *IEAI) {
  handleSimpleInstr(IEAI);
}

void InstructionVisitor::visitInitExistentialValueInst(InitExistentialValueInst *IEVI) {
  handleSimpleInstr(IEVI);
}

void InstructionVisitor::visitDeinitExistentialAddrInst(DeinitExistentialAddrInst *DEAI) {
  handleSimpleInstr(DEAI);
}

void InstructionVisitor::visitDeinitExistentialValueInst(DeinitExistentialValueInst *DEVI) {
  handleSimpleInstr(DEVI);
}

void InstructionVisitor::visitOpenExistentialAddrInst(OpenExistentialAddrInst *OEAI) {
  handleSimpleInstr(OEAI);
}

void InstructionVisitor::visitOpenExistentialValueInst(OpenExistentialValueInst *OEVI) {
  handleSimpleInstr(OEVI);
}

void InstructionVisitor::visitInitExistentialRefInst(InitExistentialRefInst *IERI) {
  handleSimpleInstr(IERI);
}

void InstructionVisitor::visitOpenExistentialRefInst(OpenExistentialRefInst *OERI) {
  handleSimpleInstr(OERI);
}

void InstructionVisitor::visitInitExistentialMetatypeInst(InitExistentialMetatypeInst *IEMI) {
  handleSimpleInstr(IEMI);
}

void InstructionVisitor::visitOpenExistentialMetatypeInst(OpenExistentialMetatypeInst *OEMI) {
  handleSimpleInstr(OEMI);
}

void InstructionVisitor::visitAllocExistentialBoxInst(AllocExistentialBoxInst *AEBI) {
  handleSimpleInstr(AEBI);
}

void InstructionVisitor::visitProjectExistentialBoxInst(ProjectExistentialBoxInst *PEBI) {
  handleSimpleInstr(PEBI);
}

void InstructionVisitor::visitOpenExistentialBoxInst(OpenExistentialBoxInst *OEBI) {
  handleSimpleInstr(OEBI);
}

void InstructionVisitor::visitOpenExistentialBoxValueInst(OpenExistentialBoxValueInst *OEBVI) {
  handleSimpleInstr(OEBVI);
}

void InstructionVisitor::visitDeallocExistentialBoxInst(DeallocExistentialBoxInst *DEBI) {
  handleSimpleInstr(DEBI);
}

void InstructionVisitor::visitProjectBlockStorageInst(ProjectBlockStorageInst *PBSI) {
  handleSimpleInstr(PBSI);
}

void InstructionVisitor::visitInitBlockStorageHeaderInst(InitBlockStorageHeaderInst *IBSHI) {
  handleSimpleInstr(IBSHI);
  SILValue referencedFunction = IBSHI->getInvokeFunction();
  std::string FuncName = addressToString(referencedFunction.getOpaqueValue());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [INVOKE FUNC]:" << FuncName << "\n";
  }
  ADD_PROP(MAKE_CONST(FuncName.c_str()));
}

/*******************************************************************************/
/*                          Blocks                                             */
/*******************************************************************************/

// Documentation incomplete for this section.

/*******************************************************************************/
/*                          Unchecked Conversions                              */
/*******************************************************************************/

void InstructionVisitor::visitUpcastInst(UpcastInst *UI) {
  handleSimpleInstr(UI);
}

void InstructionVisitor::visitAddressToPointerInst(AddressToPointerInst *ATPI) {
  handleSimpleInstr(ATPI);
}

void InstructionVisitor::visitPointerToAddressInst(PointerToAddressInst *PTAI) {
  handleSimpleInstr(PTAI);
}

void InstructionVisitor::visitUncheckedRefCastInst(UncheckedRefCastInst *URCI) {
  handleSimpleInstr(URCI);
}

void InstructionVisitor::visitUncheckedRefCastAddrInst(UncheckedRefCastAddrInst *URCAI) {
  handleSimpleInstr(URCAI);
}

void InstructionVisitor::visitUncheckedAddrCastInst(UncheckedAddrCastInst *UACI) {
  handleSimpleInstr(UACI);
}

void InstructionVisitor::visitUncheckedTrivialBitCastInst(UncheckedTrivialBitCastInst *BI) {
  handleSimpleInstr(BI);
}

void InstructionVisitor::visitUncheckedBitwiseCase(UncheckedBitwiseCastInst *UBCI) {
  handleSimpleInstr(UBCI);
}

void InstructionVisitor::visitRefToRawPointerInst(RefToRawPointerInst *CI) {
  handleSimpleInstr(CI);
}

void InstructionVisitor::visitRawPointerToRefInst(RawPointerToRefInst *CI) {
  handleSimpleInstr(CI);
}

void InstructionVisitor::visitRefToUnownedInst(RefToUnownedInst *RTUI) {
  handleSimpleInstr(RTUI);
}

void InstructionVisitor::visitUnownedToRefInst(UnownedToRefInst *UTRI) {
  handleSimpleInstr(UTRI);
}

void InstructionVisitor::visitRefToUnmanagedInst(RefToUnmanagedInst *RTUI) {
  handleSimpleInstr(RTUI);
}

void InstructionVisitor::visitUnmanagedToRefInst(UnmanagedToRefInst *CI) {
  handleSimpleInstr(CI);
}

void InstructionVisitor::visitConvertFunctionInst(ConvertFunctionInst *CFI) {
  handleSimpleInstr(CFI);
}

void InstructionVisitor::visitConvertEscapeToNoEscapeInst(ConvertEscapeToNoEscapeInst *CVT) {
  handleSimpleInstr(CVT);
}

void InstructionVisitor::visitThinFunctionToPointerInst(ThinFunctionToPointerInst *TFPI) {
  handleSimpleInstr(TFPI);
}

void InstructionVisitor::visitPointerToThinFunctionInst(PointerToThinFunctionInst *CI) {
  handleSimpleInstr(CI);
}

void InstructionVisitor::visitClassifyBridgeObjectInst(ClassifyBridgeObjectInst *CBOI) {
  handleSimpleInstr(CBOI);
}

void InstructionVisitor::visitValueToBridgeObjectInst(ValueToBridgeObjectInst *CTBOI) {
  handleSimpleInstr(CTBOI);
}

void InstructionVisitor::visitRefToBridgeObjectInst(RefToBridgeObjectInst *RTBOI) {
  handleSimpleInstr(RTBOI);
}

void InstructionVisitor::visitBridgeObjectToRefInst(BridgeObjectToRefInst *BOTRI) {
  handleSimpleInstr(BOTRI);
}

void InstructionVisitor::visitBridgeObjectToWordInst(BridgeObjectToWordInst *BOTWI) {
  handleSimpleInstr(BOTWI);
}

void InstructionVisitor::visitThinToThickFunctionInst(ThinToThickFunctionInst *TTFI) {
  handleSimpleInstr(TTFI);
}

void InstructionVisitor::visitThickToObjCMetatypeInst(ThickToObjCMetatypeInst *TTOMI) {
  handleSimpleInstr(TTOMI);
}

void InstructionVisitor::visitObjCToThickMetatypeInst(ObjCToThickMetatypeInst *OTTMI) {
  handleSimpleInstr(OTTMI);
}

void InstructionVisitor::visitObjCMetatypeToObjectInst(ObjCMetatypeToObjectInst *OMTOI) {
  handleSimpleInstr(OMTOI);
}

void InstructionVisitor::visitObjCExistentialMetatypeToObjectInst(ObjCExistentialMetatypeToObjectInst *OEMTOI) {
  handleSimpleInstr(OEMTOI);
}

void InstructionVisitor::visitUncheckedOwnershipConversionInst(UncheckedOwnershipConversionInst *UOCI) {
  handleSimpleInstr(UOCI);
}

/*******************************************************************************/
/*                          Checked Conversions                                */
/*******************************************************************************/

void InstructionVisitor::visitUnconditionalCheckedCastInst(UnconditionalCheckedCastInst *UCCI) {
  handleSimpleInstr(UCCI);
}

void InstructionVisitor::visitUnconditionalCheckedCastAddrInst(UnconditionalCheckedCastAddrInst *UCCAI) {
  handleSimpleInstr(UCCAI);
}

void InstructionVisitor::visitUnconditionalCheckedCastValueInst(UnconditionalCheckedCastValueInst *UCCVI) {
  handleSimpleInstr(UCCVI);
}

/*******************************************************************************/
/*                          Runtime Failures                                   */
/*******************************************************************************/

void InstructionVisitor::visitCondFailInst(CondFailInst *FI) {
  handleSimpleInstr(FI);
}

/*******************************************************************************/
/*                           Terminators                                       */
/*******************************************************************************/

void InstructionVisitor::visitUnreachableInst(__attribute__((unused)) UnreachableInst *UI) { }

void InstructionVisitor::visitReturnInst(ReturnInst *RI) {
  handleSimpleInstr(RI);
}

void InstructionVisitor::visitThrowInst(ThrowInst *TI) {
  handleSimpleInstr(TI);
}

void InstructionVisitor::visitYieldInst(YieldInst *YI) {
  SILBasicBlock *ResumeBB = YI->getResumeBB();
  SILBasicBlock *UnwindBB = YI->getUnwindBB();
  std::string ResumeLabel = label(ResumeBB);
  std::string UnwindLabel = label(UnwindBB);
  if (SWAN_PRINT) {
    llvm::outs() << "\t [RESUME BB]: " << ResumeLabel << "\n";
    llvm::outs() << "\t [UNWIND BB]: " << UnwindLabel << "\n";
  }
  ADD_PROP(MAKE_CONST(ResumeLabel.c_str()));
  ADD_PROP(MAKE_CONST(UnwindLabel.c_str()));
  list<jobject> yieldValues;
  for (const auto value : YI->getYieldedValues()) {
    std::string valName = addressToString(value.getOpaqueValue());
    if (SWAN_PRINT) {
      llvm::outs() << "\t [YIELD VALUE]: " << valName << "\n";
    }
    yieldValues.push_back(MAKE_CONST(valName.c_str()));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&yieldValues)));
}

void InstructionVisitor::visitUnwindInst(__attribute__((unused)) UnwindInst *UI) { }

void InstructionVisitor::visitBranchInst(BranchInst *BI) {
  std::string DestBranch = label(BI->getDestBB());
  ADD_PROP(MAKE_CONST(DestBranch.c_str()));
  if (SWAN_PRINT) {
    llvm::outs() << "\t [DEST BB]: " << DestBranch << "\n";
  }
  std::list<jobject> Arguments;
  for (unsigned int opIndex = 0; opIndex < BI->getNumArgs(); ++opIndex) {
    std::string OperandName = addressToString(BI->getOperand(opIndex).getOpaqueValue());
    if (SWAN_PRINT) {
      llvm::outs() << "\t\t [OPER NAME]: " << OperandName << "\n";
      llvm::outs() << "\t\t -------\n";
    }
    Arguments.push_back(MAKE_CONST(OperandName.c_str()));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Arguments)));
}

void InstructionVisitor::visitCondBranchInst(CondBranchInst *CBI) {
  std::string CondOperandName = addressToString(CBI->getCondition().getOpaqueValue());
  std::string TrueDestName = label(CBI->getTrueBB());
  std::string FalseDestName = label(CBI->getFalseBB());
  if (SWAN_PRINT) {
    llvm::outs() << "\t [COND NAME]: " << CondOperandName << "\n";
    llvm::outs() << "\t [TRUE DEST BB]: " << TrueDestName << "\n";
    llvm::outs() << "\t [FALSE DEST BB]: " << FalseDestName << "\n";
  }
  ADD_PROP(MAKE_CONST(CondOperandName.c_str()));
  ADD_PROP(MAKE_CONST(TrueDestName.c_str()));
  ADD_PROP(MAKE_CONST(FalseDestName.c_str()));
  std::list<jobject> TrueArguments;
  if (SWAN_PRINT) {
    llvm::outs() << "\t True Args \n";
  }
  for (unsigned int opIndex = 0; opIndex < CBI->getTrueOperands().size(); ++opIndex) {
    std::string OperandName = addressToString(CBI->getTrueOperands()[opIndex].get().getOpaqueValue());
    if (SWAN_PRINT) {
      llvm::outs() << "\t\t [OPER NAME]: " << OperandName << "\n";
      llvm::outs() << "\t\t -------\n";
    }
    TrueArguments.push_back(MAKE_CONST(OperandName.c_str()));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&TrueArguments)));
  std::list<jobject> FalseArguments;
  if (SWAN_PRINT) {
    llvm::outs() << "\t False Args \n";
  }
  for (unsigned int opIndex = 0; opIndex < CBI->getFalseOperands().size(); ++opIndex) {
    std::string OperandName = addressToString(CBI->getFalseOperands()[opIndex].get().getOpaqueValue());
    if (SWAN_PRINT) {
      llvm::outs() << "\t\t [OPER NAME]: " << OperandName << "\n";
      llvm::outs() << "\t\t -------\n";
    }
    FalseArguments.push_back(MAKE_CONST(OperandName.c_str()));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&FalseArguments)));
}

void InstructionVisitor::visitSwitchValueInst(SwitchValueInst *SVI) {
  std::string EnumName = addressToString(SVI->getOperand().getOpaqueValue());
  ADD_PROP(MAKE_CONST(EnumName.c_str()));
  std::list<jobject> Cases;
  for (unsigned int i = 0; i < SVI->getNumCases(); ++i) {
    auto Case = SVI->getCase(i);
    SILValue CaseValue = Case.first;
    SILBasicBlock *CaseBasicBlock = Case.second;
    std::string CaseValueName = addressToString(CaseValue.getOpaqueValue());
    int DestBlock = CaseBasicBlock->getDebugID();
    std::list<jobject> Fields;
    Fields.push_back(MAKE_CONST(CaseValueName.c_str()));
    Fields.push_back(MAKE_CONST(DestBlock));
    Cases.push_back(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Fields)));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Cases)));
  if (SVI->hasDefault()) {
    SILBasicBlock *DefaultBasicBlock = SVI->getDefaultBB();
    int DestBlock = DefaultBasicBlock->getDebugID();
    ADD_PROP(MAKE_CONST(DestBlock));
  }
  
}

void InstructionVisitor::visitSelectValueInst(__attribute__((unused)) SelectValueInst *SVI) { }

void InstructionVisitor::visitSwitchEnumInst(SwitchEnumInst *SWI) {
  std::string EnumName = addressToString(SWI->getOperand().getOpaqueValue());
  ADD_PROP(MAKE_CONST(EnumName.c_str()));
  std::list<jobject> Cases;
  for (unsigned int i = 0; i < SWI->getNumCases(); ++i) {
    auto Case = SWI->getCase(i);
    EnumElementDecl *CaseDecl = Case.first;
    SILBasicBlock *CaseBasicBlock = Case.second;
    std::string CaseName = CaseDecl->getNameStr().str();
    int DestBlock = CaseBasicBlock->getDebugID();
    std::list<jobject> Fields;
    Fields.push_back(MAKE_CONST(CaseName.c_str()));
    Fields.push_back(MAKE_CONST(DestBlock));
    Cases.push_back(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Fields)));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Cases)));
  if (SWI->hasDefault()) {
    SILBasicBlock *DefaultBasicBlock = SWI->getDefaultBB();
    int DestBlock = DefaultBasicBlock->getDebugID();
    ADD_PROP(MAKE_CONST(DestBlock));
  }
}

void InstructionVisitor::visitSwitchEnumAddrInst(SwitchEnumAddrInst *SEAI) {
  std::string EnumName = addressToString(SEAI->getOperand().getOpaqueValue());
  ADD_PROP(MAKE_CONST(EnumName.c_str()));
  std::list<jobject> Cases;
  for (unsigned int i = 0; i < SEAI->getNumCases(); ++i) {
    auto Case = SEAI->getCase(i);
    EnumElementDecl *CaseDecl = Case.first;
    SILBasicBlock *CaseBasicBlock = Case.second;
    std::string CaseName = CaseDecl->getNameStr().str();
    int DestBlock = CaseBasicBlock->getDebugID();
    std::list<jobject> Fields;
    Fields.push_back(MAKE_CONST(CaseName.c_str()));
    Fields.push_back(MAKE_CONST(DestBlock));
    Cases.push_back(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Fields)));
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&Cases)));
  if (SEAI->hasDefault()) {
    SILBasicBlock *DefaultBasicBlock = SEAI->getDefaultBB();
    int DestBlock = DefaultBasicBlock->getDebugID();
    ADD_PROP(MAKE_CONST(DestBlock));
  }
}

void InstructionVisitor::visitDynamicMethodBranchInst(DynamicMethodBranchInst *DMBI)
{
  handleSimpleInstr(DMBI);
  std::string method = DMBI->getMember().getDecl()->getFullName().getBaseIdentifier().str();
  int hasMethodBB = DMBI->getHasMethodBB()->getDebugID();
  int noMethodBB = DMBI->getNoMethodBB()->getDebugID();
  ADD_PROP(MAKE_CONST(method.c_str()));
  ADD_PROP(MAKE_CONST(hasMethodBB));
  ADD_PROP(MAKE_CONST(noMethodBB));
  if (SWAN_PRINT) {
    llvm::outs() << "\t [METHOD]: " << method << "\n";
    llvm::outs() << "\t [HAS METHOD BB: " << hasMethodBB << "\n";
    llvm::outs() << "\t [NO METHOD BB]: " << noMethodBB << "\n";
  }
}

void InstructionVisitor::visitCheckedCastBranchInst(CheckedCastBranchInst *CI) {
  handleSimpleInstr(CI);
  SILBasicBlock *successBlock = CI->getSuccessBB();
  ADD_PROP(MAKE_CONST(successBlock->getDebugID()));
  SILBasicBlock *failureBlock = CI->getFailureBB();
  ADD_PROP(MAKE_CONST(failureBlock->getDebugID()));
}

void InstructionVisitor::visitCheckedCastBranchValueInst(__attribute__((unused)) CheckedCastValueBranchInst CI) { }

void InstructionVisitor::visitCheckedCastAddrBranchInst(CheckedCastAddrBranchInst *CI) {
  handleSimpleInstr(CI);
  SILBasicBlock *successBlock = CI->getSuccessBB();
  ADD_PROP(MAKE_CONST(successBlock->getDebugID()));
  SILBasicBlock *failureBlock = CI->getFailureBB();
  ADD_PROP(MAKE_CONST(failureBlock->getDebugID()));
}

void InstructionVisitor::visitTryApplyInst(TryApplyInst *TAI) {
  ADD_PROP(MAKE_CONST(label(TAI->getNormalBB()).c_str()));
  ADD_PROP(MAKE_CONST(label(TAI->getErrorBB()).c_str()));
  auto *Callee = TAI->getReferencedFunctionOrNull();
  std::list<jobject> arguments;
  for (auto arg : TAI->getArguments()) {
    arguments.push_back(MAKE_CONST(addressToString(arg.getOpaqueValue()).c_str()));
  }
  ADD_PROP(MAKE_CONST(addressToString(TAI->getOperand(0).getOpaqueValue()).c_str()));
  if (SWAN_PRINT) {
    llvm::outs() << "\t [FUNC REF ADDR]: " << TAI->getOperand(0).getOpaqueValue() << "\n";
  }
  if (SWAN_PRINT) {
    for (auto arg : arguments) {
      llvm::outs() << "\t\t [ARG]: " << Instance->CAst->getConstantValue(arg) << "\n";
    }
  }
  ADD_PROP(MAKE_NODE2(CAstWrapper::PRIMITIVE, MAKE_ARRAY(&arguments)));
  if (Callee) {
    auto *FD = Callee->getLocation().getAsASTNode<FuncDecl>();
    if (FD && (FD->isUnaryOperator() || FD->isBinaryOperator())) {
      jobject OperatorNode = MAKE_CONST(FD->getName().str().str().c_str());
      arguments.push_front(OperatorNode);
      if (SWAN_PRINT) {
        llvm::outs() << "\t [OPERATOR NAME]:" << Instance->CAst->getConstantValue(OperatorNode) << "\n";
      }
      if (FD->isUnaryOperator()) {
        ADD_PROP(MAKE_NODE2(CAstWrapper::UNARY_EXPR, MAKE_ARRAY(&arguments)));
      } else if (FD->isBinaryOperator()) {
        ADD_PROP(MAKE_NODE2(CAstWrapper::BINARY_EXPR, MAKE_ARRAY(&arguments)));
      }
    }
  }
}
