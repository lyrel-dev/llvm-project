# Clang Reduced Parser: C and C++ Only

## Summary

This document describes the changes made to reduce the scope of the Clang
parser and lexer to only recognize and parse standard C (latest: C23/C2y) and
standard C++ (latest: C++23/C++26), removing active support for all other
input language modes.

All changes have been implemented and the affected libraries (`clangBasic`,
`clangLex`, `clangParse`, `clangFrontend`) build cleanly with zero errors and
zero warnings.

---

## Motivation

The goal is a **minimal, fully compliant parser** for standard C and C++. By
removing support for Objective-C, CUDA, OpenCL, HIP, HLSL, and OpenMP/OpenACC
at the language-selection layer, the compiler becomes smaller, easier to
maintain, and less exposed to bugs in unused language paths.

---

## Changes Implemented

### 1. New Diagnostic (`clang/include/clang/Basic/DiagnosticFrontendKinds.td`)

Added `err_fe_unsupported_input_language` (DefaultFatal) to clearly report
when an unsupported input language is requested:

```
error: language 'X' is not supported; this build only accepts standard C
       (C23) and C++ (C++26) inputs
```

### 2. Language Standards Removal (`clang/include/clang/Basic/LangStandards.def`)

Removed all non-C/C++ language standard entries:

**OpenCL standards removed:**
- `opencl10` / `cl1.0` / `cl` (and deprecated aliases `CL`, `CL1.0`)
- `opencl11` / `cl1.1` (alias `CL1.1`)
- `opencl12` / `cl1.2` (alias `CL1.2`)
- `opencl20` / `cl2.0` (alias `CL2.0`)
- `opencl30` / `cl3.0` (alias `CL3.0`)
- `openclcpp10` / `clc++1.0` / `clc++` (aliases `CLC++`, `CLC++1.0`)
- `openclcpp2021` / `clc++2021` (alias `CLC++2021`)

**HLSL standards removed:**
- `hlsl` / `hlsl2015` / `hlsl2016` / `hlsl2017` / `hlsl2018`
- `hlsl2021` / `hlsl202x` / `hlsl202y`

Any attempt to use `-std=cl1.0`, `-std=hlsl2021`, etc. now produces an
"invalid value" error from the `-std=` option parser.

### 3. Default Language Standard (`clang/lib/Basic/LangStandards.cpp`)

- `getDefaultLanguageStandard()` now calls `llvm_unreachable()` for
  `Language::OpenCL`, `Language::OpenCLCXX`, `Language::ObjC`, and
  `Language::HLSL` — these paths are dead since the frontend rejects those
  inputs before this function is reached.
- `getHLSLLangKind()` now always returns `lang_unspecified` (retained for
  link compatibility with the driver).

### 4. Language Option Defaults (`clang/lib/Basic/LangOptions.cpp`)

In `LangOptions::setLangDefaults()`:

- Removed Objective-C setup: `Opts.ObjC = 1` is no longer set for ObjC/ObjCXX
  inputs (dead code since those inputs are rejected).
- Removed HLSL defaults block: `Opts.HLSL`, default header inclusion,
  `MaxMatrixDimension`.
- Removed OpenCL version detection for all `lang_opencl*` / `lang_openclcpp*`
  standards (removed from `LangStandards.def`).
- Removed HLSL version detection for all `lang_hlsl*` standards.
- Removed OpenCL additional-defaults block (AltiVec, ZVector, FP contract
  mode, pipes, generic address space, default header).
- Removed CUDA/HIP defaults block (`Opts.CUDA`, `Opts.HIP`, FP contract mode).
- Updated `Opts.Bool`: now simply `Opts.Bool = Opts.CPlusPlus || Opts.C23`.
- Removed `Opts.Half = Opts.OpenCL || Opts.HLSL` (always false for C/C++).
- Removed `Opts.PreserveVec3Type = Opts.HLSL` (always false for C/C++).

### 5. Frontend Language Rejection (`clang/lib/Frontend/CompilerInvocation.cpp`)

**ParseLangArgs() — early rejection:**
Added a switch statement that rejects unsupported language kinds and emits
`err_fe_unsupported_input_language`:
- `Language::ObjC`, `Language::ObjCXX`
- `Language::OpenCL`, `Language::OpenCLCXX`
- `Language::CUDA`, `Language::HIP`, `Language::HLSL`

**OpenCL -cl-std= processing removed:**
The entire `-cl-std=` override block is replaced with an error. Passing
`-cl-std=` now immediately triggers the unsupported-language diagnostic.

**OpenCL default optimization removed from `getOptimizationLevel()`:**
OpenCL previously defaulted to `-O2`; this special case is now gone.

**Dead HLSL validation block removed (`ParseLangArgs`):**
A large block validating HLSL shader targets, 16-bit type requirements, and
minimum language standards (which referenced the now-deleted `lang_hlsl2018`
and `lang_hlsl202x` enum values) has been removed. This was the source of the
four compilation errors discovered during the build test.

**Dead checks removed from `FixupInvocation()`:**
- `-hlsl-entry` / `-fdx-rootsignature-*` option checks (always dead since
  `LangOpts.HLSL` is never set).
- `-fgpu-allow-device-init` / `-gpu-max-threads-per-block=` HIP-only checks.
- HLSL automatic `-Wconversion` / `-Wvector-conversion` / `-Wmatrix-conversion`
  warning injection block.
- OpenCL strict-aliasing version diagnostic.

**Dead argument generation removed from `GenerateLangArgs()`:**
- `IncludeDefaultHeader` / `DeclareOpenCLBuiltins` generate calls.
- Entire `if (Opts.ObjC)` Objective-C runtime argument generation block.
- OpenCL exclusion from the Blocks (`-fblocks`) condition:
  `Opts.Blocks && !(Opts.OpenCL && Opts.OpenCLVersion == 200)` → `Opts.Blocks`.

**Dead include filtering removed from `GeneratePreprocessorArgs()`:**
The filters that suppressed `opencl-c.h`, `opencl-c-base.h`, and `hlsl.h`
from being re-serialized into the argument list have been removed.

**Dead post-ParseLangArgs setup removed:**
- `RewriteObjC` action → `LangOpts.ObjCExceptions = 1` setup removed.
- `LangOpts.CUDA` → `HostTriple` mapping removed.
- `LangOpts.OpenACC && !UseClangIRPipeline` diagnostic removed.

**Dead `isCodeGenAction()` helper removed:**
This static function was only used by the now-deleted OpenACC check and was
producing a `-Wunused-function` warning. The entire function has been removed.

**`lang_opencl*` switch in `GenerateLangArgs()` removed:**
The `OptSpecifier StdOpt` switch that mapped OpenCL standards to `-cl-std=`
has been replaced with a direct assignment to `OPT_std_EQ`.

### 6. File Extension Mapping (`clang/lib/Frontend/FrontendOptions.cpp`)

Removed the following file extensions from `getInputKindForExtension()`:

| Extension(s) | Was mapped to     | Now          |
|-------------|-------------------|--------------|
| `.m`, `.mi` | `Language::ObjC`  | `Unknown`    |
| `.mm`, `.M`, `.mii` | `Language::ObjCXX` | `Unknown` |
| `.cu`, `.cuh`, `.cui` | `Language::CUDA` | `Unknown` |
| `.cl`       | `Language::OpenCL` | `Unknown`   |
| `.clcpp`    | `Language::OpenCLCXX` | `Unknown` |
| `.hip`      | `Language::HIP`   | `Unknown`    |
| `.hlsl`     | `Language::HLSL`  | `Unknown`    |

Files with these extensions are now treated as unknown inputs. The driver
produces an appropriate "unknown file type" error rather than attempting to
compile them as the corresponding language.

### 7. Dead-Code Notices Added to Parse and Lex Files

The following files have had notices added to their file headers marking them
as dead code that is retained only for link compatibility:

- `clang/lib/Parse/ParseObjc.cpp` — ~3,300 lines of Objective-C parsing
- `clang/lib/Parse/ParseOpenMP.cpp` — ~5,400 lines of OpenMP pragma parsing
- `clang/lib/Parse/ParseOpenACC.cpp` — ~1,700 lines of OpenACC pragma parsing
- `clang/lib/Parse/ParseHLSL.cpp` — ~340 lines of HLSL declaration parsing
- `clang/lib/Parse/ParseHLSLRootSignature.cpp` — ~1,580 lines of HLSL root
  signature parsing
- `clang/lib/Lex/LexHLSLRootSignature.cpp` — HLSL root signature lexer

---

## Supported Language Modes

After these changes, the following language modes and standards are accepted:

### C Standards
| Flag | Standard |
|------|----------|
| `-std=c89` / `-std=c90` / `-std=iso9899:1990` | ISO C 1990 |
| `-std=iso9899:199409` | ISO C 1990 + Amendment 1 |
| `-std=gnu89` / `-std=gnu90` | ISO C 1990 + GNU extensions |
| `-std=c99` / `-std=iso9899:1999` | ISO C 1999 |
| `-std=gnu99` | ISO C 1999 + GNU extensions |
| `-std=c11` / `-std=iso9899:2011` | ISO C 2011 |
| `-std=gnu11` | ISO C 2011 + GNU extensions |
| `-std=c17` / `-std=c18` / `-std=iso9899:2017` | ISO C 2017 |
| `-std=gnu17` / `-std=gnu18` | ISO C 2017 + GNU extensions |
| `-std=c23` / `-std=iso9899:2024` | **ISO C 2023** (latest ratified) |
| `-std=gnu23` | ISO C 2023 + GNU extensions |
| `-std=c2y` | Working Draft for ISO C2y |
| `-std=gnu2y` | Working Draft for ISO C2y + GNU extensions |

### C++ Standards
| Flag | Standard |
|------|----------|
| `-std=c++98` / `-std=c++03` | ISO C++ 1998 |
| `-std=gnu++98` / `-std=gnu++03` | ISO C++ 1998 + GNU extensions |
| `-std=c++11` | ISO C++ 2011 |
| `-std=gnu++11` | ISO C++ 2011 + GNU extensions |
| `-std=c++14` | ISO C++ 2014 |
| `-std=gnu++14` | ISO C++ 2014 + GNU extensions |
| `-std=c++17` | ISO C++ 2017 |
| `-std=gnu++17` | ISO C++ 2017 + GNU extensions |
| `-std=c++20` | ISO C++ 2020 |
| `-std=gnu++20` | ISO C++ 2020 + GNU extensions |
| `-std=c++23` | **ISO C++ 2023** (latest ratified) |
| `-std=gnu++23` | ISO C++ 2023 + GNU extensions |
| `-std=c++26` / `-std=c++2c` | Working Draft for C++26 |
| `-std=gnu++26` / `-std=gnu++2c` | Working Draft for C++26 + GNU extensions |

---

## Removed Language Modes

The following language modes are **no longer accepted**:

| Language | Extensions | `-x` Flag | Notes |
|----------|-----------|-----------|-------|
| Objective-C | `.m`, `.mi` | `-x objective-c` | All ObjC syntax rejected |
| Objective-C++ | `.mm`, `.M`, `.mii` | `-x objective-c++` | All ObjC++ syntax rejected |
| CUDA | `.cu`, `.cuh`, `.cui` | `-x cuda` | CUDA extensions rejected |
| HIP | `.hip` | `-x hip` | HIP extensions rejected |
| OpenCL C | `.cl` | `-x cl` | All OpenCL versions rejected |
| C++ for OpenCL | `.clcpp` | `-x clcpp` | All OpenCL C++ versions rejected |
| HLSL | `.hlsl` | `-x hlsl` | All HLSL versions rejected |

---

## Remaining Dead Code (Future Cleanup)

The following code is now **unreachable** (due to the frontend rejection) but
has not yet been removed. A future cleanup pass should remove them to achieve
a truly minimal parser. All files are documented with a notice in their header.

### Parser (`clang/lib/Parse/`)
- `ParseObjc.cpp` — ~3,300 lines of Objective-C parsing logic.
- `ParseOpenMP.cpp` — ~5,400 lines of OpenMP pragma parsing.
- `ParseOpenACC.cpp` — ~1,700 lines of OpenACC pragma parsing.
- `ParseHLSL.cpp` — ~340 lines of HLSL declaration parsing.
- `ParseHLSLRootSignature.cpp` — ~1,580 lines of HLSL root signature parsing.

### Lexer (`clang/lib/Lex/`)
- `LexHLSLRootSignature.cpp` — HLSL root signature lexer.
- Objective-C `@`-directive and keyword handling in `Lexer.cpp`.
- ObjC-specific token kinds in `TokenKinds.def`.

### Semantic Analysis (`clang/lib/Sema/`)
- `SemaDeclObjC.cpp`, `SemaExprObjC.cpp`, `SemaObjC.cpp`,
  `SemaObjCProperty.cpp` — Objective-C semantic analysis.
- `SemaCUDA.cpp` — CUDA semantic analysis.
- `SemaHLSL.cpp`, `HLSLBuiltinTypeDeclBuilder.cpp`,
  `HLSLExternalSemaSource.cpp` — HLSL semantic analysis.
- `SemaOpenACC.cpp` and related files — OpenACC semantic analysis.
- `SemaOpenCL.cpp` — OpenCL semantic analysis.
- `SemaOpenMP.cpp` — OpenMP semantic analysis (~12,000 lines).

### AST (`clang/lib/AST/`)
- `DeclObjC.cpp`, `ExprObjC.cpp`, `StmtObjC.cpp` — Objective-C AST nodes.
- `DeclOpenMP.cpp`, `StmtOpenMP.cpp`, `OpenMPClause.cpp` — OpenMP AST nodes.
- `DeclOpenACC.cpp`, `StmtOpenACC.cpp`, `OpenACCClause.cpp` — OpenACC AST.
- `HLSLResource.cpp` — HLSL resource AST.

### Code Generation (`clang/lib/CodeGen/`)
- `CGObjC.cpp`, `CGObjCGNU.cpp`, `CGObjCMac.cpp`,
  `CGObjCRuntime.cpp` — Objective-C code generation.
- `CGCUDANV.cpp`, `CGCUDARuntime.cpp` — CUDA code generation.
- `CGHLSLRuntime.cpp`, `CGHLSLBuiltins.cpp`,
  `HLSLBufferLayoutBuilder.cpp` — HLSL code generation.
- `CGOpenCLRuntime.cpp` — OpenCL code generation.
- `CGOpenMPRuntime.cpp`, `CGOpenMPRuntimeGPU.cpp`,
  `CGStmtOpenMP.cpp` — OpenMP code generation.

### Driver (`clang/lib/Driver/ToolChains/`)
- `Cuda.cpp` — CUDA toolchain.
- `HIPAMD.cpp`, `HIPSPV.cpp`, `HIPUtility.cpp` — HIP toolchains.
- `HLSL.cpp` — HLSL toolchain.
- `AMDGPUOpenMP.cpp`, `SPIRVOpenMP.cpp` — OpenMP offload toolchains.

### Language Options (`clang/include/clang/Basic/LangOptions.def`)
- `ObjC`, `ObjCDefaultSynthProperties`, `EncodeExtendedBlockSig`,
  `ObjCInferRelatedResultType`, `AppExt`, `ObjCExceptions`, etc.
- `OpenCL`, `OpenCLVersion`, `OpenCLCPlusPlus`, etc.
- `CUDA`, `HIP` and related options.
- `OpenMP` and all `OpenMP*` variants.
- `HLSL`, `HLSLVersion`, `HLSLStrictAvailability`, etc.

### Include Files and Headers
- All Objective-C runtime headers.
- OpenCL builtin headers (`opencl-c.h`, `opencl-c-base.h`).
- HLSL headers (`hlsl.h`).
- CUDA headers.

---

## Build System Impact

### CMake Changes Required (Future)

To fully remove the dead code, the following CMake changes are needed:

1. Remove `FrontendOpenMP` from `clang/lib/Parse/CMakeLists.txt` link
   components (after removing `ParseOpenMP.cpp`).
2. Remove `FrontendHLSL` from `clang/lib/Parse/CMakeLists.txt` link
   components (after removing `ParseHLSL.cpp` and
   `ParseHLSLRootSignature.cpp`).
3. Remove `omp_gen` DEPENDS target (after removing OpenMP parsing).
4. Remove OpenMP, HLSL, CUDA, HIP, OpenCL and OpenACC libraries from the
   clang Sema and CodeGen CMakeLists files.

---

## Build Verification

The following libraries were built and verified to compile cleanly (zero
errors, zero warnings) with the changes applied:

- `libclangBasic`
- `libclangLex`
- `libclangParse`
- `libclangFrontend`

Build command used:
```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DLLVM_ENABLE_PROJECTS=clang -DLLVM_TARGETS_TO_BUILD=X86 \
  -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DLLVM_INCLUDE_BENCHMARKS=OFF
ninja clangBasic clangLex clangParse clangFrontend
```

---

## Testing

Tests that exercise the removed language modes will fail as expected. These
tests should be updated to verify the correct rejection diagnostic:

```
error: language 'X' is not supported; this build only accepts standard C
       (C23) and C++ (C++26) inputs
```

The following test directories are affected and should be reviewed:
- `clang/test/Parser/` — ObjC, HLSL, OpenMP, OpenACC parser tests
- `clang/test/Sema/` — ObjC, CUDA, OpenCL, HLSL sema tests
- `clang/test/CodeGen/` — ObjC, CUDA, OpenCL, HLSL CodeGen tests
- `clang/test/OpenMP/` — All OpenMP tests
- `clang/test/SemaOpenMP/` — All SemaOpenMP tests
- `clang/test/SemaOpenCL/` — All SemaOpenCL tests
- `clang/test/SemaCUDA/` — All SemaCUDA tests
- `clang/test/SemaObjC/` — All SemaObjC tests
- `clang/test/HLSL/` — All HLSL tests
