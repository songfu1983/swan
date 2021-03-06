//===--- ValueTable.java -------------------------------------------------===//
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

package ca.maple.swan.swift.translator.swanir.values;

import ca.maple.swan.swift.translator.Settings;
import com.ibm.wala.util.debug.Assertions;

import java.util.HashMap;
import java.util.HashSet;

/*
 * Stores values during SIL -> SWANIR translation.
 */

public class ValueTable {

    protected HashMap<String, Value> values;

    protected HashSet<String> usedValues;

    public ValueTable() {
        this.values = new HashMap<>();
        this.usedValues = new HashSet<>();
    }

    // For implicit copies. Useful for reducing IR assignment statement bloat.
    public void copy(String to, String from) {
        if (Settings.ignoreMissingValues() && !has(from)) { values.put(from, new Value(from, "FAKE_VAL")); }
        Assertions.productionAssertion(has(from));
        values.put(to, values.get(from));
        usedValues.add(from);
    }

    public Value getValue(String s) {
        if (Settings.ignoreMissingValues() && !has(s)) { values.put(s, new Value(s, "FAKE_VAL")); }
        Assertions.productionAssertion(has(s));
        // We need to know if aliases show up where they are not expected.
        Assertions.productionAssertion(!(values.get(s) instanceof FieldAliasValue) && !(values.get(s) instanceof ArrayIndexAliasValue));
        usedValues.add(s);
        return values.get(s);
    }

    public Value getPossibleAlias(String s) {
        if (Settings.ignoreMissingValues() && !has(s)) { values.put(s, new Value(s, "FAKE_VAL")); }
        Assertions.productionAssertion(has(s));
        usedValues.add(s);
        return values.get(s);
    }

    public void replace(Value v1, Value v2) {
        this.values.remove(v1.name);
        this.values.put(v2.name, v2);
    }

    public void arrayAlias(String resultName, String resultType, String operandName, int index) {
        Value operandValue = this.getValue(operandName);
        Value resultValue = new ArrayIndexAliasValue(resultName, resultType, operandValue, index);
        this.add(resultValue);
    }

    public void fieldAlias(String resultName, String resultType, String operandName, String operandField) {
        Value operandValue = this.getValue(operandName);
        Value resultValue = new FieldAliasValue(resultName, resultType, operandValue, operandField);
        this.add(resultValue);

    }

    public void setUnused(String s) {
        this.usedValues.remove(s);
    }

    public boolean isUsed(String s) {
        return this.usedValues.contains(s);
    }

    public void add(Value v) {
        values.put(v.name, v);
    }

    public void add(String s, Value v) {
        values.put(s, v);
    }

    public boolean has(Value v) {
        return values.containsKey(v.name);
    }

    public boolean has(String s) {
        return values.containsKey(s);
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("\n");
        for (String key : values.keySet()) {
            sb.append(values.get(key).name);
            sb.append(" : ");
            sb.append(values.get(key).type);
            sb.append("\n");
        }
        return sb.toString();
    }
}
