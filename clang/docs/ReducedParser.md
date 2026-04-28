# Clang Reduced Parser: C and C++ Only

## Summary

This document describes the changes made to reduce the scope of the Clang
parser and lexer to only recognize and parse standard C (latest: C23/C2y) and
standard C++ (latest: C++23/C++26), removing active support for all other
input language modes.

---

## Motivation

The goal is a **minimal, fully compliant parser** for standard C and C++. By
removing support for Objective-C, CUDA, OpenCL, HIP, HLSL, and OpenMP/OpenACC
at the language-selection layer, the compiler becomes smaller, easier to
maintain, and less exposed to bugs in unused language paths.

---

## Changes Implemented

### 1. Frontend Language Rejection (`clang/lib/Frontend/CompilerInvocation.cpp`)

Added early rejection in `ParseLangArgs()` for the following language kinds:

- `Language::ObjC` (Objective-C)
- `Language::ObjCXX` (Objective-C++)
- `Language::OpenCL` (OpenCL C)
- `Language::OpenCLCXX` (C++ for OpenCL)
- `Language::CUDA` (CUDA)
- `Language::HIP` (HIP)
- `Language::HLSL` (HLSL)

Any attempt to compile a file in one of these modes now produces a fatal
diagnostic:

```
error: language 'X' is not supported; this build only accepts standard C
       (C23) and C++ (C++26) inputs
```

Additional changes in this file:
- Removed the `-cl-std=` OpenCL standard override processing block.
- Removed the `lang_opencl*` switch cases from `GenerateLangArgs()` (the
  serialization path for `LangOptions`).
- Removed the OpenCL default optimization level (`-O2` for OpenCL) from
  `getOptimizationLevel()`.

### 2. New Diagnostic (`clang/include/clang/Basic/DiagnosticFrontendKinds.td`)

Added `err_fe_unsupported_input_language` (DefaultFatal) to clearly report
when an unsupported input language is requested.

### 3. Language Standards Removal (`clang/include/clang/Basic/LangStandards.def`)

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

This means `-std=cl1.0`, `-std=hlsl2021`, etc. now report an unknown standard.

### 4. Default Language Standard (`clang/lib/Basic/LangStandards.cpp`)

- `getDefaultLanguageStandard()` now calls `llvm_unreachable()` for
  `Language::OpenCL`, `Language::OpenCLCXX`, `Language::ObjC`, and
  `Language::HLSL`  — these paths are dead since the frontend rejects those
  language inputs first.
- `getHLSLLangKind()` now always returns `lang_unspecified` (retained for
  ABI compatibility with callers in the driver but effectively disabled).

### 5. Language Option Defaults (`clang/lib/Basic/LangOptions.cpp`)

In `LangOptions::setLangDefaults()`:

- Removed Objective-C setup: `Opts.ObjC = 1` is no longer set for ObjC/ObjCXX
  inputs (dead code since those inputs are rejected).
- Removed HLSL defaults block: `Opts.HLSL`, default header inclusion,
  `MaxMatrixDimension`.
- Removed OpenCL version detection for `lang_opencl*` and `lang_openclcpp*`
  standards.
- Removed HLSL version detection for `lang_hlsl*` standards.
- Removed the OpenCL additional-defaults block (AltiVec, ZVector, FP contract
  mode, pipes, generic address space, default header).
- Removed CUDA/HIP defaults block (`Opts.CUDA`, `Opts.HIP`, FP contract mode).
- Updated `Opts.Bool`: removed the dead `Opts.OpenCL` term; now simply
  `Opts.Bool = Opts.CPlusPlus || Opts.C23`.
- Removed `Opts.Half = Opts.OpenCL || Opts.HLSL` (now always false for C/C++).
- Removed `Opts.PreserveVec3Type = Opts.HLSL` (always false for C/C++).

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

Files with these extensions will be treated as unknown inputs. The driver will
produce an appropriate error.

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
a truly minimal parser:

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
