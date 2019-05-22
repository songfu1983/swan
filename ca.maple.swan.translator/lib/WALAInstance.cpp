/******************************************************************************
 * Copyright (c) 2019 Maple @ University of Alberta
 * All rights reserved. This program and the accompanying materials (unless
 * otherwise specified by a license inside of the accompanying material)
 * are made available under the terms of the Eclipse Public License v2.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v20.html
 *****************************************************************************/

 // SEE HEADER FILE FOR DOCUMENTATION

#include "WALAInstance.h"
#include "SwiftCHook.h"

#include <CAstWrapper.h>
#include <launch.h>
#include <unistd.h>

#include "llvm/Support/raw_ostream.h"
#include "swift/SIL/SILModule.h"
#include "swift/AST/Module.h"
#include "swift/FrontendTool/FrontendTool.h"
#include "swift/Frontend/Frontend.h"
#include "SILWalaInstructionVisitor.h"
#include "swift/SIL/SILValue.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Path.h"

using namespace swift_wala;
using namespace llvm;
using namespace swift;

void WALAInstance::print(jobject Object) {
  auto ObjClass = JavaEnv->FindClass("java/lang/Object");
  auto ToString = JavaEnv->GetMethodID(ObjClass, "toString", "()Ljava/lang/String;");
  auto Message = (jstring) JavaEnv->CallObjectMethod(ObjClass, ToString);

  jboolean Result = false;
  const char *Text = JavaEnv->GetStringUTFChars(Message, &Result);

  outs() << "WALA: " << Text << "\n";
  JavaEnv->ReleaseStringUTFChars(Message, Text);
}

void WALAInstance::analyzeSILModule(SILModule &SM) {
  SILWalaInstructionVisitor Visitor(this, true);
  Visitor.visitModule(&SM);
}

void WALAInstance::analyze() {
  // this -emit-sil option is critical as it specifies the action for the frontend,
  // otherwise the compiler will not do anything and complain no action was given
  auto Argv = {"", "-emit-sil", File.c_str()};
  swift_wala::Observer observer(this); // create the hook
  SmallVector<const char *, 256> argv(Argv.begin(), Argv.end());

  // change current working path to allow for relative pathed input files
  // regular working dir is swan/ca.maple.swan.analysis, we change it to just swan/
  char temp[1024];
  std::string currentWorkingPath = getcwd(temp, sizeof(temp)) ? std::string( temp ) : std::string("");
  size_t lastSlashIndex = currentWorkingPath.find_last_of("/");
  std::string newCurrentWorkingPath = currentWorkingPath.substr(0, lastSlashIndex);
  chdir(newCurrentWorkingPath.c_str());

  // call Swift compiler frontend
  performFrontend(llvm::makeArrayRef(argv.data()+1,
                                     argv.data()+argv.size()),
                  argv[0], (void *)(intptr_t)getExecutablePath,
                  &observer);
}

WALAInstance::WALAInstance(JNIEnv *Env, jobject Obj) : JavaEnv(Env), Translator(Obj) {
  TRY(Exception, JavaEnv)
      CAst = new CAstWrapper(JavaEnv, Exception, Translator); // for JNI calls
      // find the bridge class
      auto TranslatorClass = JavaEnv->FindClass("ca/maple/swan/analysis/translator/SwiftToCAstTranslator");
      THROW_ANY_EXCEPTION(Exception);

      // get the file to analyze
      auto GetLocalFile = JavaEnv->GetMethodID(TranslatorClass, "getLocalFile", "()Ljava/lang/String;");
      THROW_ANY_EXCEPTION(Exception);
      auto LocalFile = (jstring)(JavaEnv->CallObjectMethod(Translator, GetLocalFile, 0));
      THROW_ANY_EXCEPTION(Exception);
      auto LocalFileStr = JavaEnv->GetStringUTFChars(LocalFile, 0);
      THROW_ANY_EXCEPTION(Exception);
      File = std::string(LocalFileStr);
      JavaEnv->ReleaseStringUTFChars(LocalFile, LocalFileStr);
      THROW_ANY_EXCEPTION(Exception);
  CATCH()
      // TODO: Report exceptions to user
}


jobject WALAInstance::makeBigDecimal(const char *strData, int strLen) {
  char *safeData = strndup(strData, strLen);
  jobject val = JavaEnv->NewStringUTF(safeData);
  delete safeData;
  jclass bigDecimalCls = JavaEnv->FindClass("java/math/BigDecimal");
  jmethodID bigDecimalInit = JavaEnv->GetMethodID(bigDecimalCls,
    "<init>", "(Ljava/lang/String;)V");
  jobject bigDecimal = JavaEnv->NewObject(bigDecimalCls, bigDecimalInit, val);
  JavaEnv->DeleteLocalRef(val);
  return bigDecimal;
}

jobject WALAInstance::getCAstNodes() {
  jclass java_util_ArrayList      = JavaEnv->FindClass("java/util/ArrayList");
  jmethodID java_util_ArrayList_  = JavaEnv->GetMethodID(java_util_ArrayList, "<init>", "(I)V");
  jmethodID java_util_ArrayList_add         = JavaEnv->GetMethodID(java_util_ArrayList, "add", "(Ljava/lang/Object;)Z");

  auto result = JavaEnv->NewObject(java_util_ArrayList, java_util_ArrayList_, CAstNodes.size());

  for (jobject decl: CAstNodes) {
    JavaEnv->CallBooleanMethod(result, java_util_ArrayList_add, decl);
  }

  return result;
}