//===--- FunctionEntity.java ---------------------------------------------===//
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

package ca.maple.swan.swift.tree;

import com.ibm.wala.cast.ir.translator.AbstractCodeEntity;
import com.ibm.wala.cast.tree.*;
import com.ibm.wala.cast.tree.impl.LineNumberPosition;

import java.util.ArrayList;
import java.util.Collection;

/*
 * This class represents a translated SIL function.
 */

public class FunctionEntity extends AbstractCodeEntity {

    private String functionName;
    private final String[] arguments;
    private CAstSourcePositionMap.Position namePosition;
    private ArrayList<CAstSourcePositionMap.Position> argumentPositions;
    // rawInfo holds the raw AST node representing the function
    // generated by the C++ side.
    public final CAstNode rawInfo;
    public boolean artificial = false;

    public FunctionEntity(String name, String returnType,
                          ArrayList<String> argumentTypes,
                          ArrayList<String> argumentNames,
                          CAstSourcePositionMap.Position functionPosition,
                          ArrayList<CAstSourcePositionMap.Position> argumentPositions) {
        this(name, returnType, argumentTypes, argumentNames, functionPosition, argumentPositions, null);
        this.artificial = true;
    }

    public FunctionEntity(String name, String returnType,
                          ArrayList<String> argumentTypes,
                          ArrayList<String> argumentNames,
                          CAstSourcePositionMap.Position functionPosition,
                          ArrayList<CAstSourcePositionMap.Position> argumentPositions,
                          CAstNode rawInfo) {
        super(new SwiftFunctionType(returnType, argumentTypes));
        assert((argumentNames.size() == argumentTypes.size()) && (argumentTypes.size() == argumentPositions.size()))
                : "Function: " + name + " :argument information is not parallel";
        this.functionName = name;
        this.arguments = argumentNames.toArray(new String[0]);
        this.argumentPositions = argumentPositions;
        this.setPosition(functionPosition);
        // SILLocation/SILFunction doesn't have a way of getting the columns of the function name (AFAIK), so we just
        // grab the first line of the function.
        this.namePosition = functionPosition != null ? new LineNumberPosition(functionPosition.getURL(), functionPosition.getURL(), functionPosition.getFirstLine()) : null;
        this.rawInfo = rawInfo;
    }

    @Override
    public int getKind() {
        return CAstEntity.FUNCTION_ENTITY;
    }

    @Override
    public String getName() {
        return this.functionName;
    }

    @Override
    public String[] getArgumentNames() {
        return this.arguments;
    }

    @Override
    public CAstNode[] getArgumentDefaults() {
        return new CAstNode[0];
    }

    @Override
    public int getArgumentCount() {
        return this.arguments.length;
    }

    @Override
    public CAstSourcePositionMap.Position getNamePosition() {
        return this.namePosition;
    }

    @Override
    public CAstSourcePositionMap.Position getPosition(int i) {
        return this.argumentPositions.get(i);
    }

    @Override
    public Collection<CAstQualifier> getQualifiers() {
        return null;
    }

    @Override
    public String toString() {
        return getName();
    }

}
