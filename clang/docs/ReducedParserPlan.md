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

1. Start from the top-level parser entry (`ParseAST.h` / `ParseAST.cpp`).
2. Proceed to the main parser class (`Parser.h` / `Parser.cpp`).
3. Delete dead source files entirely once their declarations are gone.
4. Work through remaining parse files, removing dead branches.
5. Verify the build is clean after each commit.

---

## Status Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Done and verified to compile |
| 🔧 | In progress |
| ⬜ | Not started |

---

## Commit 0 — `ParseAST.h` + `ParseAST.cpp`

**Files:** `clang/include/clang/Parse/ParseAST.h`,
`clang/lib/Parse/ParseAST.cpp`

These files are the top-level entry point to the parser. They contain no
ObjC, OpenMP, OpenACC, HLSL, or CUDA/HIP code. No changes required.

**Status:** ✅ Clean — no modifications needed

---

## Commit 1 — `Parser.h`

**File:** `clang/include/clang/Parse/Parser.h`

### Includes removed (4)

- `#include "clang/Basic/OpenACCKinds.h"`
- `#include "clang/Sema/SemaObjC.h"`
- `#include "clang/Sema/SemaOpenMP.h"`
- `#include "llvm/Frontend/OpenMP/OMPContext.h"`

### Forward declarations removed (8)

- `class InMessageExpressionRAIIObject;`
- `class OMPClause;`
- `class OpenACCClause;`
- `class ObjCTypeParamList;`
- `struct OMPTraitProperty;`
- `struct OMPTraitSelector;`
- `struct OMPTraitSet;`
- `class OMPTraitInfo;`

### Enum removed

- `enum class ObjCTypeQual { ... }` (lines 91–102)

### Table of Contents entries removed (4)

- `// 7. HLSL Constructs (ParseHLSL.cpp)`
- `// 9. Objective-C Constructs (ParseObjc.cpp)`
- `// 10. OpenACC Constructs (ParseOpenACC.cpp)`
- `// 11. OpenMP Constructs (ParseOpenMP.cpp)`

### Scattered declarations removed

- `void ParseCUDAFunctionAttributes(ParsedAttributes &attrs);`
- `bool isHLSLQualifier(const Token &Tok) const;`
- `void ParseHLSLQualifiers(ParsedAttributes &Attrs);`
- `void ParseObjCBridgeRelatedAttribute(...);`
- `void ParseOpenMPAttributeArgs(...);`
- `void ParseHLSLRootSignatureAttributeArgs(...);`
- `bool tryParseOpenMPArrayShapingCastPart();`
- `ExprResult ParseObjCBoolLiteral();`
- `ExprResult ParseAssignmentExprWithObjCMessageExprStart(...);`
- OpenMP token replay helpers in `ParseCXX11AttributeSpecifier`

### `ParsedStmtContext` enum — removed value

- `AllowStandaloneOpenMPDirectives = 0x2`
- Updated `Compound` value to not include OpenMP flag

### DSC enum — removed value

- `DSC_objc_method_result` from `DeclSpecContext`

### Pragma member variables removed

- `std::unique_ptr<PragmaHandler> OpenMPHandler;`
- `std::unique_ptr<PragmaHandler> OpenACCHandler;`
- `std::unique_ptr<PragmaHandler> CUDAForceHostDeviceHandler;`

### Whole sections deleted (~1,900 lines)

- **HLSL Constructs** section (`\name HLSL Constructs`, lines ~5194–5232)
- **Objective-C Constructs** section (`\name Objective-C Constructs`, lines ~5348–6054)
- **OpenACC Constructs** section (`\name OpenACC Constructs`, lines ~6062–6322)
- **OpenMP Constructs** section (`\name OpenMP Constructs`, lines ~6330–7020)

**Status:** ✅

---

## Commit 2 — `Parser.cpp`

**File:** `clang/lib/Parse/Parser.cpp`

### Constructor `Parser::Parser()` — removals

- `InMessageExpression(false)` from initializer list
- `ParsingInObjCContainer(false)` from initializer list
- `CurParsedObjCImpl = nullptr;`
- ObjC type quals initialization block (`if (getLangOpts().ObjC) { ... }`)

### `Parser::Initialize()` — removal

- `if (getLangOpts().OpenMP) Actions.OpenMP().startOpenMPLoop();`

### `skipUntilPragmaHarmless()` — removals

- `if (OpenMPDirectiveParsing) ...` block
- `if (OpenACCDirectiveParsing) ...` block

### `ParseExternalDeclaration()` — removals

- `ParseOpenMPDeclarativeDirectiveWithExtDecl(...)` branch
- `ParseOpenACCDirectiveDecl(...)` branch
- `ParseObjCAtDirectives(...)` branch
- HLSL `export` block (`if (getLangOpts().HLSL) ...`)
- ObjC `@` keyword branches
- ObjC method definition handling (`if (CurParsedObjCImpl) ...`)

### `ParseDeclarationOrFunctionDefinition()` — removals

- `ObjCDeclContextSwitch` usage
- ObjC type annotation calls

### `ParseModuleImport()` — removal

- `IsObjCAtImport` branch

### Type annotation helpers — removals

- ObjC type args handling blocks in `ParseOptionalCXXScopeSpecifier`

**Status:** ✅

---

## Commit 3 — Delete dead source files + CMakeLists.txt

**Files deleted:**

- `clang/lib/Parse/ParseObjc.cpp` (~3,340 lines)
- `clang/lib/Parse/ParseOpenMP.cpp` (~5,406 lines)
- `clang/lib/Parse/ParseOpenACC.cpp` (~1,700 lines)
- `clang/lib/Parse/ParseHLSL.cpp` (~348 lines)
- `clang/lib/Parse/ParseHLSLRootSignature.cpp` (~1,589 lines)
- `clang/include/clang/Parse/ParseHLSLRootSignature.h`

**`clang/lib/Parse/CMakeLists.txt` changes:**

- Remove `ParseObjc.cpp`, `ParseOpenMP.cpp`, `ParseOpenACC.cpp`,
  `ParseHLSL.cpp`, `ParseHLSLRootSignature.cpp` from source list
- Remove `FrontendHLSL` and `FrontendOpenMP` from `LLVM_LINK_COMPONENTS`
- Remove `omp_gen` from `DEPENDS`

**Estimated removal:** ~12,400+ lines

**Status:** ✅

---

## Commit 4 — `ParsePragma.cpp`

**File:** `clang/lib/Parse/ParsePragma.cpp`

### Includes removed

- `#include "clang/Sema/SemaCUDA.h"`

### Struct/handler definitions deleted

- `struct PragmaOpenMPHandler` + `HandlePragma()` impl
- `struct PragmaNoOpenMPHandler` + `HandlePragma()` impl
- `struct PragmaOpenACCHandler` + `HandlePragma()` impl
- `struct PragmaNoOpenACCHandler` + `HandlePragma()` impl
- `struct PragmaForceCUDAHostDeviceHandler` + `HandlePragma()` impl

### `initializePragmaHandlers()` — removals

- OpenMP handler registration
- OpenACC handler registration
- CUDA force-host-device handler registration

### `resetPragmaHandlers()` — removals

- OpenMP handler removal
- OpenACC handler removal
- CUDA handler removal

### OpenMP pragma handling deleted

- All `HandlePragma` impl for `annot_pragma_openmp`

### OpenACC pragma handling deleted

- All `HandlePragma` impl for `annot_pragma_openacc`

**Status:** ⬜

---

## Commit 5 — `ParseDecl.cpp`

**File:** `clang/lib/Parse/ParseDecl.cpp`

### Expected removals

- All `if (getLangOpts().ObjC)` guarded blocks
- `ParseObjCBridgeRelatedAttribute()` call sites and implementation
- `isHLSLQualifier()` + `ParseHLSLQualifiers()` call sites
- `ParseCUDAFunctionAttributes()` call sites
- OpenMP `threadprivate` / `allocate` directive handling
- `DSC_objc_method_result` switch cases

**Status:** ⬜

---

## Commit 6 — `ParseDeclCXX.cpp`

**File:** `clang/lib/Parse/ParseDeclCXX.cpp`

### Expected removals

- HLSL `export` declaration parsing
- OpenMP `declare` directive branches
- ObjC `@` handling in class context
- CUDA attribute handling

**Status:** ⬜

---

## Commit 7 — `ParseStmt.cpp`

**File:** `clang/lib/Parse/ParseStmt.cpp`

### Expected removals

- `ParseObjCAtStatement()` call site and ObjC statement branches
- `ParseOpenMPDeclarativeOrExecutableDirective()` call sites
- `ParseOpenACCDirectiveStmt()` call sites
- `annot_pragma_openmp` token handling
- `annot_pragma_openacc` token handling
- ObjC try/catch/throw/synchronized statement branches
- `AllowStandaloneOpenMPDirectives` references in `ParsedStmtContext`

**Status:** ⬜

---

## Commit 8 — `ParseExpr.cpp` + `ParseExprCXX.cpp`

**Files:** `clang/lib/Parse/ParseExpr.cpp`,
`clang/lib/Parse/ParseExprCXX.cpp`

### `ParseExpr.cpp` removals

- `ParseObjCBoolLiteral()` definition
- ObjC message expression branches (`[`, `@selector`, `@string`)
- `ParseAssignmentExprWithObjCMessageExprStart()` definition and call sites
- `tryParseOpenMPArrayShapingCastPart()` definition and call sites
- OpenMP array shaping expression handling

### `ParseExprCXX.cpp` removals

- ObjC generics `<` parsing (`parseObjCTypeArgsOrProtocolQualifiers`)
- HLSL-specific expression handling

**Status:** ⬜

---

## Commit 9 — Remaining parse files

**Files:** `ParseTemplate.cpp`, `ParseTentative.cpp`, `ParseInit.cpp`,
`ParseStmtAsm.cpp`, `ParseCXXInlineMethods.cpp`

### Expected removals

- Any OpenMP / ObjC / HLSL / CUDA / OpenACC conditional branches in these files

**Status:** ⬜

---

## Commit 10 — Final build verification + document update

- Rebuild: `clangBasic`, `clangLex`, `clangParse`, `clangFrontend`, `clangSema`
- Confirm zero errors, zero warnings related to removals
- Update this document to mark all items ✅

**Status:** ⬜

---

## Metrics

| Metric | Value |
|--------|-------|
| Files to fully delete | 5 `.cpp` + 1 `.h` |
| Dead language lines removed (files deleted) | ~12,400 |
| Dead language lines removed (in-place) | ~3,000+ |
| Total estimated removal | ~15,400+ lines |

---

## Rules

- **Never add comments where code is removed** — just delete the code.
- **Never leave empty files** — delete them entirely.
- **Rebuild after each commit** to catch breakage early.
- Dead code = any code path reachable only when
  `getLangOpts().ObjC`, `getLangOpts().OpenMP`,
  `getLangOpts().OpenACC`, `getLangOpts().HLSL`,
  `getLangOpts().CUDA`, or `getLangOpts().HIP` is true.
