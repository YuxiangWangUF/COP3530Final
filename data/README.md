# Data

This directory holds IMDb-derived data used by the recommender.

## Layout

| File             | Status   | Purpose                                                |
| ---------------- | -------- | ------------------------------------------------------ |
| `sample.tsv`     | shipped  | 5-row synthetic dataset used by the unit tests         |
| `title.basics.tsv`  | NOT shipped | full IMDb title dump, downloaded manually (see below) |
| `title.ratings.tsv` | NOT shipped | full IMDb rating dump, downloaded manually (see below) |

Anything matching `*.tsv` or `*.tsv.gz` is git-ignored (see top-level
`.gitignore`). `sample.tsv` is committed so tests can run in CI without
network access.

## Source

The official IMDb datasets live at:

* <https://datasets.imdbws.com/title.basics.tsv.gz>  (~700 MB compressed)
* <https://datasets.imdbws.com/title.ratings.tsv.gz> (~25 MB compressed)

They are released for **personal and non-commercial use only** under IMDb's
terms. They are NOT part of this repository.

### Download

```powershell
# from project root
Invoke-WebRequest -Uri "https://datasets.imdbws.com/title.basics.tsv.gz" `
                  -OutFile "data\title.basics.tsv.gz"
Invoke-WebRequest -Uri "https://datasets.imdbws.com/title.ratings.tsv.gz" `
                  -OutFile "data\title.ratings.tsv.gz"

# Decompress — PowerShell ships gzip support since 5.0
Get-ChildItem data\*.tsv.gz | ForEach-Object {
    $out = $_.FullName -replace '\.gz$', ''
    # .NET gunzip (works on Windows + Linux PowerShell)
    $gz = [System.IO.File]::OpenRead($_.FullName)
    $gs = New-Object System.IO.Compression.GZipStream($gz, [System.IO.Compression.CompressionMode]::Decompress)
    $fs = [System.IO.File]::Create($out)
    $gs.CopyTo($fs); $fs.Close(); $gs.Close(); $gz.Close()
}
```

> **Note**: `title.basics.tsv` does NOT contain rating data. To get a Top-250
> list, join `title.basics.tsv` with `title.ratings.tsv` on `tconst` and emit
> one row per match. The Loader accepts the joined 11-column TSV directly
> (column contract is documented in `include/imdb/Loader.h` and
> `src/loader.cpp`).

## Column contract (used by the Loader)

| Index | Column          | Type    | Notes                                 |
| ----- | --------------- | ------- | ------------------------------------- |
| 0     | `tconst`        | string  | IMDb primary key (`tt\d+`)            |
| 1     | `titleType`     | string  | `movie`, `short`, `tvSeries`, ...     |
| 2     | `primaryTitle`  | string  | UTF-8                                 |
| 3     | `originalTitle` | string  | UTF-8, often identical to primary     |
| 4     | `isAdult`       | "0"/"1" |                                       |
| 5     | `startYear`     | year    | `\N` if unknown                       |
| 6     | `endYear`       | year    | `\N` if still running or unknown      |
| 7     | `runtimeMinutes`| int     | `\N` if unknown                       |
| 8     | `genres`        | string  | comma-separated, `\N` if unknown      |
| 9     | `averageRating` | float   | optional; `\N`/missing -> 0.0         |
| 10    | `numVotes`      | int     | optional; `\N`/missing -> 0           |

Missing fields in the source are encoded by the literal string `\N`. The
Loader treats `\N` and an absent column identically (default 0 / empty).

## `sample.tsv`

5 representative rows committed for tests:

* 3 strong candidates (rating ≥ 9.0, votes ≥ 1.9 M)
* 1 hidden gem (rating 9.1 but votes only 50) — designed to be filtered
  out by any sensible `min_votes` threshold
* 1 row with quoted title containing a literal comma, missing runtime /
  genres — exercises the quote-escape and missing-field paths
* 1 row with a Chinese UTF-8 title — exercises the multi-byte path

All five rows are checked into git; everything else is gitignored.
