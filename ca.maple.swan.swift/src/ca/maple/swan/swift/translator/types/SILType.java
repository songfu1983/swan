//===--- SILType.java ----------------------------------------------------===//
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

package ca.maple.swan.swift.translator.types;

import com.ibm.wala.cast.tree.CAstType;

import java.util.Collection;

/*
 * Represents a type in SIL.
 */

public class SILType implements CAstType {

    private final String Name;

    SILType(String name) {
        this.Name = name;
    }

    @Override
    public String getName() {
        return Name;
    }

    @Override
    public Collection<CAstType> getSupertypes() {
        return null;
    }
}
