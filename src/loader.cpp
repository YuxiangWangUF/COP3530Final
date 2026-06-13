// SPDX-License-Identifier: MIT
// Implementation of imdb::load_movies / for_each_movie / StreamMovies /
// LoadMovies. See include/imdb/Loader.h for the public API and column
// contract.
//
// TSV column contract (11 columns, header row skipped by default):
//   0  tconst         (e.g. "tt0111161")
//   1  titleType      (e.g. "movie", "short", "tvSeries")
//   2  primaryTitle   (UTF-8)
//   3  originalTitle  (UTF-8)
//   4  isAdult        ("0" or "1")
//   5  startYear      (e.g. "1994" or "\N")
//   6  endYear        (e.g. "\N")
//   7  runtimeMinutes (e.g. "142" or "\N")
//   8  genres         (comma-separated or "\N")
//   9  averageRating  (e.g. "9.2" or "\N")       [optional]
//  10  numVotes       (e.g. "2458234" or "\N")    [optional]
//
// Lines with fewer than 9 columns are skipped and counted as parse errors.
// Lines with 9-10 columns parse successfully but rating/votes default to 0.

#include "imdb/Loader.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace imdb {

namespace {

constexpr std::string_view kMissing = "\\N";

// -----------------------------------------------------------------------------
// Low-level parsing helpers
// -----------------------------------------------------------------------------

void StripBom(std::string& line) {
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line.erase(0, 3);
    }
}

void StripTrailingCR(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

// Split a TSV line into fields with defensive quoted-field handling.
//
// Quoted-field rules (TSV does not require quoting; IMDb's official
// dumps never quote -- we support it so alternate inputs don't break):
//   * A field is "quoted" if it begins with `"`.
//   * Inside a quoted field, `""` (two consecutive quotes) represents
//     a single literal `"`.
//   * A quoted field ends at the first unescaped `"`; the enclosing
//     quotes are stripped from the output.
//
// Empty fields are preserved: "a,\t,b" -> ["a", "", "b"].
std::vector<std::string> SplitTsvLine(const std::string& line) {
    std::vector<std::string> fields;
    fields.reserve(11);

    const char* p   = line.data();
    const char* end = p + line.size();

    while (p <= end) {
        if (p == end) {
            fields.emplace_back();  // trailing empty field
            break;
        }
        if (*p == '"') {
            ++p;  // skip opening quote
            std::string out;
            while (p < end) {
                if (*p == '"') {
                    if (p + 1 < end && *(p + 1) == '"') {
                        out.push_back('"');
                        p += 2;
                    } else {
                        ++p;  // skip closing quote
                        break;
                    }
                } else {
                    out.push_back(*p++);
                }
            }
            fields.push_back(std::move(out));
            // Skip whatever follows the closing quote up to next tab.
            while (p < end && *p != '\t') {
                ++p;
            }
        } else {
            const char* start = p;
            while (p < end && *p != '\t') {
                ++p;
            }
            fields.emplace_back(start, p);
        }
        if (p < end && *p == '\t') {
            ++p;
        }
    }

    return fields;
}

bool IsMissing(const std::string& field) {
    return field == kMissing || field.empty();
}

bool ParseInt(const std::string& field, int& out, int missing_value = 0) {
    if (IsMissing(field)) { out = missing_value; return true; }
    try {
        size_t pos = 0;
        int v = std::stoi(field, &pos);
        if (pos != field.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseUint32(const std::string& field, std::uint32_t& out) {
    if (IsMissing(field)) { out = 0; return true; }
    try {
        size_t pos = 0;
        long long v = std::stoll(field, &pos);
        if (pos != field.size()) return false;
        if (v < 0) return false;
        out = static_cast<std::uint32_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseFloat(const std::string& field, float& out) {
    if (IsMissing(field)) { out = 0.0f; return true; }
    try {
        size_t pos = 0;
        double v = std::stod(field, &pos);
        if (pos != field.size()) return false;
        out = static_cast<float>(v);
        return true;
    } catch (...) {
        return false;
    }
}

const std::string& PickTitle(const std::vector<std::string>& f) {
    if (f.size() > 2 && !f[2].empty()) return f[2];
    if (f.size() > 3)                  return f[3];
    static const std::string kEmpty;
    return kEmpty;
}

bool BuildMovie(const std::vector<std::string>& fields, Movie& out) {
    if (fields.empty() || IsMissing(fields[0])) return false;
    out.tconst        = fields[0];
    out.primary_title = PickTitle(fields);

    if (fields.size() > 5  && !ParseInt(fields[5],  out.start_year))      return false;
    if (fields.size() > 7  && !ParseInt(fields[7],  out.runtime_minutes, -1)) return false;
    if (fields.size() > 9  && !ParseFloat(fields[9],  out.rating))          return false;
    if (fields.size() > 10 && !ParseUint32(fields[10], out.num_votes))      return false;

    if (fields.size() > 8 && !IsMissing(fields[8])) {
        out.genres = fields[8];
    }
    return true;
}

// -----------------------------------------------------------------------------
// Streaming implementation
// -----------------------------------------------------------------------------

void StreamMoviesImpl(std::istream& in,
                      const LoadOptions& options,
                      MovieVisitor       visitor,
                      LoadStats&         stats) {
    std::string line;
    line.reserve(1024);

    bool first_line = options.skip_header;
    std::int64_t since_progress = 0;

    while (std::getline(in, line)) {
        StripTrailingCR(line);
        StripBom(line);

        if (line.empty()) continue;

        if (first_line) {
            // We don't strictly need the header -- column positions are
            // hard-coded by contract -- but we consume it so we don't
            // accidentally parse "tconst" as a tconst.
            (void)SplitTsvLine(line);
            first_line = false;
            continue;
        }

        ++stats.rows_seen;

        auto fields = SplitTsvLine(line);
        if (fields.size() < 9) {
            ++stats.parse_errors;
            continue;
        }

        if (!options.allowed_title_types.empty()) {
            const std::string& tt = fields[1];
            bool ok = false;
            for (const auto& allow : options.allowed_title_types) {
                if (tt == allow) { ok = true; break; }
            }
            if (!ok) continue;
        }

        Movie m;
        if (!BuildMovie(fields, m)) {
            ++stats.parse_errors;
            continue;
        }

        if (options.min_rating > 0.0 && m.rating    < options.min_rating) continue;
        if (options.min_votes  > 0    && m.num_votes < options.min_votes)  continue;

        ++stats.rows_kept;
        if (visitor && !visitor(m)) {
            stats.aborted_by_visitor = true;
            break;
        }

        if (options.on_progress &&
            options.progress_every > 0 &&
            (++since_progress) >= options.progress_every) {
            options.on_progress(stats.rows_seen, stats.rows_kept);
            since_progress = 0;
        }
    }

    if (options.on_progress) {
        options.on_progress(stats.rows_seen, stats.rows_kept);
    }
}

void StreamMoviesImplWithFloats(std::istream& in,
                                float min_rating, std::uint32_t min_votes,
                                MovieVisitor       visitor,
                                LoadStats&         stats) {
    LoadOptions opts;
    opts.min_rating = static_cast<double>(min_rating);
    opts.min_votes  = static_cast<std::int64_t>(min_votes);
    StreamMoviesImpl(in, opts, std::move(visitor), stats);
}

}  // namespace

// =============================================================================
// Public API
// =============================================================================

void StreamMovies(const std::string& path,
                  const LoadOptions& options,
                  MovieVisitor        visitor,
                  LoadStats*          stats) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("StreamMovies: cannot open '" + path + "'");
    }

    LoadStats local;
    StreamMoviesImpl(in, options, std::move(visitor), local);
    if (stats) *stats = local;
}

std::vector<Movie> LoadMovies(const std::string& path,
                              const LoadOptions& options,
                              LoadStats*         stats) {
    std::vector<Movie> out;
    out.reserve(1024);
    StreamMovies(path, options,
                 [&out](const Movie& m) {
                     out.push_back(m);
                     return true;
                 },
                 stats);
    return out;
}

std::size_t load_movies(const std::filesystem::path& path,
                        std::vector<Movie>& out,
                        float min_rating,
                        std::uint32_t min_votes) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return 0;

    LoadStats stats;
    std::size_t before = out.size();
    StreamMoviesImplWithFloats(in, min_rating, min_votes,
        [&out](const Movie& m) {
            out.push_back(m);
            return true;
        },
        stats);
    return out.size() - before;
}

}  // namespace imdb
