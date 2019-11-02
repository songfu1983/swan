//===--- ConditionalBranchInstruction.java -------------------------------===//
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

package ca.maple.swan.swift.translator.silir.instructions;

import ca.maple.swan.swift.translator.silir.BasicBlock;
import ca.maple.swan.swift.translator.silir.context.InstructionContext;
import ca.maple.swan.swift.translator.silir.values.Value;

public class ConditionalBranchInstruction extends SILIRInstruction {

    public final Value conditionValue;

    public final BasicBlock trueBlock;

    public final BasicBlock falseBlock;

    public ConditionalBranchInstruction(String conditionValue, BasicBlock trueBlock, BasicBlock falseBlock, InstructionContext ic) {
        super(ic);
        this.conditionValue = ic.valueTable().getValue(conditionValue);
        this.trueBlock = trueBlock;
        this.falseBlock = falseBlock;
    }

    @Override
    public String toString() {
        return "cond_br " + conditionValue.simpleName() + " true: bb" + trueBlock.getNumber() + ", false: bb" + falseBlock.getNumber() + "\n";
    }

    @Override
    public void visit(ISILIRVisitor v) {
        v.visitConditionalBranchInstruction(this);
    }
}
