// SPDX-License-Identifier: MIT
// Streaming TSV loader for IMDb datasets.
//
// Two public surfaces are exposed:
//
//   1. The thin `load_movies` / `for_each_movie` API requested by the
//      parallel `core-algorithms` session -- kept verbatim for binary
//      compatibility with its MinHeap/RedBlackTree tests.
//
//   2. The rich `StreamMovies` / `LoadMovies` API that lets callers pass
//      structured options (rating/votes thresholds, title-type filter,
//      progress callback, header skip flag). Used by tests and CLI.
//
// Both APIs share the same streaming implementation in src/loader.cpp --
// the file is read line-by-line, parsed, filtered, and dispatched. We
// never hold the full file in memory.

#pragma once

#include "imdb/Movie.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

namespace imdb {

/// Streaming-friendly callback. Returning `false` aborts iteration.
using MovieCallback = std::function<bool(const Movie&)>;

// =============================================================================
// Thin API (core-algorithms contract)
// =============================================================================

/// Read movies from `path` and append them to `out`.
/// `min_rating` keeps rows with rating >= threshold (0 disables).
/// `min_votes`  keeps rows with votes  >= threshold (0 disables).
/// Returns the number of movies appended (i.e. that passed the filter).
std::size_t load_movies(const std::filesystem::path& path,
                        std::vector<Movie>& out,
                        float min_rating = 0.0f,
                        std::uint32_t min_votes = 0);

/// Stream the file invoking `cb` per row that passes the filter.
/// `cb` must be invocable as `bool(const Movie&)` -- returning `false`
/// aborts the stream early.
/// Returns the count of rows that passed the filter.
template <class F>
std::size_t for_each_movie(const std::filesystem::path& path, F&& cb,
                           float min_rating = 0.0f,
                           std::uint32_t min_votes = 0) {
    static_assert(std::is_invocable_r_v<bool, F&, const Movie&>,
                  "callback must be invocable as bool(const Movie&)");
    std::vector<Movie> sink;
    const std::size_t n = load_movies(path, sink, min_rating, min_votes);
    for (const auto& m : sink) {
        if (!cb(m)) break;
    }
    return n;
}

// =============================================================================
// Rich API (foundation-and-data)
// =============================================================================

/// Per-call tuning knobs.
struct LoadOptions {
    /// Keep only rows with rating >= this. 0.0 disables.
    double min_rating = 0.0;

    /// Keep only rows with votes >= this. 0 disables.
    std::int64_t min_votes = 0;

    /// When non-empty, keep only rows whose `titleType` matches one of
    /// these strings (case-sensitive). Empty = keep all types.
    std::vector<std::string> allowed_title_types{};

    /// Optional progress callback invoked every `progress_every` rows.
    std::function<void(std::int64_t rows_seen, std::int64_t rows_kept)>
        on_progress = nullptr;
    std::int64_t progress_every = 100000;

    /// Skip the header row (IMDb dumps ship one). Default ON.
    bool skip_header = true;
};

/// Diagnostics for a load pass.
struct LoadStats {
    std::int64_t rows_seen = 0;        // data rows after header
    std::int64_t rows_kept = 0;        // rows that passed the filter
    std::int64_t parse_errors = 0;     // rows we couldn't parse
    bool aborted_by_visitor = false;
};

/// Visitor signature for the rich streaming API.
using MovieVisitor = std::function<bool(const Movie&)>;

/// Stream movies from `path`. Throws `std::runtime_error` if the file
/// cannot be opened. Other failures (bad rows) are recorded in `stats`
/// and the stream continues.
void StreamMovies(const std::string& path,
                  const LoadOptions& options,
                  MovieVisitor        visitor,
                  LoadStats*          stats = nullptr);

/// Convenience: collect all matching movies into a vector.
/// Memory: O(rows_kept). The parser never holds the full file in memory.
std::vector<Movie> LoadMovies(const std::string& path,
                              const LoadOptions& options,
                              LoadStats*         stats = nullptr);

}  // namespace imdb
