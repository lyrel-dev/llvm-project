//===--- LangStandards.cpp - Language Standard Definitions ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/LangStandard.h"
#include "clang/Config/config.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/TargetParser/Triple.h"
using namespace clang;

StringRef clang::languageToString(Language L) {
  switch (L) {
  case Language::Unknown:
    return "Unknown";
  case Language::Asm:
    return "Asm";
  case Language::LLVM_IR:
    return "LLVM IR";
  case Language::CIR:
    return "ClangIR";
  case Language::C:
    return "C";
  case Language::CXX:
    return "C++";
  case Language::ObjC:
    return "Objective-C";
  case Language::ObjCXX:
    return "Objective-C++";
  case Language::OpenCL:
    return "OpenCL";
  case Language::OpenCLCXX:
    return "OpenCLC++";
  case Language::CUDA:
    return "CUDA";
  case Language::HIP:
    return "HIP";
  case Language::HLSL:
    return "HLSL";
  }

  llvm_unreachable("unhandled language kind");
}

#define LANGSTANDARD(id, name, lang, desc, features, version)                  \
  static const LangStandard Lang_##id = {name, desc, features, Language::lang, \
                                         version};
#include "clang/Basic/LangStandards.def"

const LangStandard &LangStandard::getLangStandardForKind(Kind K) {
  switch (K) {
  case lang_unspecified:
    llvm::report_fatal_error("getLangStandardForKind() on unspecified kind");
#define LANGSTANDARD(id, name, lang, desc, features, version)                  \
  case lang_##id:                                                              \
    return Lang_##id;
#include "clang/Basic/LangStandards.def"
  }
  llvm_unreachable("Invalid language kind!");
}

LangStandard::Kind LangStandard::getLangKind(StringRef Name) {
  return llvm::StringSwitch<Kind>(Name)
#define LANGSTANDARD(id, name, lang, desc, features, version)                  \
  .Case(name, lang_##id)
#define LANGSTANDARD_ALIAS(id, alias) .Case(alias, lang_##id)
#include "clang/Basic/LangStandards.def"
      .Default(lang_unspecified);
}

LangStandard::Kind LangStandard::getHLSLLangKind(StringRef Name) {
  // HLSL is no longer supported. This function is retained for ABI compatibility
  // but always returns lang_unspecified.
  return LangStandard::lang_unspecified;
}

const LangStandard *LangStandard::getLangStandardForName(StringRef Name) {
  Kind K = getLangKind(Name);
  if (K == lang_unspecified)
    return nullptr;

  return &getLangStandardForKind(K);
}

LangStandard::Kind clang::getDefaultLanguageStandard(clang::Language Lang,
                                                     const llvm::Triple &T) {
  switch (Lang) {
  case Language::Unknown:
  case Language::LLVM_IR:
  case Language::CIR:
    llvm_unreachable("Invalid input kind!");
  case Language::OpenCL:
  case Language::OpenCLCXX:
    // OpenCL is not supported; this code path is unreachable since
    // the frontend rejects OpenCL inputs before reaching here.
    llvm_unreachable("OpenCL is not supported");
  case Language::Asm:
  case Language::C:
    // The PS4 uses C99 as the default C standard.
    if (T.isPS4())
      return LangStandard::lang_gnu99;
    return LangStandard::lang_gnu17;
  case Language::ObjC:
    // Objective-C is not supported; this code path is unreachable since
    // the frontend rejects ObjC inputs before reaching here.
    llvm_unreachable("Objective-C is not supported");
  case Language::CXX:
  case Language::ObjCXX:
  case Language::CUDA:
  case Language::HIP:
    return LangStandard::lang_gnucxx17;
  case Language::HLSL:
    // HLSL is not supported; this code path is unreachable since
    // the frontend rejects HLSL inputs before reaching here.
    llvm_unreachable("HLSL is not supported");
  }
  llvm_unreachable("unhandled Language kind!");
}
