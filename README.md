# SJTU Compilers — a Tiger compiler in C++

> A complete compiler for the **Tiger** language — an independent, from-skeleton
> implementation of **SE3355 Compilers (编译原理)** (Shanghai Jiao Tong University,
> IPADS), part of a [csdiy.wiki](https://csdiy.wiki/) full-catalog build.

![status](https://img.shields.io/badge/status-complete-brightgreen)
![language](https://img.shields.io/badge/C++-informational)
![license](https://img.shields.io/badge/license-MIT-blue)

## Overview

SJTU's compilers course follows Andrew Appel's *Modern Compiler Implementation*
(the "Tiger book"). Across five labs you build a full compiler for **Tiger**, a
small statically-typed language with records, arrays, nested functions, and
first-class strings. This repository implements every lab end-to-end: a
straight-line-program interpreter, a lexer, a parser producing an abstract
syntax tree, a type checker, escape analysis, and a code generator that lowers
Tiger to native x86-64 (via LLVM IR) so the compiled programs actually run.

The course's official framework is Docker-only (it uses the non-standard
`flexc++`/`bisonc++` tools and its `src/tiger/` sources are gated behind TA
access). This repo instead builds the same compiler from scratch with the
**standard `flex` + `bison`** toolchain, matching the course's reference output
**byte-for-byte**, and is verified with the course's own `scripts/grade.sh`.

## Results (measured on WSL2 Ubuntu 24.04, flex 2.6.4 / bison 3.8.2 / g++ 13.3 / llc 18)

Every lab scores **100** on the course's own grader (`scripts/grade.sh`):

| Lab | What it does | Result (measured) |
|---|---|---|
| Lab 1 — Straight-line program | Tree-walk interpreter + max-args analysis | **100/100** (7/7 tests) |
| Lab 2 — Lexer | `flex` scanner → reference token stream | **100/100** (54/54 testcases) |
| Lab 3 — Parser | `bison` grammar → AST pretty-print | **100/100** (51/51 testcases) |
| Lab 4 — Type checking | Semantic analysis, reference error messages | **100/100** (51/51 testcases) |
| Lab 5 — Escape + codegen | Escape analysis → LLVM IR → native x86-64 | **100/100** (17 programs, incl. stdin) |

```
$ scripts/grade.sh all
========== Lab1 Test ==========
LAB1 SCORE: 100
========== Lab2 Test ==========
LAB2 SCORE: 100
========== Lab3 Test ==========
LAB3 SCORE: 100
========== Lab4 Test ==========
LAB4 SCORE: 100
========== Compiler Test ==========
Pass bsearch ... Pass queens ... Pass merge/test1..4 ... Pass tlink
[^_^]: Pass
LAB TOTAL SCORE: 100
```

The generated compiler produces real, runnable native executables. For example,
the 8-queens program (`queens.tig`) is compiled to x86-64 assembly, linked with
the C runtime, and prints all 92 solutions; `merge.tig` reads integers from
stdin and merges two sorted lists; `tfact.tig` computes `10! = 3628800`. Full
captured grader output is in [`results/`](results/).

## Implemented assignments

- [x] **Lab 1 — Straight-line program interpreter** — `MaxArgs`/`Interp` over the
  SLP AST, a persistent value environment, nested-`print` argument analysis.
- [x] **Lab 2 — Lexer** — `flex` scanner: keywords, operators, nested comments,
  string escapes (`\n \t \ddd \^C`, line continuations), 1-based char positions
  robust to CRLF sources.
- [x] **Lab 3 — Parser** — `bison` grammar building the full Tiger AST, with the
  reference desugarings (`&`/`|` → `if`, unary `-` → `0-e`) and a pretty-printer
  matching the course's node names exactly.
- [x] **Lab 4 — Type checking** — scoped type/value environments, structural
  type equality (`nil` ~ record), and the reference diagnostic messages.
- [x] **Lab 5 — Escape analysis & LLVM translation + code generation** — escape
  analysis, then AST → LLVM IR with nested-function static links, records,
  arrays, strings, and control flow; `llc` lowers it to native assembly.

## Project structure

```
sjtu-compilers/
├── CMakeLists.txt              # standard flex/bison build
├── src/
│   ├── straightline/           # Lab 1: SLP interpreter
│   └── tiger/
│       ├── symbol/             # interned symbols + scoped tables
│       ├── errormsg/           # file:line.col error reporting
│       ├── absyn/              # AST + reference pretty-printer
│       ├── lex/                # Lab 2: tiger.lex (flex)
│       ├── parse/              # Lab 3: tiger.y (bison) + driver
│       ├── types/ env/         # semantic types + base environments
│       ├── semant/             # Lab 4: type checker
│       ├── escape/             # Lab 5: escape analysis
│       ├── translate/          # Lab 5: AST -> LLVM IR
│       ├── runtime/            # runtime.c (Tiger stdlib)
│       └── main/               # test_lex/test_parse/test_semant/tiger-compiler
├── testdata/                   # course testcases + reference outputs (LF)
├── scripts/grade.sh            # the course's own grader
└── results/                    # captured grader output
```

## How to run

Prerequisites (Linux / WSL2): `flex`, `bison`, `g++` (C++17), `cmake`, and LLVM's
`llc` (`clang`/`llvm` package) for Lab 5 code generation.

```bash
# Build every target
mkdir -p build && cd build && cmake .. && make -j

# Grade a single lab or all of them (from the repo root)
scripts/grade.sh lab1     # SLP interpreter
scripts/grade.sh lab2     # lexer
scripts/grade.sh lab3     # parser
scripts/grade.sh lab4     # type checker
scripts/grade.sh lab5     # full compiler (codegen + run)
scripts/grade.sh all      # everything

# Compile and run a Tiger program directly
./build/tiger-compiler testdata/lab5or6/testcases/queens.tig   # -> queens.tig.s
gcc -no-pie -m64 queens.tig.s src/tiger/runtime/runtime.c -o queens
./queens
```

## Verification

Verification is the course's own `scripts/grade.sh`, which diffs each tool's
output against the provided reference outputs in `testdata/`:

- **Labs 1–4** diff `test_slp` / `test_lex` / `test_parse` / `test_semant`
  output against the reference token streams, ASTs, and error messages.
- **Lab 5** compiles each `.tig` program to assembly, links it with `runtime.c`
  (using `-Wl,--wrap,getchar` for the stdin-driven `merge` case), runs it, and
  diffs the program's actual output against the reference.

All scores are **100/100**; see [`results/grade_all.txt`](results/grade_all.txt)
and the per-lab logs in [`results/`](results/).

## Tech stack

- **C++17** for all compiler passes.
- **flex 2.6.4** (lexer) and **bison 3.8.2** (parser) — the standard toolchain,
  in place of the course's Docker-only flexc++/bisonc++.
- **LLVM (`llc` 18)** as the Lab 5 backend: the compiler emits textual LLVM IR,
  which `llc` lowers to native x86-64 assembly.
- **CMake** build; a small **C** runtime (`runtime.c`).

## Key ideas / what I learned

- Building an AST whose pretty-printer reproduces the reference output exactly
  (node names, indentation, operator spellings like `LESSTHAN`/`NOTEQUAL`).
- Position bookkeeping in the lexer: 1-based character offsets that stay stable
  across CRLF vs LF sources, and the reference's `(null)` rendering of the empty
  string literal.
- The classic Appel type checker: mutually-recursive type/function declaration
  groups, `nil` vs record typing, and cycle detection.
- Escape analysis and its consumer: threading a **static link** so nested Tiger
  functions can read their enclosing frames' variables.
- Lowering a high-level language to **LLVM IR** by hand — heap frames, `phi`
  nodes with correct predecessors, and a C runtime for the standard library.

## Credits & license

Based on the labs of **SE3355 Compilers (编译原理)** at Shanghai Jiao Tong
University (IPADS), which follow Andrew W. Appel's *Modern Compiler
Implementation in C* (the Tiger book). The straight-line-program skeleton and
all test data come from the public course framework
(`east-china-normal-university_ttb_cs/tiger-compiler-25sp`). This repository is
an independent educational reimplementation; all course materials and
specifications belong to their original authors. Original code here is released
under the [MIT License](LICENSE).
