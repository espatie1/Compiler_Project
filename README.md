# IFJ24 Compiler — Documentation

## 1. Purpose

This project is a **single‑pass compiler** for the **IFJ24 language**.  
Input: `.ifj24` source.  
Output: target program in the IFJ virtual machine instruction set (used by `ic24int`).

Main stages:

1. **Scanner (lexer)** → tokens.  
2. **Parser** → Abstract Syntax Tree (AST).  
3. **Semantic checks** → symbol table, types, scopes.  
4. **Code generator** → textual VM code (printed to file or stdout).

## 2. Repository layout

```
Compiler_Project-main/
├─ Makefile                 # build to ./ifj24_compiler
├─ ic24int                  # VM interpreter (binary for tests)
├─ all_in_one.py            # test runner
├─ all_tests/               # sample and negative tests
├─ src/
│  ├─ scanner.[ch]          # lexer
│  ├─ tokens.h              # token kinds (keywords, literals, ops, punctuation)
│  ├─ parser.[ch]           # recursive‑descent parser + semantic layer
│  ├─ ast.[ch]              # AST nodes and builders
│  ├─ symtable.[ch]         # hash‑table symbol table with scopes
│  ├─ codegen.[ch]          # IFJ VM code emitter (functions, control flow, expr)
│  ├─ error.[ch]            # unified error codes/exit
│  └─ utils.[ch]            # safe alloc + pointer registry and cleanup
```

## 3. Build and run

### Build
```bash
make            # produces ./ifj24_compiler  (C99, -Wall -Wextra -pedantic -Werror)
```

### Run
```bash
# read from stdin, write VM code to stdout
./ifj24_compiler

# read from file, write VM code to stdout
./ifj24_compiler program.ifj24

# read from file, write VM code to a file
./ifj24_compiler program.ifj24 output.ifjcode
```

### End‑to‑end execute (compile + interpret)
```bash
./ifj24_compiler program.ifj24 output.txt
./ic24int output.txt              # runs the VM code
```

### Test runner
```bash
python3 all_in_one.py --dir all_tests/ifj24_tests
python3 all_in_one.py --dir all_tests/errors_tests
```

## 4. Language overview (as implemented)

### 4.1 Lexical elements
- **Identifiers**: letters, digits, `_`, not starting with digit.
- **Integer literal**: `i32` range.
- **Float literal**: `f64` (with decimal or exponent).
- **String literal**: double quotes with escapes.
- **Char/byte literal**: fits `u8` where required.
- **Keywords** (subset): `pub`, `fn`, `var`, `const`, `if`, `else`, `while`, `return`, `void`, `i32`, `f64`, `u8`, `null`, `import`.
- **Operators**: `+ - * / == != < <= > >= =` and unary `-`.
- **Punctuation**: `(` `)` `{` `}` `,` `;` `:` `.` `?` `|`.

Comments and whitespace are skipped. The scanner supports single‑line and block comments.

### 4.2 Types and nullability
Primitive types:
- `i32`, `f64`, `u8`, and `void` (for functions with no return).
- Literal `null` and **nullable forms** with `?` (e.g., `i32?`, `f64?`, `u8?`).

**Union‑like** composition with `|` is recognized by the parser for type relations.  
Type inference is supported for `var` when expression type is clear; otherwise error 8.

### 4.3 Declarations and scopes
- **Variables**
  - `var name: Type = expr;` or `var name = expr;` (inference).  
  - `const name: Type = expr;` (must initialize; cannot reassign).
  - Block scope; shadowing controlled via the symbol table.
- **Functions**
  - `pub fn name(params...) : ReturnType { ... }` and non‑public `fn`.
  - Parameters are declared in function scope; types checked on call.
  - A pre‑declaration pass collects all function headers to allow calls before bodies.
- **Import**
  - At program start: `@import "ifj"` (required to enable built‑ins).

### 4.4 Statements
- Block `{ ... }`, variable declaration, assignment, if/else, while, return, expression statement.

### 4.5 Expressions and precedence
Implemented via classic recursive‑descent layers:
1. multiplicative `* /`
2. additive `+ -`
3. relational `< <= > >=`
4. equality `== !=`
5. primary: identifiers, literals, parenthesized, calls.

Type checks perform numeric promotions (`i32 ↔ f64`) and reject invalid mixes.  
Nullable values are checked or unwrapped when needed.

### 4.6 Built‑in functions (via `ifj` import)
Detected and emitted only if used:
- `ifj.readi32() -> i32?`
- `ifj.readf64() -> f64?`
- `ifj.readstr() -> string?`
- `ifj.write(x1, x2, ...) -> void` (prints)
- `ifj.length(s: string) -> i32`
- `ifj.concat(a: string, b: string) -> string`
- `ifj.substring(s: string, start: i32, len: i32) -> string`
- `ifj.strcmp(a: string, b: string) -> i32`
- `ifj.i2f(x: i32) -> f64`
- `ifj.f2i(x: f64) -> i32`
- `ifj.chr(x: i32|u8) -> string` (one‑char string)
- `ifj.ord(s: string) -> i32` (code point of the first char)
- `ifj.string(x) -> string` (constructor/format)

Each has a code‑gen template in `codegen.c`. Many check `null` / bounds at runtime.

## 5. Front end design

### 5.1 Scanner (`scanner.c/.h`)
- Reads from `FILE*` (stdin by default). Tracks **line** and **column**.
- Skips whitespace and comments.
- Returns `Token { type, lexeme, line, column }`.
- Recognizes keywords vs identifiers, numbers, strings, operators and delimiters.
- Reports lexical errors (code **1**) for invalid characters or too long literals.

### 5.2 Parser (`parser.c/.h`)
- **Recursive‑descent** with one‑token lookahead.
- Top‑level: optional `@import`, then function pre‑scan, then full parse into an **AST**.
- Grammar (informal, simplified):
  ```ebnf
  program      := import? function* ;
  import       := '@import' STRING_LITERAL ';' ;
  function     := ('pub')? 'fn' IDENT '(' params? ')' ':' type block ;
  params       := param (',' param)* ;
  param        := IDENT ':' type ;
  block        := '{' stmt* '}' ;
  stmt         := var_decl | const_decl | assign | if | while | ret | expr ';' ;
  var_decl     := ('var'|'const') IDENT (':' type)? '=' expr ';' ;
  assign       := IDENT '=' expr ';' ;
  if           := 'if' '(' expr ')' block ('else' block)? ;
  while        := 'while' '(' expr ')' block ;
  ret          := 'return' expr? ';' ;
  type         := 'i32' | 'f64' | 'u8' | 'void' | type '?' | type '|' type ;
  expr         := equality ;
  equality     := relational (('=='|'!=') relational)* ;
  relational   := additive   (('<'|'>'|'<='|'>=') additive)* ;
  additive     := multiplicative (('+'|'-') multiplicative)* ;
  multiplicative := primary (('*'|'/') primary)* ;
  primary      := literal | IDENT | call | '(' expr ')' ;
  call         := IDENT '(' args? ')' | 'ifj'.IDENT '(' args? ')' ;
  args         := expr (',' expr)* ;
  ```
- **AST** (`ast.[ch]`): nodes for program, function, block, var/const, assign, if, while, return, call, binary op, literal, identifier.

### 5.3 Semantic analysis (`parser.c`, `symtable.*`)
- **Symbol table**: hash table with separate chaining; per‑scope stacks, numeric scope IDs.
- Tracks **variables**, **constants**, **parameters**, **functions** (declared/defined), and **usage**.
- Checks:
  - duplicate declaration in scope (error **5**),
  - use before declaration / missing function (error **3**),
  - wrong argument count/types (error **4**),
  - assignment to `const` (error **5**),
  - return type mismatch or missing/extra return (error **6**),
  - type compatibility in expressions (error **7**),
  - cannot infer `var` type (error **8**),
  - **unused variable** detection at end (error **9**),
  - `main` function presence/signature (checked in `is_main_correct`).
- Nullable: helper `detach_nullable(...)` and conversions when needed.

## 6. Code generation (`codegen.[ch]`)

Target is the **IFJ VM** (stack/frames with `GF/LF/TF`, `PUSHFRAME`, `CALL`, `LABEL`, `JUMP*`, arithmetic ops).

### 6.1 Strategy
- **Two phases** per function: declare frame variables, then emit bodies.
- Expression code is **stack‑oriented**: sub‑expressions push to stack; emitter stores to temp locals (`LF@tmp...`) as needed.
- Control flow uses `LABEL`/`JUMP`, including unique counters from `generate_unique_label()`.
- Built‑ins are emitted **only if used** (`codegen_generate_builtin_functions()`), each as a separate labeled routine.

### 6.2 Mapping (examples)
- Arithmetic `+,-,*,/` → `ADD/SUB/MUL/IDIV` or floating ops; integer divide guarded to avoid float/int mix.
- Comparisons → `LT/GT/EQ` + branching to boolean results.
- `if` → evaluate condition → `JUMPIFEQ/JUMPIFNEQ` to true/false labels.
- `while` → entry label, condition, body, back‑edge.
- Calls → prepare arguments in current frame, `CALL` function label, get return via `POPS`/local `retval`.
- Strings → helper temps; `ifj.*` calls where required (concat, length, substring, chr/ord).

### 6.3 Output
- By default prints to **stdout**; if an output file is given, writes there (`codegen_init()` / `codegen_finalize()` manage the stream).

## 7. Errors and exits (`error.[ch]`)

| Code | Meaning |
|-----:|--------|
| 0    | OK |
| 1    | **Lexical** error |
| 2    | **Syntax** error |
| 3    | Semantic: undefined function/variable |
| 4    | Semantic: wrong function parameters |
| 5    | Semantic: other (redefinition, assign to const, …) |
| 6    | Semantic: return (missing/extra/wrong) |
| 7    | Semantic: type compatibility in expressions |
| 8    | Semantic: cannot infer variable type |
| 9    | Semantic: unused variable |
| 10   | Semantic: other |
| 99   | **Internal** compiler error |

The compiler prints `ERROR <code>: <message>` to stderr and exits with `<code>`. Memory is cleaned before exit.

## 8. Memory management (`utils.[ch]`)

- `safe_malloc/safe_realloc` with checks → calls `error_exit(99, ...)` on failure.
- **Pointer registry**: all allocated pointers can be registered and are freed by `cleanup_pointers_storage()` at the end, to avoid leaks.
- Helpers for string duplication and small utilities used across modules.

## 9. Known limitations

- Grammar is a focused subset; not every IFJ24 feature may be present.
- No user‑defined types; only primitives and nullable/union combinations.
- Runtime for built‑ins is text‑emitted; performance is acceptable for coursework scale.
- Error recovery is minimal (stop at first error).

## 10. Example

```
// minimal example
@import "ifj";

pub fn main(): void {
    var a: i32 = 5;
    var b = 2;          // inferred i32
    var s = "Hi";
    if (a > b) {
        ifj.write(s, "\n");
    }
}
```

Compile and run:
```bash
./ifj24_compiler sample.ifj24 out.txt
./ic24int out.txt
```

- Added tests in `all_tests/` for both valid and error cases.

---

