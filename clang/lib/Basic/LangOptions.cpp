//===- LangOptions.cpp - C Language Family Language Options ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines the LangOptions class.
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/LangOptions.h"
#include "clang/Basic/LangStandard.h"
#include "llvm/Support/Path.h"

using namespace clang;

LangOptions::LangOptions() : LangStd(LangStandard::lang_unspecified) {
#define LANGOPT(Name, Bits, Default, Compatibility, Description) Name = Default;
#define ENUM_LANGOPT(Name, Type, Bits, Default, Compatibility, Description)    \
  set##Name(Default);
#include "clang/Basic/LangOptions.def"
}

void LangOptions::resetNonModularOptions() {
#define LANGOPT(Name, Bits, Default, Compatibility, Description)               \
  if constexpr (CompatibilityKind::Compatibility == CompatibilityKind::Benign) \
    Name = Default;
#define ENUM_LANGOPT(Name, Type, Bits, Default, Compatibility, Description)    \
  if constexpr (CompatibilityKind::Compatibility == CompatibilityKind::Benign) \
    Name = static_cast<unsigned>(Default);
#include "clang/Basic/LangOptions.def"

  // Reset "benign" options with implied values (Options.td ImpliedBy relations)
  // rather than their defaults. This avoids unexpected combinations and
  // invocations that cannot be round-tripped to arguments.
  // FIXME: we should derive this automatically from ImpliedBy in tablegen.
  AllowFPReassoc = UnsafeFPMath;
  NoHonorInfs = FastMath;
  NoHonorNaNs = FastMath;

  // These options do not affect AST generation.
  NoSanitizeFiles.clear();
  XRayAlwaysInstrumentFiles.clear();
  XRayNeverInstrumentFiles.clear();

  CurrentModule.clear();
  IsHeaderFile = false;
}

bool LangOptions::isNoBuiltinFunc(StringRef FuncName) const {
  for (unsigned i = 0, e = NoBuiltinFuncs.size(); i != e; ++i)
    if (FuncName == NoBuiltinFuncs[i])
      return true;
  return false;
}

VersionTuple LangOptions::getOpenCLVersionTuple() const {
  const int Ver = OpenCLCPlusPlus ? OpenCLCPlusPlusVersion : OpenCLVersion;
  if (OpenCLCPlusPlus && Ver != 100)
    return VersionTuple(Ver / 100);
  return VersionTuple(Ver / 100, (Ver % 100) / 10);
}

unsigned LangOptions::getOpenCLCompatibleVersion() const {
  if (!OpenCLCPlusPlus)
    return OpenCLVersion;
  if (OpenCLCPlusPlusVersion == 100)
    return 200;
  if (OpenCLCPlusPlusVersion == 202100)
    return 300;
  llvm_unreachable("Unknown OpenCL version");
}

void LangOptions::remapPathPrefix(SmallVectorImpl<char> &Path) const {
  for (const auto &Entry : MacroPrefixMap)
    if (llvm::sys::path::replace_path_prefix(Path, Entry.first, Entry.second))
      break;
}

std::string LangOptions::getOpenCLVersionString() const {
  std::string Result;
  {
    llvm::raw_string_ostream Out(Result);
    Out << (OpenCLCPlusPlus ? "C++ for OpenCL" : "OpenCL C") << " version "
        << getOpenCLVersionTuple().getAsString();
  }
  return Result;
}

void LangOptions::setLangDefaults(LangOptions &Opts, Language Lang,
                                  const llvm::Triple &T,
                                  std::vector<std::string> &Includes,
                                  LangStandard::Kind LangStd) {
  // Set some properties which depend solely on the input kind; it would be nice
  // to move these to the language standard, and have the driver resolve the
  // input kind + language standard.
  //
  // Note: Objective-C, OpenCL, CUDA, HIP, and HLSL inputs are rejected at the
  // frontend before reaching this point. Only C and C++ are supported.
  if (Lang == Language::Asm)
    Opts.AsmPreprocessor = 1;

  if (LangStd == LangStandard::lang_unspecified)
    LangStd = getDefaultLanguageStandard(Lang, T);
  const LangStandard &Std = LangStandard::getLangStandardForKind(LangStd);
  Opts.LangStd = LangStd;
  Opts.LineComment = Std.hasLineComments();
  Opts.C99 = Std.isC99();
  Opts.C11 = Std.isC11();
  Opts.C17 = Std.isC17();
  Opts.C23 = Std.isC23();
  Opts.C2y = Std.isC2y();
  Opts.CPlusPlus = Std.isCPlusPlus();
  Opts.CPlusPlus11 = Std.isCPlusPlus11();
  Opts.CPlusPlus14 = Std.isCPlusPlus14();
  Opts.CPlusPlus17 = Std.isCPlusPlus17();
  Opts.CPlusPlus20 = Std.isCPlusPlus20();
  Opts.CPlusPlus23 = Std.isCPlusPlus23();
  Opts.CPlusPlus26 = Std.isCPlusPlus26();
  Opts.GNUMode = Std.isGNUMode();
  Opts.GNUCVersion = 0;
  Opts.HexFloats = Std.hasHexFloats();
  Opts.WChar = Std.isCPlusPlus();
  Opts.Digraphs = Std.hasDigraphs();
  Opts.RawStringLiterals = Std.hasRawStringLiterals();
  Opts.AllowLiteralDigitSeparator = Std.allowLiteralDigitSeparator();
  Opts.NamedLoops = Std.isC2y();

  // C++ and C23 have bool, true, false keywords.
  Opts.Bool = Opts.CPlusPlus || Opts.C23;
}

FPOptions FPOptions::defaultWithoutTrailingStorage(const LangOptions &LO) {
  FPOptions result(LO);
  return result;
}

FPOptionsOverride FPOptions::getChangesSlow(const FPOptions &Base) const {
  FPOptions::storage_type OverrideMask = 0;
#define FP_OPTION(NAME, TYPE, WIDTH, PREVIOUS)                                 \
  if (get##NAME() != Base.get##NAME())                                         \
    OverrideMask |= NAME##Mask;
#include "clang/Basic/FPOptions.def"
  return FPOptionsOverride(*this, OverrideMask);
}

LLVM_DUMP_METHOD void FPOptions::dump() {
#define FP_OPTION(NAME, TYPE, WIDTH, PREVIOUS)                                 \
  llvm::errs() << "\n " #NAME " " << get##NAME();
#include "clang/Basic/FPOptions.def"
  llvm::errs() << "\n";
}

LLVM_DUMP_METHOD void FPOptionsOverride::dump() {
#define FP_OPTION(NAME, TYPE, WIDTH, PREVIOUS)                                 \
  if (has##NAME##Override())                                                   \
    llvm::errs() << "\n " #NAME " Override is " << get##NAME##Override();
#include "clang/Basic/FPOptions.def"
  llvm::errs() << "\n";
}

std::optional<uint32_t> LangOptions::getCPlusPlusLangStd() const {
  if (!CPlusPlus)
    return std::nullopt;

  LangStandard::Kind Std;
  if (CPlusPlus26)
    Std = LangStandard::lang_cxx26;
  else if (CPlusPlus23)
    Std = LangStandard::lang_cxx23;
  else if (CPlusPlus20)
    Std = LangStandard::lang_cxx20;
  else if (CPlusPlus17)
    Std = LangStandard::lang_cxx17;
  else if (CPlusPlus14)
    Std = LangStandard::lang_cxx14;
  else if (CPlusPlus11)
    Std = LangStandard::lang_cxx11;
  else
    Std = LangStandard::lang_cxx98;

  return LangStandard::getLangStandardForKind(Std).getVersion();
}

std::optional<uint32_t> LangOptions::getCLangStd() const {
  LangStandard::Kind Std;
  if (C2y)
    Std = LangStandard::lang_c2y;
  else if (C23)
    Std = LangStandard::lang_c23;
  else if (C17)
    Std = LangStandard::lang_c17;
  else if (C11)
    Std = LangStandard::lang_c11;
  else if (C99)
    Std = LangStandard::lang_c99;
  else if (!GNUMode && Digraphs)
    Std = LangStandard::lang_c94;
  else
    return std::nullopt;

  return LangStandard::getLangStandardForKind(Std).getVersion();
}
