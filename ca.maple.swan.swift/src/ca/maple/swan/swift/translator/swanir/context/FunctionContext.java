//===--- FunctionContext.java --------------------------------------------===//
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

package ca.maple.swan.swift.translator.swanir.context;

import ca.maple.swan.swift.translator.swanir.Function;
import ca.maple.swan.swift.translator.swanir.values.Argument;
import ca.maple.swan.swift.translator.swanir.values.Value;

import java.util.HashMap;

/*
 * Holds anything a function would need for translation.
 */

public class FunctionContext {

    public Function function;

    public ProgramContext pc;

    public HashMap<String, FunctionContext> coroutines;

    public CoroutineContext cc = null;

    public FunctionContext(Function f, ProgramContext pc) {
        this.function = f;
        this.pc  = pc;
        this.coroutines = new HashMap<>();
        for (Argument a : f.getArguments()) {
           pc.vt.add(a.name, new Value(a.name, a.type));
        }
    }

    public Function getFunction() {
        return this.function;
    }
}
