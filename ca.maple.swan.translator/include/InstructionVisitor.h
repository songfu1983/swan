//===--- SILWalaInstructionVisitor.h - SIL to CAst Translator ------------===//
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
/// This file defines the SILWalaInstructionVisitor class, which inherits
/// the SILInstructionVisitor template class (part of the Swift compiler).
/// The SILInstructionVisitor translates a given SIL (Swift Intermediate
/// Language) Module to CAst (WALA IR).
///
//===---------------------------------------------------------------------===//

#ifndef SWAN_INSTRUCTIONVISITOR_H
#define SWAN_INSTRUCTIONVISITOR_H

#include "WALAInstance.h"
#include "InfoStructures.hpp"
#include "swift/SIL/ApplySite.h"
#include "swift/SIL/SILVisitor.h"
#include <jni.h>
#include <list>
#include <fstream>

using namespace swift;

namespace swan {

  class WALAInstance;

  // NOTE: Printing using llvm::outs() is highly unreliable when using a JVM.
  // Often, the output will not be complete.
  // A possible solution is to burn CPU cycles so that the llvm printer can catch up.

  // TODO: Move these to env variables or something.

  /// Decides whether to print the translation debug info to terminal at runtime.
  static bool SWAN_PRINT = false;
  /// Source information can be annoying/unnecessary for debugging, so there is an option to disable it.
  static bool SWAN_PRINT_SOURCE = false;
  /// Disable printing memory and file information.
  static bool SWAN_PRINT_FILE_AND_MEMORY = false;
  /// Toggles printing the SIL for each function.
  static bool PRINT_SIL = false;

  /// This class translates SIL to CAst by using Swift's SILInstructionVisitor which has callbacks, including
  /// ones for every type of SILInstruction. This makes translating simple.
  class InstructionVisitor : public SILInstructionVisitor<InstructionVisitor, void> {
  public:
    InstructionVisitor(WALAInstance *Instance) : Instance(Instance) { }

    /// Visit the SILModule of the swift file that holds the SILFunctions.
    void visitSILModule(SILModule *M);

    /// If we do not have a callback handler implemented for a SILInstruction, it will fire this method.
    void visitSILInstruction(SILInstruction *I) {
      llvm::outs() << "Instruction not handled: \n\t" << *I << "\n";
    }

  private:
    /// Visit the SILFunction that holds the SILBasicBlocks.
    void visitSILFunction(SILFunction *F);
    /// Visit the SILBasicBlock that holds the SILInstructions.
    void visitSILBasicBlock(SILBasicBlock *BB);
    /// Prints the SILInstructionInfo
    void printSILInstructionInfo();

    /// The WALAInstance that holds the resultant CAst.
    WALAInstance *Instance;

    /// Current instruction number.
    unsigned InstructionCounter = 0;

    /// Raw data of current instruction begin translated.
    std::unique_ptr<RootInstructionInfo> currentInstruction;

    /// Raw data of current basic block.
    std::unique_ptr<RootBasicBlockInfo> currentBasicBlock;

    /// Current 'Cast Entity' which contains info necessary to later create the CAstEntity for the current function.
    std::unique_ptr<RootFunctionInfo> currentFunction;

    /// Source information about the SILInstruction.
    std::unique_ptr<SILInstructionInfo> instrInfo;
    /// Source information about the SILFunction.
    std::unique_ptr<SILFunctionInfo> functionInfo;
    /// Source information about the SILModule.
    std::unique_ptr<SILModuleInfo> moduleInfo;


    /// Witness table information for dynamic dispatch.
    /// Key is the protocol name, value is a vector of strings representing all functions.
    std::unordered_map<string, std::vector<string>> witnessTable;

    /// Handles any instruction with nothing but regular operands and results.
    /// Can also be used on complex instructions, but isn't garuanteed to contain
    /// all relevent information about that instructions.
    void handleSimpleInstr(SILInstruction *UIB);

  public:

    // SIL INSTRUCTION VISITOR CALLBACKS (TRANSLATE EACH INSTRUCTION TO CAST NODE)

    /// Before visiting the SILInstruction, this callback is fired and here we set up the source info of the instruction.
    void beforeVisit(SILInstruction *I);

    /*******************************************************************************/
    /*                      Allocation and Deallocation                            */
    /*******************************************************************************/

    void visitAllocStackInst(AllocStackInst *ASI);
    void visitAllocRefInst(AllocRefInst *ARI);
    void visitAllocRefDynamicInst(AllocRefDynamicInst *ARDI);
    void visitAllocBoxInst(AllocBoxInst *ABI);
    void visitAllocValueBufferInst(AllocValueBufferInst *AVBI);
    void visitAllocGlobalInst(AllocGlobalInst *AGI);
    void visitDeallocStackInst(DeallocStackInst *DSI);
    void visitDeallocBoxInst(DeallocBoxInst *DBI);
    void visitProjectBoxInst(ProjectBoxInst *PBI);
    void visitDeallocRefInst(DeallocRefInst *DRI);
    void visitDeallocPartialRefInst(DeallocPartialRefInst *DPRI);
    void visitDeallocValueBufferInst(DeallocValueBufferInst *DVBI);
    void visitProjectValueBufferInst(ProjectValueBufferInst *PVBI);

    /*******************************************************************************/
    /*                        Debug Information                                    */
    /*******************************************************************************/

    void visitDebugValueInst(DebugValueInst *DBI);
    void visitDebugValueAddrInst(DebugValueAddrInst *DVAI);

    /*******************************************************************************/
    /*                        Accessing Memory                                     */
    /*******************************************************************************/

    void visitLoadInst(LoadInst *LI);
    void visitStoreInst(StoreInst *SI);
    void visitStoreBorrowInst(StoreBorrowInst *SBI);
    void visitLoadBorrowInst(LoadBorrowInst *LBI);
    void visitBeginBorrowInst(BeginBorrowInst *BBI);
    void visitEndBorrowInst(EndBorrowInst *EBI);
    void visitAssignInst(AssignInst *AI);
    void visitAssignByWrapperInst(AssignByWrapperInst *ABWI);
    void visitMarkUninitializedInst(MarkUninitializedInst *MUI);
    void visitMarkFunctionEscapeInst(MarkFunctionEscapeInst *MFEI);
    void visitCopyAddrInst(CopyAddrInst *CAI);
    void visitDestroyAddrInst(DestroyAddrInst *DAI);
    void visitIndexAddrInst(IndexAddrInst *IAI);
    void visitTailAddrInst(TailAddrInst *TAI);
    void visitIndexRawPointerInst(IndexRawPointerInst *IRPI);
    void visitBindMemoryInst(BindMemoryInst *BMI);
    void visitBeginAccessInst(BeginAccessInst *BAI);
    void visitEndAccessInst(EndAccessInst *EAI);
    void visitBeginUnpairedAccessInst(BeginUnpairedAccessInst *BUI);
    void visitEndUnpairedAccessInst(EndUnpairedAccessInst *EUAI);

    /*******************************************************************************/
    /*                        Reference Counting                                   */
    /*******************************************************************************/

    void visitStrongRetainInst(StrongRetainInst *SRTI);
    void visitStrongReleaseInst(StrongReleaseInst *SRLI);
    void visitSetDeallocatingInst(SetDeallocatingInst *SDI);
    void visitCopyUnownedValueInst(CopyUnownedValueInst *CUVI);
    void visitStrongRetainUnownedInst(StrongRetainUnownedInst *SRUI);
    void visitUnownedRetainInst(UnownedRetainInst *URTI);
    void visitUnownedReleaseInst(UnownedReleaseInst *URLI);
    void visitLoadWeakInst(LoadWeakInst *LWI);
    void visitStoreWeakInst(StoreWeakInst *SWI);
    void visitLoadUnownedInst(LoadUnownedInst *LUI);
    void visitStoreUnownedInst(StoreUnownedInst *SUI);
    void visitFixLifetimeInst(FixLifetimeInst *FLI);
    void visitEndLifetimeInst(EndLifetimeInst *ELI);
    void visitMarkDependenceInst(MarkDependenceInst *MDI);
    void visitIsUniqueInst(IsUniqueInst *IUI);
    void visitIsEscapingClosureInst(IsEscapingClosureInst *IECI);
    void visitCopyBlockInst(CopyBlockInst *CBI);
    void visitCopyBlockWithoutEscapingInst(CopyBlockWithoutEscapingInst *CBWEI);
    // TODO? builtin "unsafeGuaranteed" & builtin "unsafeGuaranteedEnd"

    /*******************************************************************************/
    /*                         Literals                                            */
    /*******************************************************************************/

    void visitFunctionRefInst(FunctionRefInst *FRI);
    void visitDynamicFunctionRefInst(DynamicFunctionRefInst *DFRI);
    void visitPreviousDynamicFunctionRefInst(PreviousDynamicFunctionRefInst *PDFRI);
    void visitGlobalAddrInst(GlobalAddrInst *GAI);
    void visitGlobalValueInst(GlobalValueInst *GVI);
    void visitIntegerLiteralInst(IntegerLiteralInst *ILI);
    void visitFloatLiteralInst(FloatLiteralInst *FLI);
    void visitStringLiteralInst(StringLiteralInst *SLI);

    /*******************************************************************************/
    /*                         Dynamic Dispatch                                    */
    /*******************************************************************************/

    void visitClassMethodInst(ClassMethodInst *CMI);
    void visitObjCMethodInst(ObjCMethodInst *AMI);
    void visitSuperMethodInst(SuperMethodInst *SMI);
    void visitObjCSuperMethodInst(ObjCSuperMethodInst *ASMI);
    void visitWitnessMethodInst(WitnessMethodInst *WMI);

    /*******************************************************************************/
    /*                         Function Application                                */
    /*******************************************************************************/

    void visitApplyInst(ApplyInst *AI);
    void visitBeginApplyInst(BeginApplyInst *BAI);
    void visitAbortApplyInst(AbortApplyInst *AAI);
    void visitEndApplyInst(EndApplyInst *EAI);
    void visitPartialApplyInst(PartialApplyInst *PAI);
    void visitBuiltinInst(BuiltinInst *BI);

    /*******************************************************************************/
    /*                          Metatypes                                          */
    /*******************************************************************************/

    void visitMetatypeInst(MetatypeInst *MI);
    void visitValueMetatypeInst(ValueMetatypeInst *VMI);
    void visitExistentialMetatypeInst(ExistentialMetatypeInst *EMI);
    void visitObjCProtocolInst(ObjCProtocolInst *OPI);

    /*******************************************************************************/
    /*                          Aggregate Types                                    */
    /*******************************************************************************/

    void visitRetainValueInst(RetainValueInst *RVI);
    void visitRetainValueAddrInst(RetainValueAddrInst *RVAI);
    void visitUnmanagedRetainValueInst(UnmanagedRetainValueInst *URVI);
    void visitCopyValueInst(CopyValueInst *CVI);
    void visitReleaseValueInst(ReleaseValueInst *REVI);
    void visitReleaseValueAddrInst(ReleaseValueAddrInst *REVAI);
    void visitUnmanagedReleaseValueInst(UnmanagedReleaseValueInst *UREVI);
    void visitDestroyValueInst(DestroyValueInst *DVI);
    void visitAutoreleaseValueInst(AutoreleaseValueInst *AREVI);
    void visitTupleInst(TupleInst *TI);
    void visitTupleExtractInst(TupleExtractInst *TEI);
    void visitTupleElementAddrInst(TupleElementAddrInst *TEAI);
    void visitDestructureTupleInst(DestructureTupleInst *DTI);
    void visitStructInst(StructInst *SI);
    void visitStructExtractInst(StructExtractInst *SEI);
    void visitStructElementAddrInst(StructElementAddrInst *SEAI);
    void visitDestructureStructInst(DestructureStructInst *DSI);
    void visitObjectInst(ObjectInst *OI);
    void visitRefElementAddrInst(RefElementAddrInst *REAI);
    void visitRefTailAddrInst(RefTailAddrInst *RTAI);

    /*******************************************************************************/
    /*                          Enums                                              */
    /*******************************************************************************/

    void visitEnumInst(EnumInst *EI);
    void visitUncheckedEnumDataInst(UncheckedEnumDataInst *UEDI);
    void visitInitEnumDataAddrInst(InitEnumDataAddrInst *UDAI);
    void visitInjectEnumAddrInst(InjectEnumAddrInst *IUAI);
    void visitUncheckedTakeEnumDataAddrInst(UncheckedTakeEnumDataAddrInst *UDAI);
    void visitSelectEnumInst(SelectEnumInst *SEI);
    void visitSelectEnumAddrInst(SelectEnumAddrInst *SEAI);

    /*******************************************************************************/
    /*                          Protocol and Protocol Composition Types            */
    /*******************************************************************************/

    void visitInitExistentialAddrInst(InitExistentialAddrInst *IEAI);
    void visitInitExistentialValueInst(InitExistentialValueInst *IEVI);
    void visitDeinitExistentialAddrInst(DeinitExistentialAddrInst *DEAI);
    void visitDeinitExistentialValueInst(DeinitExistentialValueInst *DEVI);
    void visitOpenExistentialAddrInst(OpenExistentialAddrInst *OEAI);
    void visitOpenExistentialValueInst(OpenExistentialValueInst *OEVI);
    void visitInitExistentialRefInst(InitExistentialRefInst *IERI);
    void visitOpenExistentialRefInst(OpenExistentialRefInst *OERI);
    void visitInitExistentialMetatypeInst(InitExistentialMetatypeInst *IEMI);
    void visitOpenExistentialMetatypeInst(OpenExistentialMetatypeInst *OEMI);
    void visitAllocExistentialBoxInst(AllocExistentialBoxInst *AEBI);
    void visitProjectExistentialBoxInst(ProjectExistentialBoxInst *PEBI);
    void visitOpenExistentialBoxInst(OpenExistentialBoxInst *OEBI);
    void visitOpenExistentialBoxValueInst(OpenExistentialBoxValueInst *OEBVI);
    void visitDeallocExistentialBoxInst(DeallocExistentialBoxInst *DEBI);

    /*******************************************************************************/
    /*                          Blocks                                             */
    /*******************************************************************************/

    void visitProjectBlockStorageInst(ProjectBlockStorageInst *PBSI);
    void visitInitBlockStorageHeaderInst(InitBlockStorageHeaderInst *IBSHI);

    /*******************************************************************************/
    /*                          Unchecked Conversions                              */
    /*******************************************************************************/

    void visitUpcastInst(UpcastInst *UI);
    void visitAddressToPointerInst(AddressToPointerInst *ATPI);
    void visitPointerToAddressInst(PointerToAddressInst *PTAI);
    void visitUncheckedRefCastInst(UncheckedRefCastInst *URCI);
    void visitUncheckedRefCastAddrInst(UncheckedRefCastAddrInst *URCAI);
    void visitUncheckedAddrCastInst(UncheckedAddrCastInst *UACI);
    void visitUncheckedTrivialBitCastInst(UncheckedTrivialBitCastInst *BI);
    void visitUncheckedBitwiseCase(UncheckedBitwiseCastInst *UBCI);
    void visitRefToRawPointerInst(RefToRawPointerInst *CI);
    void visitRawPointerToRefInst(RawPointerToRefInst *CI);
    void visitRefToUnownedInst(RefToUnownedInst *RTUI);
    void visitUnownedToRefInst(UnownedToRefInst *UTRI);
    void visitRefToUnmanagedInst(RefToUnmanagedInst *RTUI);
    void visitUnmanagedToRefInst(UnmanagedToRefInst *UTRI);
    void visitConvertFunctionInst(ConvertFunctionInst *CFI);
    void visitConvertEscapeToNoEscapeInst(ConvertEscapeToNoEscapeInst *CETNEI);
    void visitThinFunctionToPointerInst(ThinFunctionToPointerInst *TFPI);
    void visitPointerToThinFunctionInst(PointerToThinFunctionInst *CI);
    void visitClassifyBridgeObjectInst(ClassifyBridgeObjectInst *CBOI);
    void visitValueToBridgeObjectInst(ValueToBridgeObjectInst *CTBOI);
    void visitRefToBridgeObjectInst(RefToBridgeObjectInst *RTBOI);
    void visitBridgeObjectToRefInst(BridgeObjectToRefInst *BOTRI);
    void visitBridgeObjectToWordInst(BridgeObjectToWordInst *BOTWI);
    void visitThinToThickFunctionInst(ThinToThickFunctionInst *TTFI);
    void visitThickToObjCMetatypeInst(ThickToObjCMetatypeInst *TTOMI);
    void visitObjCToThickMetatypeInst(ObjCToThickMetatypeInst *OTTMI);
    void visitObjCMetatypeToObjectInst(ObjCMetatypeToObjectInst *OMTOI);
    void visitObjCExistentialMetatypeToObjectInst(ObjCExistentialMetatypeToObjectInst *OEMTOI);

    // Not in documentation, but will leave here.
    void visitUncheckedOwnershipConversionInst(UncheckedOwnershipConversionInst *UOCI);

    /*******************************************************************************/
    /*                          Checked Conversions                                */
    /*******************************************************************************/

    void visitUnconditionalCheckedCastInst(UnconditionalCheckedCastInst *UCCI);
    void visitUnconditionalCheckedCastAddrInst(UnconditionalCheckedCastAddrInst *UCCAI);
    void visitUnconditionalCheckedCastValueInst(UnconditionalCheckedCastValueInst *UCCVI);

    /*******************************************************************************/
    /*                          Runtime Failures                                   */
    /*******************************************************************************/

    void visitCondFailInst(CondFailInst *FI);

    /*******************************************************************************/
    /*                           Terminators                                       */
    /*******************************************************************************/

    void visitUnreachableInst(UnreachableInst *UI);
    void visitReturnInst(ReturnInst *RI);
    void visitThrowInst(ThrowInst *TI);
    void visitYieldInst(YieldInst *YI);
    void visitUnwindInst(UnwindInst *UI);
    void visitBranchInst(BranchInst *BI);
    void visitCondBranchInst(CondBranchInst *CBI);
    void visitSwitchValueInst(SwitchValueInst *SVI);
    void visitSelectValueInst(SelectValueInst *SVI);
    void visitSwitchEnumInst(SwitchEnumInst *SWI);
    void visitSwitchEnumAddrInst(SwitchEnumAddrInst *SEAI);
    void visitDynamicMethodBranchInst(DynamicMethodBranchInst *DMBI);
    void visitCheckedCastBranchInst(CheckedCastBranchInst *CI);
    void visitCheckedCastBranchValueInst(CheckedCastValueBranchInst CI);
    void visitCheckedCastAddrBranchInst(CheckedCastAddrBranchInst *CI);
    void visitTryApplyInst(TryApplyInst *TAI);

  };

} // end swan namespace

#endif // SWAN_INSTRUCTIONVISITOR_H
