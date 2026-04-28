# Clang Reduced Parser — Iterative Dead Code Removal Plan

## Goal

Produce a minimal, fully functional Clang parser and lexer that only supports
**standard C** (C89 through C2y) and **standard C++** (C++98 through C++26).

All code supporting the following languages must be **physically deleted** (not
commented out, not guarded by notes):

- Objective-C / Objective-C++
- OpenMP
- OpenACC
- HLSL
- CUDA / HIP

The approach is **top-down iterative**:

1. Start from the top-level parser entry (`ParseAST.h` / `ParseAST.cpp`) and
   the main parser class (`Parser.h` / `Parser.cpp`).
2. Remove dead source files entirely once their declarations are gone.
3. Work through all remaining parse files, removing dead branches.
4. Verify the build is clean after each commit.

---

## Status Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Done and verified to compile |
| 🔧 | In progress |
| ⬜ | Not started |

---

## Commit 0 — Baseline (already merged)

Preliminary groundwork from the previous session:

- ✅ `DiagnosticFrontendKinds.td` — new `err_fe_unsupported_input_language`
- ✅ `LangStandards.def` — removed all OpenCL and HLSL standard entries
- ✅ `LangStandards.cpp` — stubbed `getHLSLLangKind()`; unreachable paths for ObjC/OpenCL/HLSL in `getDefaultLanguageStandard()`
- ✅ `LangOptions.cpp` — removed HLSL/OpenCL/CUDA/HIP/ObjC blocks from `setLangDefaults()`
- ✅ `CompilerInvocation.cpp` — early rejection of non-C/C++ languages; removed all dead HLSL/HIP/ObjC/OpenCL/OpenACC post-parse setup
- ✅ `FrontendOptions.cpp` — removed non-C/C++ file extension mappings
- ✅ Dead-code notices on `ParseObjc.cpp`, `ParseOpenMP.cpp`, `ParseOpenACC.cpp`, `ParseHLSL.cpp`, `ParseHLSLRootSignature.cpp`, `LexHLSLRootSignature.cpp`

---

## Commit 1 — `Parser.h` + `Parser.cpp`

**Files:** `clang/include/clang/Parse/Parser.h`, `clang/lib/Parse/Parser.cpp`

### Parser.h — Removals

**Includes (4 lines):**
- `#include "clang/Basic/OpenACCKinds.h"`
- `#include "clang/Sema/SemaObjC.h"`
- `#include "clang/Sema/SemaOpenMP.h"`
- `#include "llvm/Frontend/OpenMP/OMPContext.h"`

**Forward declarations (8 lines):**
- `class InMessageExpressionRAIIObject;`
- `class OMPClause;`
- `class OpenACCClause;`
- `class ObjCTypeParamList;`
- `struct OMPTraitProperty;`
- `struct OMPTraitSelector;`
- `struct OMPTraitSet;`
- `class OMPTraitInfo;`

**Enum (12 lines):**
- `enum class ObjCTypeQual { ... }` (lines 91–102)

**Table of Contents entries (4 lines):**
- `// 7. HLSL Constructs (ParseHLSL.cpp)`
- `// 9. Objective-C Constructs (ParseObjc.cpp)`
- `// 10. OpenACC Constructs (ParseOpenACC.cpp)`
- `// 11. OpenMP Constructs (ParseOpenMP.cpp)`

**Scattered inline methods / declarations (~10 lines):**
- `bool isHLSLQualifier(const Token &Tok) const;` (line 2332)
- `void ParseHLSLQualifiers(ParsedAttributes &Attrs);` (line 2333)
- `void ParseCUDAFunctionAttributes(ParsedAttributes &attrs);` (line 2331)
- `void ParseOpenMPAttributeArgs(...)` (line 2978)
- `void ParseHLSLRootSignatureAttributeArgs(...)` (line 3608)
- `bool tryParseOpenMPArrayShapingCastPart();` (line 4375)

**Complete sections (≈1,700 lines):**
- HLSL section (lines ~5190–5232)
- Objective-C section (lines ~5348–6054)
- OpenACC section (lines ~6062–6322)
- OpenMP section (lines ~6330–7020)

**Member variables:**
- `InMessageExpression`, `ParsingInObjCContainer`, `CurParsedObjCImpl`
- `ObjCTypeQuals[]`, `Ident_instancetype`, `Ident_super`
- `OpenMPDirectiveParsing`, `OMPClauseKind`
- `OpenACCDirectiveParsing`, `AllowOpenACCArraySections`
- `OpenMPHandler`, `OpenACCHandler`, `CUDAForceHostDeviceHandler`

**Friend declarations:**
- `friend class InMessageExpressionRAIIObject;`
- `friend class ObjCDeclContextSwitch;`
- `friend class ParsingOpenACCDirectiveRAII;`
- `friend class ParsingOpenMPDirectiveRAII;`

### Parser.cpp — Removals

**Constructor `Parser::Parser()`:**
- Remove `InMessageExpression(false)`, `ParsingInObjCContainer(false)` from init list
- Remove `CurParsedObjCImpl = nullptr;`
- Remove ObjCTypeQuals initialization block (`if (getLangOpts().ObjC) { ... }`)

**`Parser::Initialize()`:**
- Remove `if (getLangOpts().OpenMP) Actions.OpenMP().startOpenMPLoop();`

**`skipUntilPragmaHarmless()` / similar:**
- Remove `if (OpenMPDirectiveParsing) ...`
- Remove `if (OpenACCDirectiveParsing) ...`

**`ParseExternalDeclaration()`:**
- Remove `ParseOpenMPDeclarativeDirectiveWithExtDecl(...)` branch
- Remove `ParseOpenACCDirectiveDecl(...)` branch
- Remove `ParseObjCAtDirectives(...)` branch
- Remove HLSL `export` block (`if (getLangOpts().HLSL) ...`)
- Remove ObjC `@` keyword branches
- Remove ObjC method definition handling (`if (CurParsedObjCImpl) ...`)

**`ParseDeclarationOrFunctionDefinition()` and related:**
- Remove `ObjCDeclContextSwitch` usage
- Remove ObjC-specific type annotation calls

**`ParseModuleImport()`:**
- Remove `IsObjCAtImport` branch

**Status:** ⬜

---

## Commit 2 — `ParsePragma.cpp`

**File:** `clang/lib/Parse/ParsePragma.cpp`

**Removals:**
- `#include "clang/Sema/SemaCUDA.h"` include
- `struct PragmaNoOpenMPHandler` and `struct PragmaNoOpenACCHandler`
- `struct PragmaOpenMPHandler` and `struct PragmaOpenACCHandler`
- `struct PragmaForceCUDAHostDeviceHandler` and its `HandlePragma()` impl
- `void PragmaNSReturnNotOwned::HandlePragma()` (ObjC) if present
- In `initializePragmaHandlers()`: remove OpenMP, OpenACC, CUDA handler registration
- In `resetPragmaHandlers()`: remove OpenMP, OpenACC, CUDA handler removal
- `HandlePragmaNoSupport()` stubs for omp/acc
- All OpenMP pragma comment handlers (`HandlePragma` impl for OpenMP)
- All OpenACC pragma comment handlers
- CUDA `#pragma unroll` special case

**Estimated removal:** ~1,500 lines

**Status:** ⬜

---

## Commit 3 — Delete dead source files + CMakeLists update

**Files deleted:**
- `clang/lib/Parse/ParseObjc.cpp` (3,340 lines)
- `clang/lib/Parse/ParseOpenMP.cpp` (5,406 lines)
- `clang/lib/Parse/ParseOpenACC.cpp` (1,700 lines)
- `clang/lib/Parse/ParseHLSL.cpp` (348 lines)
- `clang/lib/Parse/ParseHLSLRootSignature.cpp` (1,589 lines)
- `clang/include/clang/Parse/ParseHLSLRootSignature.h`

**CMakeLists.txt update:**
- Remove `ParseObjc.cpp`, `ParseOpenMP.cpp`, `ParseOpenACC.cpp`,
  `ParseHLSL.cpp`, `ParseHLSLRootSignature.cpp` from source list
- Remove `FrontendHLSL` and `FrontendOpenMP` from `LLVM_LINK_COMPONENTS`
- Remove `omp_gen` from `DEPENDS`

**Estimated removal:** ~12,383 lines

**Status:** ⬜

---

## Commit 4 — `ParseDecl.cpp`

**File:** `clang/lib/Parse/ParseDecl.cpp`

**Expected removals:**
- All `if (getLangOpts().ObjC)` guarded blocks
- `ParseObjCBridgeRelatedAttribute()` invocations
- HLSL qualifier parsing (`isHLSLQualifier`, `ParseHLSLQualifiers`)
- `ParseCUDAFunctionAttributes()` call sites
- OpenMP `threadprivate` / `allocate` directive handling
- `DSC_objc_method_result` switch cases in `DeclSpecContext`

**Status:** ⬜

---

## Commit 5 — `ParseDeclCXX.cpp`

**File:** `clang/lib/Parse/ParseDeclCXX.cpp`

**Expected removals:**
- HLSL `export` declaration parsing
- OpenMP `declare` directive branches
- ObjC `@` handling in class context

**Status:** ⬜

---

## Commit 6 — `ParseStmt.cpp`

**File:** `clang/lib/Parse/ParseStmt.cpp`

**Expected removals:**
- `ParseObjCAtStatement()` call site and ObjC statement branches
- `ParseOpenMPDeclarativeOrExecutableDirective()` call sites
- `ParseOpenACCDirectiveStmt()` call sites
- OpenMP `annot_pragma_openmp` token handling
- OpenACC `annot_pragma_openacc` token handling
- ObjC try/catch/throw/synchronized statement branches

**Status:** ⬜

---

## Commit 7 — `ParseExpr.cpp` + `ParseExprCXX.cpp`

**Files:** `clang/lib/Parse/ParseExpr.cpp`, `clang/lib/Parse/ParseExprCXX.cpp`

**Expected removals (ParseExpr.cpp):**
- `ParseObjCBoolLiteral()` call site
- ObjC message expression branches (`[`, `@selector`, `@string`)
- `ParseAssignmentExprWithObjCMessageExprStart()` call sites
- `tryParseOpenMPArrayShapingCastPart()` call sites
- OpenMP array shaping expression handling

**Expected removals (ParseExprCXX.cpp):**
- ObjC generics `<` parsing (`parseObjCTypeArgsOrProtocolQualifiers`)
- HLSL-specific expression handling

**Status:** ⬜

---

## Commit 8 — Remaining parse files

**Files:** `ParseTemplate.cpp`, `ParseTentative.cpp`, `ParseInit.cpp`,
`ParseStmtAsm.cpp`, `ParseCXXInlineMethods.cpp`

**Expected removals:**
- Any OpenMP/ObjC/HLSL conditional branches in these files

**Status:** ⬜

---

## Commit 9 — Final build verification + document update

- Rebuild all affected libraries: `clangBasic`, `clangLex`, `clangParse`,
  `clangFrontend`, `clangSema`
- Confirm zero errors, zero warnings
- Update this document to mark all items ✅
- Update `ReducedParser.md` to reflect actual deletions

**Status:** ⬜

---

## Metrics

| Metric | Value |
|--------|-------|
| Files to fully delete | 5 parse files + 1 header |
| Estimated total lines removed | ~15,000+ lines |
| Libraries rebuilt clean | TBD |

---

## Notes

- **Never add comments where code is removed** — just delete the code.
- **Never leave empty files** — delete them entirely.
- **Rebuild after each commit** to catch breakage early.
- ObjC `@` handling in `ParseModuleImport()` can remain only if it is
  genuinely needed for C++ module imports (it is not — remove it).
- `DSC_objc_method_result` in `DeclSpecContext` can be removed only once
  all call sites in `ParseDecl.cpp` are removed first.
- CUDA `#pragma unroll` special case in `ParsePragma.cpp` (line ~3830) can
  be simplified to drop the `PP.getLangOpts().CUDA` condition.
