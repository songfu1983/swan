//===--- BuiltinHandler.java ---------------------------------------------===//
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

package ca.maple.swan.swift.translator.swanir.summaries;

import ca.maple.swan.swift.translator.swanir.context.InstructionContext;
import ca.maple.swan.swift.translator.swanir.instructions.*;
import ca.maple.swan.swift.translator.swanir.instructions.basic.LiteralInstruction;
import ca.maple.swan.swift.translator.swanir.instructions.allocation.NewInstruction;
import ca.maple.swan.swift.translator.swanir.instructions.array.StaticArrayReadInstruction;
import ca.maple.swan.swift.translator.swanir.instructions.field.DynamicFieldReadInstruction;
import ca.maple.swan.swift.translator.swanir.instructions.field.DynamicFieldWriteInstruction;
import ca.maple.swan.swift.translator.swanir.instructions.field.StaticFieldReadInstruction;
import ca.maple.swan.swift.translator.swanir.instructions.field.StaticFieldWriteInstruction;
import ca.maple.swan.swift.translator.swanir.values.LiteralValue;
import ca.maple.swan.swift.translator.swanir.values.Value;
import com.ibm.wala.util.debug.Assertions;

import java.util.ArrayList;
import java.util.UUID;

/*
 * Builtin handler for SWANIR.
 * !!! BLOW AWAY WHEN EVERYTHING IS REWRITTEN IN SWANIR SUMMARIES.
 */

public class BuiltinHandler {

    public static SWANIRInstruction findSummary(String funcName, String resultName, String resultType, ArrayList<String> params, InstructionContext C) {

        // TODO: Proper string interpolation support.
        // TODO: Handle all Array, Set, and Dictionary container type functions.
        // TODO: os_log needs to have all values of given array sunk.

        // TODO:
        /*
        func $() `Swift.Array.append(__owned A) -> ()`(v0 : $*String, v1 : $*Array<String>)
        func $Array<Int> `Swift.Array.init() -> Swift.Array<A>`(v0 : $@thin Array<Int>.Type)
        func $Array<Int> `Swift.Array.init(repeating: A, count: Swift.Int) -> Swift.Array<A>`(v0 : $*Int, v1 : $Builtin.IntLiteral, v2 : $@thin Array<Int>.Type)
        func $Array<Int> `Swift.Array.init<A where A == A1.Element, A1: Swift.Sequence>(A1) -> Swift.Array<A>`(v0 : $*ClosedRange<Int>, v1 : $@thin Array<Int>.Type)
        func $Bool `(extension in Swift):Swift.Collection.isEmpty.getter : Swift.Bool`(v0 : $*Array<Int>)
        func $Int `Swift.Array.count.getter : Swift.Int`(v0 : $Array<Int>)
        func $Int `Swift.Array.capacity.getter : Swift.Int`(v0 : $Array<String>)
        func $() `(extension in Swift):Swift.Collection.first.getter : Swift.Optional<A.Element>`(v0 : $*Optional<String>, v1 : $*Array<String>)
        func $() `(extension in Swift):Swift.BidirectionalCollection.last.getter : Swift.Optional<A.Element>`(v0 : $*Optional<String>, v1 : $*Array<String>)
        func $() `(extension in Swift):Swift.MutableCollection.subscript.getter : <A where A1: Swift.RangeExpression, A.Index == A1.Bound>(A1) -> A.SubSequence`(v0 : $*ArraySlice<Int>, v1 : $*ClosedRange<Int>, v2 : $*Array<Int>)
        func $Int `Swift.Array.endIndex.getter : Swift.Int`(v0 : $Array<String>)
        func $ArraySlice<String> `Swift.Array.subscript.getter : (Swift.Range<Swift.Int>) -> Swift.ArraySlice<A>`(v0 : $Range<Int>, v1 : $Array<String>)
        func $() `(extension in Swift):Swift.Collection< where A.Element: Swift.Equatable>.firstIndex(of: A.Element) -> Swift.Optional<A.Index>`(v0 : $*Optional<Int>, v1 : $*String, v2 : $*Array<String>)
        func $() `Swift._diagnoseUnexpectedNilOptional(_filenameStart: Builtin.RawPointer, _filenameLength: Builtin.Word, _filenameIsASCII: Builtin.Int1, _line: Builtin.Word, _isImplicitUnwrap: Builtin.Int1) -> ()`(v0 : $Builtin.RawPointer, v1 : $Builtin.Word, v2 : $Builtin.Int1, v3 : $Builtin.Word, v4 : $Builtin.Int1)
        func $() `Swift.Array.insert(_: __owned A, at: Swift.Int) -> ()`(v0 : $*Int, v1 : $Builtin.IntLiteral, v2 : ${ var Array<Int> })
        func $() `(extension in Swift):Swift.RangeReplaceableCollection.insert<A where A1: Swift.Collection, A.Element == A1.Element>(contentsOf: __owned A1, at: A.Index) -> ()`(v0 : $*ClosedRange<Int>, v1 : $*Int, v2 : ${ var Array<Int> })
        func $() `Swift.repeatElement<A>(_: A, count: Swift.Int) -> Swift.Repeated<A>`(v0 : $*Repeated<Int>, v1 : $*Int, v2 : $Builtin.IntLiteral)
        func $() `(extension in Swift):Swift.RangeReplaceableCollection.replaceSubrange<A, B where A1: Swift.Collection, B1: Swift.RangeExpression, A.Element == A1.Element, A.Index == B1.Bound>(_: B1, with: __owned A1) -> ()`(v0 : $*ClosedRange<Int>, v1 : $*Repeated<Int>, v2 : ${ var Array<Int> })
        func $() `Swift.Array.reserveCapacity(Swift.Int) -> ()`(v0 : $Builtin.IntLiteral, v1 : ${ var Array<Int> })
        func $() `Swift.Array.append<A where A == A1.Element, A1: Swift.Sequence>(contentsOf: __owned A1) -> ()`(v0 : $*ClosedRange<Int>, v1 : ${ var Array<Int> })
        func $() `Swift.Array.remove(at: Swift.Int) -> A`(v0 : $*Int, v1 : $Builtin.IntLiteral, v2 : ${ var Array<Int> })
        func $() `(extension in Swift):Swift.RangeReplaceableCollection.removeFirst() -> A.Element`(v0 : $*Int, v1 : ${ var Array<Int> })
        func $() `(extension in Swift):Swift.RangeReplaceableCollection.removeFirst(Swift.Int) -> ()`(v0 : $Builtin.IntLiteral, v1 : ${ var Array<Int> })
        func $() `(extension in Swift):Swift.RangeReplaceableCollection< where A: Swift.BidirectionalCollection>.removeLast() -> A.Swift.Sequence.Element`(v0 : $*Int, v1 : ${ var Array<Int> })
        func $() `(extension in Swift):Swift.RangeReplaceableCollection< where A: Swift.BidirectionalCollection>.removeLast(Swift.Int) -> ()`(v0 : $Builtin.IntLiteral, v1 : ${ var Array<Int> })
        func $() `(extension in Swift):Swift.RangeReplaceableCollection.removeSubrange<A where A1: Swift.RangeExpression, A.Index == A1.Bound>(A1) -> ()`(v0 : $*ClosedRange<Int>, v1 : ${ var Array<Int> })
        func $() `(extension in Swift):Swift.RangeReplaceableCollection< where A: Swift.BidirectionalCollection>.popLast() -> Swift.Optional<A.Swift.Sequence.Element>`(v0 : $*Optional<Int>, v1 : ${ var Array<Int> })
        func $Bool `default argument 0 of Swift.Array.removeAll(keepingCapacity: Swift.Bool) -> ()`()
        func $() `Swift.Array.removeAll(keepingCapacity: Swift.Bool) -> ()`(v0 : $Bool, v1 : ${ var Array<Int> })
        */

        switch(funcName) {


            case "default argument 0 of (extension in Swift):Swift.BidirectionalCollection< where A.Element == Swift.String>.joined(separator: Swift.String) -> Swift.String": {
                return new LiteralInstruction("", resultName, resultName, C);
            }

            case "(extension in Swift):Swift.BidirectionalCollection< where A.Element == Swift.String>.joined(separator: Swift.String) -> Swift.String": {
                // TODO: here we need to add all elements of the array together and return it.
                // This is temporary.
                return new StaticFieldReadInstruction(resultName, resultType, params.get(1), "value", C);
            }



            case "Swift.StaticString.init(_builtinStringLiteral: Builtin.RawPointer, utf8CodeUnitCount: Builtin.Word, isASCII: Builtin.Int1) -> Swift.StaticString":
            case "Swift.Double.init(_builtinFloatLiteral: Builtin.FPIEEE80) -> Swift.Double":
            case "Swift.Double.init(_builtinIntegerLiteral: Builtin.IntLiteral) -> Swift.Double":
            case "Swift.UInt.init(_builtinIntegerLiteral: Builtin.IntLiteral) -> Swift.UInt":
            case "Swift.Double.init(Swift.Int) -> Swift.Double": {
                C.valueTable().copy(resultName, params.get(0));
                return null;
            }
            case "Swift.Bool.init(_builtinBooleanLiteral: Builtin.Int1) -> Swift.Bool": {
                C.bc.block.addInstruction(new NewInstruction(resultName, resultType, C));
                C.bc.block.addInstruction(new StaticFieldWriteInstruction(resultName, "_value", params.get(0), C));
                return null;
            }

            case "Swift.DefaultStringInterpolation.init(literalCapacity: Swift.Int, interpolationCount: Swift.Int) -> Swift.DefaultStringInterpolation":
            case "Swift.String.init(stringInterpolation: Swift.DefaultStringInterpolation) -> Swift.String": {
                return new LiteralInstruction("", resultName, resultType, C);
            }
            case "Swift.DefaultStringInterpolation.appendLiteral(Swift.String) -> ()":
            case "Swift.DefaultStringInterpolation.appendInterpolation<A>(A) -> ()":
            case "Swift.DefaultStringInterpolation.appendInterpolation<A where A: Swift.CustomStringConvertible>(A) -> ()":
            case "Swift.DefaultStringInterpolation.appendInterpolation<A where A: Swift.CustomStringConvertible, A: Swift.TextOutputStreamable>(A) -> ()": {
                return null;
            }
            default: {
                Assertions.UNREACHABLE("Should not be called without checking isSummarized(): " + funcName);
                return null;
            }
        }
    }
}
