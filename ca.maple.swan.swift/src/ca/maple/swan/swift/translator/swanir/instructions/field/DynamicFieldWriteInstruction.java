//===--- DynamicFieldWriteInstruction.java -------------------------------===//
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

package ca.maple.swan.swift.translator.swanir.instructions.field;

import ca.maple.swan.swift.translator.swanir.context.InstructionContext;
import ca.maple.swan.swift.translator.swanir.instructions.ISWANIRVisitor;
import ca.maple.swan.swift.translator.swanir.instructions.SWANIRInstruction;
import ca.maple.swan.swift.translator.swanir.values.Value;

public class DynamicFieldWriteInstruction extends SWANIRInstruction {

    public final Value writeTo;

    public final Value field;

    public final Value operand;

    public DynamicFieldWriteInstruction(String writeTo, String field, String operand, InstructionContext ic) {
        super(ic);
        this.writeTo = ic.valueTable().getValue(writeTo);
        this.operand = ic.valueTable().getValue(operand);
        this.field = ic.valueTable().getValue(field);
    }

    @Override
    public void visit(ISWANIRVisitor v) {
        v.visitDynamicFieldWriteInstruction(this);
    }

    @Override
    public String toString() {
        return writeTo.simpleName() + "." + field.simpleName() +
                " := " + operand.simpleName() + this.getComment();
    }
}
