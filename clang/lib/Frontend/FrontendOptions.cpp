//===- FrontendOptions.cpp ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/FrontendOptions.h"
#include "clang/Basic/LangStandard.h"
#include "llvm/ADT/StringSwitch.h"

using namespace clang;

InputKind FrontendOptions::getInputKindForExtension(StringRef Extension) {
  return llvm::StringSwitch<InputKind>(Extension)
      .Cases({"ast", "pcm"},
             InputKind(Language::Unknown, InputKind::Precompiled))
      .Case("c", Language::C)
      .Cases({"S", "s"}, Language::Asm)
      .Case("i", InputKind(Language::C).getPreprocessed())
      .Case("ii", InputKind(Language::CXX).getPreprocessed())
      // Objective-C source extensions (.m, .mi, .mm, .M, .mii) are not
      // supported. These file types will be treated as unknown inputs,
      // resulting in an appropriate error message from the driver.
      // CUDA source extensions (.cu, .cuh, .cui) are not supported.
      // OpenCL source extensions (.cl, .clcpp) are not supported.
      // HIP source extensions (.hip) are not supported.
      // HLSL source extensions (.hlsl) are not supported.
      .Cases({"C", "cc", "cp"}, Language::CXX)
      .Cases({"cpp", "CPP", "c++", "cxx", "hpp", "hxx"}, Language::CXX)
      .Case("cppm", Language::CXX)
      .Cases({"iim", "iih"}, InputKind(Language::CXX).getPreprocessed())
      .Cases({"ll", "bc"}, Language::LLVM_IR)
      .Case("cir", Language::CIR)
      .Default(Language::Unknown);
}
