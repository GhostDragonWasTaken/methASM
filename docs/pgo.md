# Profile-guided optimization

`--pgo` measures which functions your program calls most, then tells the
optimizer. It does that by running `main()` at compile time. There is no
instrumented build and no training run.

```bash
mettle --pgo --release --build program.mettle
```

`--pgo` implies `-O`.

## What it does

The compiler interprets `main()` in the same
[compile-time interpreter](testing.md) that runs `mettle test`, counting calls
as it goes. Any function whose count clears the threshold is treated as hot,
and a hot callee bypasses the inliner's static size budget the same way an
explicit `@inline` does.

The interpretation is deterministic and sandboxed. It reads no files and
touches no devices.

## What it prints

```text
pgo: interpreted main() at compile time - 17053 steps (ran to completion), 9
functions touched, hot threshold 1024 calls
pgo:   helper: 1000 calls
pgo:   print: 1 calls
pgo:   get_stdout: 1 calls
pgo:   fwrite: 1 calls
pgo:   println: 1 calls
```

The first line says how far it got. "ran to completion" means the whole of
`main()` was interpreted. A program that reaches something the interpreter
cannot model stops there, and the counts up to that point are still used.

## The threshold

`METTLE_PGO_HOT` sets the call count that makes a function hot:

```bash
METTLE_PGO_HOT=100 mettle --pgo --release --build program.mettle
```

Lower it to inline more, raise it to inline less.

## Seeing the effect

Pair it with [`--explain`](compilation.md) to watch inlining decisions change:

```bash
mettle --pgo --release --explain --build program.mettle
```

```text
main (call to `helper` @ line 5): inlined  [inlined]
```

Build the same file without `--pgo` and compare. A callee that was refused for
size and is now inlined is what `--pgo` bought.

## When it helps

It helps when the hot path runs through a callee just over the inliner's size
budget, which the static heuristic has no way to know is hot. It does nothing
when `main()` does not exercise the real workload, since the counts come from
that one interpreted run.

A program whose real hot path depends on input the interpreter never sees will
measure the wrong thing. Give `main()` a representative default path if you
want `--pgo` to be worth turning on.

## Measuring a real run instead

Where `--pgo` guesses from an interpreted `main()`, `--pgo-gen` and
`--pgo-use` measure the program actually running on real input.

```bash
mettle --release --pgo-gen --build program.mettle -o train.exe
METTLE_PROFILE_OUT=program.mprof ./train.exe <your real input>
mettle --release --pgo-use=program.mprof --build program.mettle -o program.exe
```

The `--pgo-gen` build counts how many times each basic block runs and writes
the counts to the file named by `METTLE_PROFILE_OUT`, defaulting to
`mettle.mprof`. It is much slower than a normal build, because every block
carries a counter; it is for the training run only.

`--pgo-use` reads those counts back and hands them to the same consumers
`--pgo` feeds: the inliner, the loop unroller, the prefetcher and the
deadline analysis. Both flags imply `-O`, and they are mutually exclusive.

The counts are joined to your source by location, so the optimized build does
not have to match the instrumented one instruction for instruction. Editing
the program between the two steps is fine; blocks whose line moved simply
stop matching and fall back to the static heuristic.

This is worth reaching for when `main()` cannot exercise the real workload,
which is exactly the case `--pgo` says it does nothing for. It costs a build
and a run, so measure whether it pays for your program rather than assuming:
on a suite of sixteen application benchmarks it landed inside the noise, and
more aggressive use of the counts to widen inlining measured slower.

## See also

- [Compile-time execution](testing.md)
- [Compilation](compilation.md)
- [Translation validation](translation-validation.md)
