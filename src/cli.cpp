// SPDX-License-Identifier: MIT
// IMDb Top-K CLI driver.
//
// Reads a title.basics-style TSV (IMDb dump format), applies a vote /
// rating filter, and prints the Top-K movies ranked by Movie::operator<
// (rating desc, then votes desc, then year asc, then title asc, then
// tconst asc -- the strict total order defined in Movie.h).
//
// Usage:
//   imdb-topk --data <path.tsv> [--top N] [--min-votes V] [--min-rating R]
//
// Implementation choice -- Top-K via bounded heap, NOT full sort.
//   * Heap size is bounded by K.  Space: O(K)  vs  O(N) for sort-then-slice.
//   * One streaming pass over the input; we never hold the full file
//     in memory.  Time: O(N log K)  vs  O(N log N) for full sort.
//
// Exit codes:
//   0   success
//   1   could not open input file
//   2   invalid command-line arguments

#include "imdb/Loader.h"
#include "imdb/Movie.h"
#include "imdb/TopK.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct CliOptions {
    std::size_t   top_k        = 250;
    std::uint32_t min_votes    = 1000;
    float         min_rating   = 0.0f;
    std::string   data_path    = "data/sample.tsv";
    bool          show_help    = false;
    bool          quiet        = false;   // suppress stderr summary
};

void PrintUsage(std::ostream& os, const char* prog) {
    os <<
R"(USAGE:
    )" << prog << R"( --data <PATH> [--top N] [--min-votes V] [--min-rating R] [--quiet]

DESCRIPTION:
    Print the top-N IMDb movies (ranked by Movie::operator<) from a
    title.basics.tsv-style input.  Streaming Top-K: O(N log K) time,
    O(K) extra memory.  Heap size is bounded by N; the full file is
    never held in memory.

OPTIONS:
    --data PATH        Path to the input TSV (IMDb title.basics dump
                       format; 11 columns, header row skipped).
                       [required]
    --top N            Number of movies to print.           [default: 250]
    --min-votes V      Skip movies with fewer than V votes. [default: 1000]
    --min-rating R     Skip movies with rating below R.     [default: 0.0]
    --quiet, -q        Suppress the stderr summary line.
    --help, -h         Show this help and exit.

EXAMPLE:
    )" << prog << R"( --data data/title.basics.tsv --top 250 --min-votes 1000
)";
}

CliOptions ParseArgs(int argc, char** argv) {
    CliOptions opts;
    bool data_set = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need_value = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "[" << argv[0] << "] missing value for " << flag << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if      (a == "--data")       { opts.data_path  = need_value("--data");   data_set = true; }
        else if (a == "--top")        { opts.top_k      = std::stoull(need_value("--top")); }
        else if (a == "--min-votes")  { opts.min_votes  = static_cast<std::uint32_t>(std::stoul(need_value("--min-votes"))); }
        else if (a == "--min-rating") { opts.min_rating = std::stof(need_value("--min-rating")); }
        else if (a == "--quiet" || a == "-q") { opts.quiet = true; }
        else if (a == "--help" || a == "-h")   { opts.show_help = true; }
        else {
            std::cerr << "[" << argv[0] << "] unknown flag: " << a << "\n";
            PrintUsage(std::cerr, argv[0]);
            std::exit(2);
        }
    }
    if (!data_set && !opts.show_help) {
        std::cerr << "[" << argv[0] << "] --data PATH is required\n";
        PrintUsage(std::cerr, argv[0]);
        std::exit(2);
    }
return opts;
}

void PrintTable(const std::vector<imdb::Movie>& rows, std::ostream& os) {
    os << "rank\ttitle\tyear\trating\tvotes\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& m = rows[i];
        os << (i + 1) << '\t'
           << m.primary_title << '\t'
           << m.start_year << '\t'
           << std::fixed << std::setprecision(1) << m.rating << '\t'
           << m.num_votes << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    const CliOptions opts = ParseArgs(argc, argv);
    if (opts.show_help) { PrintUsage(std::cout, argv[0]); return 0; }

    try {
        const auto t0 = std::chrono::steady_clock::now();
        imdb::LoadStats stats;
        std::vector<imdb::Movie> rows = imdb::top_k(
            opts.data_path, opts.top_k, opts.min_votes, opts.min_rating, &stats);
        const auto t1 = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        if (!opts.quiet) {
            std::cerr << "[" << argv[0] << "] source=" << opts.data_path
                      << "  top=" << opts.top_k
                      << "  min_votes=" << opts.min_votes
                      << "  min_rating=" << opts.min_rating
                      << "  rows_seen=" << stats.rows_seen
                      << "  rows_kept=" << stats.rows_kept
                      << "  parse_errors=" << stats.parse_errors
                      << "  time_ms=" << std::fixed << std::setprecision(2) << ms
                      << '\n';
        }

        PrintTable(rows, std::cout);
        // Flush explicitly so that downstream consumers connected via
        // popen (e.g. the integration test in tests/test_cli.cpp) get
        // the table without having to wait for buffer-drain on exit.
        std::cout.flush();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[" << argv[0] << "] error: " << e.what() << "\n";
        return 1;
    }
}