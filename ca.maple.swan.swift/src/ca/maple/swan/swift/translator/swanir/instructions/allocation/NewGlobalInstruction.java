//===--- NewGlobalInstruction.java ---------------------------------------===//
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

package ca.maple.swan.swift.translator.swanir.instructions.allocation;

import ca.maple.swan.swift.translator.swanir.context.InstructionContext;
import ca.maple.swan.swift.translator.swanir.instructions.ISWANIRVisitor;
import ca.maple.swan.swift.translator.swanir.instructions.SWANIRInstruction;
import ca.maple.swan.swift.translator.swanir.values.Value;

public class NewGlobalInstruction extends SWANIRInstruction {

    public final Value value;

    public NewGlobalInstruction(String name, String type, InstructionContext ic) {
        super(ic);
        this.value = new Value(name, type);
        ic.globalValueTable().add(value);
    }

    @Override
    public void visit(ISWANIRVisitor v) {
        v.visitNewGlobalInstruction(this);
    }

    @Override
    public String toString() {
        return "new global " + value.name + this.getComment();
    }
}
