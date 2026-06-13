// SPDX-License-Identifier: MIT
// Unit tests for imdb::Loader.
//
// Comment policy: ASCII-only on purpose -- a shared Windows host's
// cp936 console rendering would corrupt UTF-8 bytes in source files
// (PowerShell rendering, not the bytes themselves, but tooling that
// re-saves through cp936 will turn U+2014 etc. into U+FFFD).

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "imdb/Loader.h"
#include "imdb/Movie.h"

using imdb::LoadMovies;
using imdb::LoadOptions;
using imdb::LoadStats;
using imdb::Movie;
using imdb::StreamMovies;

namespace fs = std::filesystem;

namespace {

// RAII temp TSV -- writes `contents` to a uniquely named file under
// the system temp dir, removes it on destruction.
class TempTsv {
public:
    explicit TempTsv(const std::string& contents) {
        static std::atomic<unsigned> counter{0};
        const unsigned n = counter.fetch_add(1);
        path_ = (fs::temp_directory_path() /
                 ("imdb_loader_test_" + std::to_string(n) + ".tsv")).string();
        std::ofstream out(path_, std::ios::binary);
        if (!out) throw std::runtime_error("cannot write " + path_);
        out.write(contents.data(),
                  static_cast<std::streamsize>(contents.size()));
        out.close();
    }
    ~TempTsv() {
        std::error_code ec;
        fs::remove(path_, ec);  // best-effort cleanup
    }

    TempTsv(const TempTsv&) = delete;
    TempTsv& operator=(const TempTsv&) = delete;

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

TempTsv MakeTsv(std::initializer_list<std::string> rows) {
    std::string body;
    for (const auto& r : rows) body += r;
    return TempTsv(body);
}

LoadOptions NoFilter() {
    LoadOptions o;
    o.skip_header = true;
    o.progress_every = 0;
    return o;
}

constexpr const char kHeader[] =
    "tconst\ttitleType\tprimaryTitle\toriginalTitle\tisAdult\t"
    "startYear\tendYear\truntimeMinutes\tgenres\t"
    "averageRating\tnumVotes\n";

}  // namespace

// =============================================================================
// Empty / header-only inputs
// =============================================================================

TEST(EmptyInput, EmptyFileYieldsZeroRows) {
    auto tsv = MakeTsv({});
    LoadStats stats;
    auto movies = LoadMovies(tsv.path(), NoFilter(), &stats);
    EXPECT_TRUE(movies.empty());
    EXPECT_EQ(stats.rows_seen, 0);
    EXPECT_EQ(stats.rows_kept, 0);
    EXPECT_EQ(stats.parse_errors, 0);
}

TEST(EmptyInput, HeaderOnlyYieldsZeroRows) {
    auto tsv = MakeTsv({kHeader});
    LoadStats stats;
    auto movies = LoadMovies(tsv.path(), NoFilter(), &stats);
    EXPECT_TRUE(movies.empty());
    EXPECT_EQ(stats.rows_seen, 0);
    EXPECT_EQ(stats.rows_kept, 0);
}

TEST(EmptyInput, MissingFileThrows) {
    EXPECT_THROW(
        LoadMovies("Z:/this/path/should/never/exist/imdb.tsv", NoFilter()),
        std::runtime_error);
}

// =============================================================================
// Single-row field parsing
// =============================================================================

TEST(SingleRow, ParsesAllFields) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000001\tmovie\tInception\tInception\t0\t"
        "2010\t\\N\t148\tAction,Sci-Fi\t8.8\t2100000\n",
    });
    auto movies = LoadMovies(tsv.path(), NoFilter());
    ASSERT_EQ(movies.size(), 1u);
    EXPECT_EQ(movies[0].tconst, "tt0000001");
    EXPECT_EQ(movies[0].primary_title, "Inception");
    EXPECT_EQ(movies[0].start_year, 2010);
    EXPECT_EQ(movies[0].runtime_minutes, 148);
    EXPECT_FLOAT_EQ(movies[0].rating, 8.8f);
    EXPECT_EQ(movies[0].num_votes, 2100000u);
    EXPECT_EQ(movies[0].genres, "Action,Sci-Fi");
}

TEST(SingleRow, MissingFieldsBecomeDefaults) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000002\tmovie\tMystery\tMystery\t0\t\\N\t\\N\t\\N\t\\N\t\\N\t\\N\n",
    });
    auto movies = LoadMovies(tsv.path(), NoFilter());
    ASSERT_EQ(movies.size(), 1u);
    EXPECT_EQ(movies[0].start_year, 0);
    EXPECT_EQ(movies[0].runtime_minutes, -1);
    EXPECT_FLOAT_EQ(movies[0].rating, 0.0f);
    EXPECT_EQ(movies[0].num_votes, 0u);
    EXPECT_TRUE(movies[0].genres.empty());
}

TEST(SingleRow, RatingAndVotesColumnsMayBeAbsent) {
    // Only 9 columns (no rating / votes at all, not even \N).
    auto tsv = MakeTsv({
        kHeader,
        "tt0000003\tmovie\tOld Movie\tOld Movie\t0\t1955\t\\N\t120\tDrama\n",
    });
    auto movies = LoadMovies(tsv.path(), NoFilter());
    ASSERT_EQ(movies.size(), 1u);
    EXPECT_FLOAT_EQ(movies[0].rating, 0.0f);
    EXPECT_EQ(movies[0].num_votes, 0u);
}

// =============================================================================
// Filter behaviour
// =============================================================================

TEST(Filter, RatingThreshold) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000010\tmovie\tA\tA\t0\t2000\t\\N\t90\tDrama\t9.1\t500000\n",
        "tt0000011\tmovie\tB\tB\t0\t2000\t\\N\t90\tDrama\t7.5\t500000\n",
    });
    auto opts = NoFilter();
    opts.min_rating = 8.0;
    auto movies = LoadMovies(tsv.path(), opts);
    ASSERT_EQ(movies.size(), 1u);
    EXPECT_EQ(movies[0].tconst, "tt0000010");
}

TEST(Filter, VotesThreshold) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000020\tmovie\tPopular\tPopular\t0\t2000\t\\N\t90\tDrama\t9.5\t1000000\n",
        "tt0000021\tmovie\tObscure\tObscure\t0\t2000\t\\N\t90\tDrama\t9.5\t100\n",
    });
    auto opts = NoFilter();
    opts.min_votes = 1000;
    auto movies = LoadMovies(tsv.path(), opts);
    ASSERT_EQ(movies.size(), 1u);
    EXPECT_EQ(movies[0].tconst, "tt0000020");
}

TEST(Filter, CombinedRatingAndVotes) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000030\tmovie\tTop\tTop\t0\t2000\t\\N\t90\tDrama\t9.5\t2000000\n",
        "tt0000031\tmovie\tMidRating\tMidRating\t0\t2000\t\\N\t90\tDrama\t7.0\t2000000\n",
        "tt0000032\tmovie\tMidVotes\tMidVotes\t0\t2000\t\\N\t90\tDrama\t9.5\t10\n",
    });
    auto opts = NoFilter();
    opts.min_rating = 8.0;
    opts.min_votes  = 1000;
    auto movies = LoadMovies(tsv.path(), opts);
    ASSERT_EQ(movies.size(), 1u);
    EXPECT_EQ(movies[0].tconst, "tt0000030");
}

TEST(Filter, AllowedTitleTypes) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000040\tmovie\tFilm\tFilm\t0\t2000\t\\N\t90\tDrama\t8.5\t5000\n",
        "tt0000041\tshort\tShortFilm\tShortFilm\t0\t2000\t\\N\t10\tDrama\t8.7\t5000\n",
        "tt0000042\ttvSeries\tSeriesX\tSeriesX\t0\t2000\t\\N\t45\tDrama\t8.9\t5000\n",
    });
    auto opts = NoFilter();
    opts.allowed_title_types = {"movie", "short"};
    auto movies = LoadMovies(tsv.path(), opts);
    ASSERT_EQ(movies.size(), 2u);
    EXPECT_EQ(movies[0].tconst, "tt0000040");
    EXPECT_EQ(movies[1].tconst, "tt0000041");
}

// =============================================================================
// Quoted fields, escape handling
// =============================================================================

TEST(Escape, QuotedTitleWithCommaIsUnescaped) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000050\tmovie\t\"Hello, World\"\t\"Hello, World\"\t0\t"
        "2010\t\\N\t90\tDrama\t8.0\t1000\n",
    });
    auto movies = LoadMovies(tsv.path(), NoFilter());
    ASSERT_EQ(movies.size(), 1u);
    EXPECT_EQ(movies[0].primary_title, "Hello, World");
}

TEST(Escape, DoubledQuoteInsideFieldBecomesSingleQuote) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000051\tmovie\t\"She said \"\"hi\"\"\"\t"
        "\"She said \"\"hi\"\"\"\t0\t2010\t\\N\t90\tDrama\t8.0\t1000\n",
    });
    auto movies = LoadMovies(tsv.path(), NoFilter());
    ASSERT_EQ(movies.size(), 1u);
    EXPECT_EQ(movies[0].primary_title, "She said \"hi\"");
}

TEST(Escape, EscapedTabInsideQuotedFieldPreserved) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000052\tmovie\t\"A\tB\"\t\"A\tB\"\t0\t2010\t\\N\t90\tDrama\t8.0\t1000\n",
    });
    auto movies = LoadMovies(tsv.path(), NoFilter());
    ASSERT_EQ(movies.size(), 1u);
    EXPECT_EQ(movies[0].primary_title, "A\tB");
}

// =============================================================================
// Encoding / line-ending robustness
// =============================================================================

TEST(Encoding, Utf8TitleBytesArePreserved) {
    // Hard-coded UTF-8 bytes for "测试" so the test is encoding-agnostic
    // at the source level -- the row we WRITE is also raw bytes.
    const std::string utf8_title = "\xE6\xB5\x8B\xE8\xAF\x95";
    std::string row = std::string("tt0000070\tmovie\t") + utf8_title +
                      "\t" + utf8_title +
                      "\t0\t2024\t\\N\t120\tSci-Fi\t8.5\t2000\n";
    auto tsv = MakeTsv({kHeader, row});

    auto movies = LoadMovies(tsv.path(), NoFilter());
    ASSERT_EQ(movies.size(), 1u);
    EXPECT_EQ(movies[0].primary_title, utf8_title);
}

TEST(Encoding, CrlfLineEndingsAreTolerated) {
    std::string body = std::string(kHeader) +
        "tt0000080\tmovie\tM\tM\t0\t2010\t\\N\t90\tDrama\t8.0\t1000\r\n";
    TempTsv tsv(body);

    auto movies = LoadMovies(tsv.path(), NoFilter());
    ASSERT_EQ(movies.size(), 1u);
    EXPECT_EQ(movies[0].tconst, "tt0000080");
    EXPECT_EQ(movies[0].start_year, 2010);
}

TEST(Encoding, BomIsStrippedFromFirstLine) {
    std::string body = std::string("\xEF\xBB\xBF") + kHeader +
        "tt0000090\tmovie\tM\tM\t0\t2010\t\\N\t90\tDrama\t8.0\t1000\n";
    TempTsv tsv(body);

    LoadStats stats;
    auto movies = LoadMovies(tsv.path(), NoFilter(), &stats);
    ASSERT_EQ(movies.size(), 1u);
    EXPECT_EQ(movies[0].tconst, "tt0000090");
    EXPECT_EQ(stats.rows_seen, 1);
}

// =============================================================================
// Streaming semantics
// =============================================================================

TEST(Streaming, VisitorReturningFalseStopsEarly) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000100\tmovie\tA\tA\t0\t2000\t\\N\t90\tDrama\t8.5\t5000\n",
        "tt0000101\tmovie\tB\tB\t0\t2000\t\\N\t90\tDrama\t8.6\t5000\n",
        "tt0000102\tmovie\tC\tC\t0\t2000\t\\N\t90\tDrama\t8.7\t5000\n",
    });
    int seen = 0;
    LoadStats stats;
    StreamMovies(tsv.path(), NoFilter(),
        [&seen](const Movie&) {
            ++seen;
            return seen < 2;  // stop after seeing the second row
        },
        &stats);
    EXPECT_EQ(seen, 2);
    EXPECT_TRUE(stats.aborted_by_visitor);
    EXPECT_EQ(stats.rows_seen, 2);
}

TEST(Streaming, ProgressCallbackFiresAtConfiguredInterval) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000110\tmovie\tA\tA\t0\t2000\t\\N\t90\tDrama\t8.5\t5000\n",
        "tt0000111\tmovie\tB\tB\t0\t2000\t\\N\t90\tDrama\t8.5\t5000\n",
        "tt0000112\tmovie\tC\tC\t0\t2000\t\\N\t90\tDrama\t8.5\t5000\n",
    });
    auto opts = NoFilter();
    opts.progress_every = 2;
    int progress_calls = 0;
    std::int64_t last_seen = 0;
    opts.on_progress = [&](std::int64_t seen, std::int64_t /*kept*/) {
        ++progress_calls;
        last_seen = seen;
    };

    LoadMovies(tsv.path(), opts);
    EXPECT_GE(progress_calls, 1);
    EXPECT_EQ(last_seen, 3);
}

TEST(Streaming, ThinApiForEachMovieDeliversEachRow) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000120\tmovie\tX\tX\t0\t2000\t\\N\t90\tDrama\t8.5\t5000\n",
        "tt0000121\tmovie\tY\tY\t0\t2000\t\\N\t90\tDrama\t8.6\t5000\n",
    });

    int count = 0;
    auto n = imdb::for_each_movie(tsv.path(),
        [&count](const Movie&) { ++count; return true; });
    EXPECT_EQ(count, 2);
    EXPECT_EQ(n, 2u);
}

TEST(Streaming, ThinApiLoadMoviesFiltersByThreshold) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000130\tmovie\tKeep\tKeep\t0\t2000\t\\N\t90\tDrama\t9.0\t5000\n",
        "tt0000131\tmovie\tDrop\tDrop\t0\t2000\t\\N\t90\tDrama\t7.0\t5000\n",
    });
    std::vector<Movie> sink;
    auto n = imdb::load_movies(tsv.path(), sink, 8.0f, 0u);
    EXPECT_EQ(n, 1u);
    ASSERT_EQ(sink.size(), 1u);
    EXPECT_EQ(sink[0].tconst, "tt0000130");
}

// =============================================================================
// Error handling
// =============================================================================

TEST(Error, MalformedRowIsCountedAndStreamContinues) {
    auto tsv = MakeTsv({
        kHeader,
        "tt0000200\tmovie\tGood\tGood\t0\t2010\t\\N\t90\tDrama\t8.0\t1000\n",
        "this-line-has-only-three-fields\n",  // malformed
        "tt0000201\tmovie\tAlsoGood\tAlsoGood\t0\t2011\t\\N\t95\tDrama\t8.1\t2000\n",
    });
    LoadStats stats;
    auto movies = LoadMovies(tsv.path(), NoFilter(), &stats);
    EXPECT_EQ(movies.size(), 2u);
    EXPECT_EQ(stats.parse_errors, 1);
    EXPECT_EQ(stats.rows_seen, 3);
    EXPECT_EQ(stats.rows_kept, 2);
}

// =============================================================================
// Integration with the committed fixture
// =============================================================================

TEST(SampleFixture, LoadsRowsFromSampleTsv) {
    // IMDB_TEST_DATA is injected by CMake (gtest_discover_tests
    // PROPERTIES ENVIRONMENT in the root CMakeLists.txt).
    const char* env = std::getenv("IMDB_TEST_DATA");
    ASSERT_NE(env, nullptr) << "IMDB_TEST_DATA not set by test runner";
    std::string base = env;
    while (!base.empty() && (base.back() == '/'  ||
                             base.back() == '\\' ||
                             base.back() == ' ')) {
        base.pop_back();
    }
    std::string sample = base + "/sample.tsv";

    auto opts = NoFilter();
    opts.min_rating = 8.0;
    opts.min_votes  = 1000;
    LoadStats stats;
    auto movies = LoadMovies(sample, opts, &stats);

    // From sample.tsv: tt0111161, tt0068646, tt0468569, tt9999992,
    // tt9999993. tt9999991 has only 50 votes so it's filtered out.
    ASSERT_EQ(movies.size(), 5u);
    EXPECT_EQ(movies[0].tconst, "tt0111161");
    EXPECT_EQ(movies[0].start_year, 1994);

    auto it = std::find_if(movies.begin(), movies.end(),
        [](const Movie& m) { return m.tconst == "tt9999992"; });
    ASSERT_NE(it, movies.end());
    EXPECT_EQ(it->primary_title, "Movie, with Comma");

    EXPECT_EQ(stats.parse_errors, 0);
}

TEST(SampleFixture, MovieOrderingIsDefined) {
    const char* env = std::getenv("IMDB_TEST_DATA");
    if (!env) GTEST_SKIP() << "IMDB_TEST_DATA not set";
    std::string base = env;
    while (!base.empty() && (base.back() == '/'  ||
                             base.back() == '\\' ||
                             base.back() == ' ')) {
        base.pop_back();
    }
    std::string sample = base + "/sample.tsv";

    auto movies = LoadMovies(sample, NoFilter());
    ASSERT_GE(movies.size(), 2u);

    Movie a = movies[0];
    Movie b = movies[1];
    EXPECT_TRUE(a < b || b < a)
        << "operator< must impose a strict total order";
}
