# Foundation Design Notes

This document records the design decisions baked into the
`foundation-and-data` layer of the IMDb Top-250 recommender, plus the
context that drove them. Read this before changing the loader, the
`Movie` struct, or the build configuration.

## 1. Why C++17

* `std::filesystem` -- portable temp-directory handling in tests
  (`fs::temp_directory_path()`), and clean path joins without dragging
  in `<sys/stat.h>` per platform.
* Structured bindings -- used in the TSV parser to keep column-index
  intent visible.
* `if constexpr` -- not strictly needed today, but lets future generic
  parser code (e.g. column-type dispatch) compile cleanly without
  SFINAE gymnastics.
* `inline` variables -- a single header-only `kMissing` literal without
  ODR headaches.
* `std::atomic` -- counters in the temp-file test helper without
  dragging in `<pthread.h>` on Windows.

C++20 would give us `std::span` and modules, but at the time of this
write MSVC 19.50 / CMake 4.2 are stable on C++17 and GCC 9+ (CI matrix)
already ship it; jumping to C++20 would have bought little and risked
breaking the Visual Studio Build Tools 18.5 default toolset.

## 2. Why streaming, line-by-line

The IMDb `title.basics.tsv` dump is ~700 MB compressed and ~6 GB
uncompressed -- 11 million rows. We must NOT load it into a
`std::vector<std::string>` and split it.

The loader reads one line at a time with `std::getline`, parses the
eleven columns on the stack, fills a `Movie`, and forwards it to the
caller. Memory is bounded by:

* one input line (typically <1 KiB; we `reserve(1024)` once),
* one `Movie` (a few dozen bytes),
* the output collection that the caller chose.

`LoadMovies(path, options)` is a convenience wrapper that streams into
a `std::vector` -- the vector is the caller's responsibility, the
parser itself never holds the whole file.

`StreamMovies(path, options, visitor, stats)` is the canonical API.
A visitor that returns `false` aborts early, which is what the
Top-K / Top-250 path uses to skip the rest of the dump once enough
candidates are gathered.

## 3. Why a defensive TSV parser

The official IMDb dumps are tab-separated, unquoted, and use `\N` for
missing. We support more than that anyway:

* **Quoted fields** -- IMDb does not currently quote, but the `akas`
  and `episode` files do, and a generic loader that breaks on those
  is a maintenance trap.
* **Doubled-quote escape** -- standard CSV/TSV convention; a `""`
  inside a quoted field represents a literal `"`.
* **Trailing empty fields** -- preserved (`"a\tb\t"` -> `["a","b",""]`),
  so downstream code can detect a column count mismatch.
* **UTF-8 BOM stripping** -- notepad on Windows saves BOMs; the parser
  silently drops the first one.
* **CRLF tolerance** -- the parser `strip`s trailing `\r` so a file
  saved with Windows line endings parses identically.

## 4. Field choices in `Movie`

The struct is the contract between the loader and every consumer
(MinHeap, RedBlackTree, CLI). It mirrors the IMDb dump columns so the
loader can fill it directly without remapping:

| Field             | Type           | Meaning                                          |
| ----------------- | -------------- | ------------------------------------------------ |
| `tconst`          | `std::string`  | IMDb primary key (`tt\d+`). Empty when missing.  |
| `primary_title`   | `std::string`  | `primaryTitle` from `title.basics.tsv` (UTF-8). |
| `start_year`      | `int` (0)      | `startYear`; `0` means unknown.                  |
| `runtime_minutes` | `int` (-1)     | `runtimeMinutes`; `-1` means unknown.            |
| `genres`          | `std::string`  | comma-separated genre list. Empty == unknown.    |
| `rating`          | `float` (0.0f) | `averageRating` from `title.ratings.tsv`.        |
| `num_votes`       | `uint32_t` (0) | `numVotes` from `title.ratings.tsv`.             |

Defaults are chosen so an `EXPECT_EQ` against a fully-default `Movie`
succeeds in tests without explicit initialization.

`operator<` is defined as a hidden friend inside the struct -- "better
movie first" (higher rating, more votes, earlier year, lex title, lex
`tconst`). This lets the Top-K heap use `MinHeap<Movie,
std::greater<Movie>>` directly without bespoke comparators.

## 5. Build / strict warnings

`-Wall -Wextra -Wpedantic -Werror` are GCC/Clang flags. On MSVC we
map them to `/W4 /WX /permissive-`. Either path produces zero
warnings on a clean tree.

The `-Werror` is **stripped** from the GoogleTest and GoogleBenchmark
targets after `FetchContent_MakeAvailable`, because both projects
deliberately emit warnings we don't own (e.g. `[[deprecated]]` inside
gtest internals, missing-field-initializers in gbench). This is the
industry-standard pattern; we keep our own code under the strict
regime without inheriting upstream's noise.

## 6. FetchContent via zipball

`github.com` over plain HTTPS is reachable on this host but `git://`
clone fails (TCP reset on port 443 from `20.205.243.166`); `git://`
goes through a different network path that is blocked. `codeload.
github.com` and `raw.githubusercontent.com` work fine, so we use
zipball downloads:

```
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
```

`DOWNLOAD_EXTRACT_TIMESTAMP TRUE` opts into CMake policy CMP0135's
NEW behavior so a re-extraction forces a rebuild -- safer when the
upstream archive's mtimes predate the host's clock.

## 7. Cross-task coordination

The `core-algorithms` and `foundation-and-data` tasks run in parallel
sessions on the same repo tree. Both touch the shared files
`Movie.h`, `Loader.h`, `loader.cpp`, `CMakeLists.txt`. The contract we
settled on (via `mavis communication send`) is:

* Field names on `Movie` are `tconst`, `primary_title`, `start_year`,
  `runtime_minutes`, `genres`, `rating`, `num_votes`. They mirror the
  IMDb dump so the loader can fill the struct directly.
* `Loader.h` exposes both the thin `load_movies` / `for_each_movie`
  API used by the algorithm layer and the rich `StreamMovies` /
  `LoadMovies` / `LoadOptions` API used by tests and CLI.
* `tests/test_loader.cpp` belongs to `foundation-and-data`.
* `tests/test_minheap.cpp` / `tests/test_rbtree.cpp` belong to
  `core-algorithms`.
* `tests/CMakeLists.txt` (the small `add_test` style) was written by
  `core-algorithms` and bypassed by our top-level `CMakeLists.txt`,
  which inlines all three test executables via `gtest_discover_tests`.
* All source files in this task use **ASCII-only** comments and
  string literals where possible, to survive the cp936 console
  rendering on the shared Windows host -- non-ASCII bytes that leak
  into source files get rewritten as U+FFFD on read.

If you change a shared file in this task, ping the `core-algorithms`
session first.

## 8. Sample fixture

`data/sample.tsv` is committed. It contains 5 rows designed to cover
the test matrix:

* 3 strong candidates (rating >= 9.0, votes >= 1.9M) -- exercise the
  happy path and the `SampleFixture` integration test.
* 1 hidden gem (rating 9.1, votes only 50) -- designed to be dropped
  by `min_votes >= 1000`, proving the filter actually filters.
* 1 row with a quoted title containing a literal comma and missing
  runtime / genres -- exercises the quote-escape path and the
  `\N`-as-default path.
* 1 row with a Chinese UTF-8 title -- exercises the multi-byte path
  with bytes hard-coded as hex so the test is encoding-agnostic at the
  source level.

The full IMDb dumps are downloaded separately (see `data/README.md`).
