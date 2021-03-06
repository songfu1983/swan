//===--- SILTypes.java ---------------------------------------------------===//
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

import java.util.HashMap;
import java.util.Set;

/*
 * Used for dynamically creating and saving SIL types. Since there
 * are so many of them, these aren't generally hardcoded.
 */

public class SILTypes {

    private static final HashMap<String, SILType> types = new HashMap<>();

    public static SILType getType(String name) {
        if (!types.containsKey(name)) {
            types.put(name, new SILType(name));
        }
        return types.get(name);
    }

    public static Set<String> getTypeNames() {
        return types.keySet();
    }

}
