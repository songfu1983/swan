package ca.maple.swan.swift.translator;

import com.ibm.wala.cast.tree.CAstNode;

public interface SwiftCAstNode extends CAstNode {

    public static final int GLOBAL_DECL_STMT = SUB_LANGUAGE_BASE + 1;
}
