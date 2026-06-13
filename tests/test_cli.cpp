// SPDX-License-Identifier: MIT
// Integration test for the imdb-topk CLI.
//
// What we verify:
//   1. The CLI binary is built and exists (set by the CMake
//      gtest_discover_tests PROPERTIES).
//   2. Running  imdb-topk --data <tsv> --top 5  produces exactly 6
//      lines of stdout: 1 header + 5 data rows.
//   3. The header is  "rank  title  year  rating  votes".
//   4. The rank column on the data rows is strictly increasing 1,2,3,4,5.
//   5. The rating column is strictly non-increasing (best-first).
//   6. The top entry is the highest-rated movie in the test fixture.
//
// We use a 12-row synthetic fixture with deliberate ties to exercise
// tie-breaking rules (operator< falls through to votes, then year,
// then title, then tconst).

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Windows-only headers for the CreateProcess-based child driver below.
// Guarded so the test still compiles (and skips) on non-Windows hosts.
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "imdb/Loader.h"
#include "imdb/MinHeap.h"
#include "imdb/Movie.h"
#include "imdb/TopK.h"

namespace fs = std::filesystem;

namespace {

// One-time synthetic fixture: 12 rows, rating 6.0..9.5 with a few
// deliberate ties.  Vote count varies so min-votes filter is exercised.
const char kFixture[] =
    "tconst\ttitleType\tprimaryTitle\toriginalTitle\tisAdult\t"
    "startYear\tendYear\truntimeMinutes\tgenres\t"
    "averageRating\tnumVotes\n"
    // (rank by operator< = rating desc, then votes desc, then year asc, ...)
    "tt0000001\tmovie\tAAA Best\tAAA Best\t0\t2000\t\\N\t100\tDrama\t"
        "9.5\t200000\n"
    "tt0000002\tmovie\tBBB Tie-9.0\tBBB Tie-9.0\t0\t2010\t\\N\t100\tDrama\t"
        "9.0\t500000\n"
    "tt0000003\tmovie\tCCC Tie-9.0-fewer-votes\tCCC Tie-9.0-fewer-votes\t0\t"
        "2005\t\\N\t100\tDrama\t9.0\t100000\n"
    "tt0000004\tmovie\tDDD 8.5-old\tDDD 8.5-old\t0\t1985\t\\N\t100\tDrama\t"
        "8.5\t300000\n"
    "tt0000005\tmovie\tEEE 8.5-newer\tEEE 8.5-newer\t0\t2020\t\\N\t100\tDrama\t"
        "8.5\t300000\n"
    "tt0000006\tmovie\tFFF 8.0\tFFF 8.0\t0\t2015\t\\N\t100\tDrama\t"
        "8.0\t50000\n"
    "tt0000007\tmovie\tGGG 7.5\tGGG 7.5\t0\t2012\t\\N\t100\tDrama\t"
        "7.5\t10000\n"
    "tt0000008\tmovie\tHHH 7.0-loud\tHHH 7.0-loud\t0\t2010\t\\N\t100\tDrama\t"
        "7.0\t9000\n"  // filtered out by min-votes=10000
    "tt0000009\tmovie\tIII 7.0-quiet\tIII 7.0-quiet\t0\t2010\t\\N\t100\tDrama\t"
        "7.0\t5000\n"  // filtered out
    "tt0000010\tmovie\tJJJ 6.5\tJJJ 6.5\t0\t2018\t\\N\t100\tDrama\t"
        "6.5\t2000\n"  // filtered out
    "tt0000011\tmovie\tKKK 6.0\tKKK 6.0\t0\t1999\t\\N\t100\tDrama\t"
        "6.0\t100000\n"
    "tt0000012\tmovie\tLLL 5.0\tLLL 5.0\t0\t1990\t\\N\t100\tDrama\t"
        "5.0\t500000\n";  // filtered out (rating < 7.5) — kept here for parse count only

class CliFixture : public ::testing::Test {
   protected:
    void SetUp() override {
        // Write fixture to a unique path under the temp directory so
        // multiple parallel ctest instances don't clobber each other.
        static int counter = 0;
        path_ = (fs::temp_directory_path() /
                 ("imdb_cli_fixture_" + std::to_string(counter++) + ".tsv")).string();
        std::ofstream out(path_, std::ios::binary);
        ASSERT_TRUE(out.good()) << "cannot write fixture to " << path_;
        out.write(kFixture, sizeof(kFixture) - 1);
        out.close();
    }
    void TearDown() override {
        std::error_code ec;
        fs::remove(path_, ec);
    }
    std::string path_;
};

// RAII wrapper around a Win32 CreateProcess child that captures stdout.
// We don't use _popen because its cmd.exe quoting layer mangles the
// quoted binary path / quoted --data arg we need.  CreateProcess with
// STARTF_USESTDHANDLES + a pipe gives us direct control over the
// child's stdout.
class ChildProcess {
   public:
    explicit ChildProcess(const std::string& cmd_line) {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE read_end = nullptr;
        HANDLE write_end = nullptr;
        if (!::CreatePipe(&read_end, &write_end, &sa, 0)) {
            throw std::runtime_error("CreatePipe failed");
        }
        // The read end must NOT be inherited (we want the parent to be
        // the sole reader); the write end must be inherited (so the
        // child's stdout is the pipe).
        if (!::SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0)) {
            ::CloseHandle(read_end);
            ::CloseHandle(write_end);
            throw std::runtime_error("SetHandleInformation failed");
        }

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = write_end;
        si.hStdError  = ::GetStdHandle(STD_ERROR_HANDLE);
        si.hStdInput  = ::GetStdHandle(STD_INPUT_HANDLE);

        // CreateProcessA takes a writable command line buffer.
        std::vector<char> cmd_buf(cmd_line.begin(), cmd_line.end());
        cmd_buf.push_back('\0');

        PROCESS_INFORMATION pi{};
        const BOOL ok = ::CreateProcessA(
            /* lpApplicationName */ nullptr,
            /* lpCommandLine      */ cmd_buf.data(),
            /* lpProcessAttributes*/ nullptr,
            /* lpThreadAttributes */ nullptr,
            /* bInheritHandles    */ TRUE,
            /* dwCreationFlags    */ 0,
            /* lpEnvironment      */ nullptr,
            /* lpCurrentDirectory */ nullptr,
            /* lpStartupInfo      */ &si,
            /* lpProcessInformation*/ &pi);
        if (!ok) {
            const DWORD err = ::GetLastError();
            ::CloseHandle(read_end);
            ::CloseHandle(write_end);
            throw std::runtime_error("CreateProcess failed; error=" +
                                     std::to_string(err) + " cmd=" + cmd_line);
        }

        ::CloseHandle(write_end);          // parent doesn't write
        read_handle_ = read_end;
        proc_handle_ = pi.hProcess;
        thread_handle_ = pi.hThread;
    }

    ~ChildProcess() {
        if (proc_handle_) {
            ::WaitForSingleObject(proc_handle_, 5000);
            ::CloseHandle(proc_handle_);
        }
        if (thread_handle_) ::CloseHandle(thread_handle_);
        if (read_handle_)   ::CloseHandle(read_handle_);
    }

    // Read entire stdout to a string.
    std::string ReadAll() {
        std::string out;
        char buf[4096];
        for (;;) {
            DWORD n = 0;
            const BOOL ok = ::ReadFile(read_handle_, buf, sizeof(buf), &n, nullptr);
            if (!ok || n == 0) break;
            out.append(buf, n);
        }
        return out;
    }

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

   private:
    HANDLE read_handle_  = nullptr;
    HANDLE proc_handle_  = nullptr;
    HANDLE thread_handle_ = nullptr;
};

// Locate the imdb-topk executable.  We rely on the test runner setting
// IMDB_CLI_BIN (CMake gtest_discover_tests PROPERTIES ENVIRONMENT), and
// fall back to a relative path so the test still runs in a manual session.
std::string LocateCliBinary() {
    if (const char* env = std::getenv("IMDB_CLI_BIN")) return env;
    // Fall back: assume we're in the build dir, look up one level.
    fs::path here = fs::current_path();
    fs::path candidate = here / "imdb-topk.exe";
    if (fs::exists(candidate)) return candidate.string();
    candidate = here / "imdb-topk";
    if (fs::exists(candidate)) return candidate.string();
    return "";  // will skip with a clear message
}

std::vector<std::string> SplitLines(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line)) {
        // Strip trailing \r (Windows line endings).
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(std::move(line));
    }
    return out;
}

std::vector<std::string> SplitTabs(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '\t') { out.push_back(std::move(cur)); cur.clear(); }
        else           { cur.push_back(c); }
    }
    out.push_back(std::move(cur));
    return out;
}

}  // namespace

// =============================================================================
// Integration: spawn the CLI binary and verify stdout.
// =============================================================================

TEST_F(CliFixture, Top5HasStrictlyIncreasingRanksAndDescendingRatings) {
    const std::string bin = LocateCliBinary();
    if (bin.empty()) {
        GTEST_SKIP() << "IMDB_CLI_BIN not set and ./imdb-topk not found in cwd; "
                        "skipping CLI integration test";
    }
    const std::string cmd = "\"" + bin + "\" --data \"" + path_ +
                            "\" --top 5 --min-votes 10000 --min-rating 7.5";
    ChildProcess child(cmd);
    const std::string out = child.ReadAll();
    const auto lines = SplitLines(out);
    ASSERT_GE(lines.size(), 1u) << "no output from CLI; raw=" << out;

    // First line is the header.
    EXPECT_EQ(lines[0], std::string("rank\ttitle\tyear\trating\tvotes"));

    // Must have exactly 5 data rows when --top 5 and the fixture has
    // exactly 5 rows that pass the filter (tt0000008/9/10/12 fail
    // min-votes or min-rating).
    ASSERT_EQ(lines.size(), 6u)
        << "expected 1 header + 5 data rows, got " << lines.size()
        << "\n--- raw output ---\n" << out;

    std::vector<std::size_t> ranks;
    std::vector<float>      ratings;
    std::vector<std::string> titles;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        const auto cols = SplitTabs(lines[i]);
        ASSERT_EQ(cols.size(), 5u) << "row " << i << " is not 5 columns: "
                                  << lines[i];
        // rank strictly increasing from 1 to 5
        const std::size_t r = static_cast<std::size_t>(std::stoull(cols[0]));
        EXPECT_EQ(r, i) << "rank should be " << i << " on row " << i;
        ranks.push_back(r);
        ratings.push_back(std::stof(cols[3]));
        titles.push_back(cols[1]);
    }

    // Ranks are strictly increasing 1..5.
    for (std::size_t i = 0; i < ranks.size(); ++i) {
        EXPECT_EQ(ranks[i], i + 1);
    }
    // Ratings are non-increasing (descending).
    for (std::size_t i = 1; i < ratings.size(); ++i) {
        EXPECT_GE(ratings[i - 1], ratings[i])
            << "rating not descending: row " << (i - 1) << " has "
            << ratings[i - 1] << ", row " << i << " has " << ratings[i];
    }

    // The top entry must be the highest-rated row in the fixture
    // (tt0000001 "AAA Best" with rating 9.5).
    EXPECT_EQ(titles.front(), "AAA Best");
    EXPECT_FLOAT_EQ(ratings.front(), 9.5f);
}

TEST_F(CliFixture, MinVotesFilterRemovesLowVoteRows) {
    const std::string bin = LocateCliBinary();
    if (bin.empty()) {
        GTEST_SKIP() << "IMDB_CLI_BIN not set";
    }
    // Same data, but ask for the top 10 with min-votes=20000.
    // Rows with <20000 votes: tt0000006 (50K -- kept), tt0000007 (10K -- out),
    // tt0000008..12.  So kept rows above 7.5 rating and 20K votes:
    //   tt0000001 (9.5, 200K), tt0000002 (9.0, 500K), tt0000003 (9.0, 100K),
    //   tt0000004 (8.5, 300K), tt0000005 (8.5, 300K), tt0000006 (8.0, 50K)
    // = 6 rows.
    const std::string cmd = "\"" + bin + "\" --data \"" + path_ +
                            "\" --top 10 --min-votes 20000 --min-rating 7.5";
    ChildProcess child(cmd);
    const std::string out = child.ReadAll();
    const auto lines = SplitLines(out);
    ASSERT_EQ(lines.size(), 7u) << "expected 1 header + 6 data rows\n"
                               << "--- raw output ---\n" << out;
    // tt0000007 (GGG 7.5, 10K) must be filtered out.
    for (std::size_t i = 1; i < lines.size(); ++i) {
        const auto cols = SplitTabs(lines[i]);
        EXPECT_NE(cols[1], std::string("GGG 7.5"))
            << "row with only 10K votes should be filtered by --min-votes 20000";
    }
}

// =============================================================================
// Unit tests: exercise imdb::top_k directly (no subprocess) so the algorithm
// path is verified even if the binary path is unavailable.
// =============================================================================

TEST(TopK, ReturnsEmptyOnZeroK) {
    auto rows = imdb::top_k("ignored.tsv", 0, 0, 0.0f);
    EXPECT_TRUE(rows.empty());
}

TEST(TopK, ReturnsAtMostKRows) {
    const auto tmp = (fs::temp_directory_path() / "imdb_topk_unit.tsv").string();
    {
        std::ofstream out(tmp, std::ios::binary);
        out << "tconst\ttitleType\tprimaryTitle\toriginalTitle\tisAdult\t"
               "startYear\tendYear\truntimeMinutes\tgenres\t"
               "averageRating\tnumVotes\n";
        for (int i = 0; i < 50; ++i) {
            out << "tt" << (1000000 + i) << "\tmovie\tT" << i << "\tT" << i
                << "\t0\t2000\t\\N\t90\tDrama\t"
                << (5.0f + i * 0.1f) << "\t1000\n";
        }
    }
    auto rows = imdb::top_k(tmp, 5, 0, 0.0f);
    EXPECT_EQ(rows.size(), 5u);
    // best-first: rating 9.9, 9.8, ..., 9.5
    EXPECT_FLOAT_EQ(rows[0].rating, 9.9f);
    EXPECT_FLOAT_EQ(rows[1].rating, 9.8f);
    EXPECT_FLOAT_EQ(rows[4].rating, 9.5f);
    std::error_code ec;
    fs::remove(tmp, ec);
}

TEST(TopK, MinVotesFilterIsApplied) {
    const auto tmp = (fs::temp_directory_path() / "imdb_topk_unit2.tsv").string();
    {
        std::ofstream out(tmp, std::ios::binary);
        out << "tconst\ttitleType\tprimaryTitle\toriginalTitle\tisAdult\t"
               "startYear\tendYear\truntimeMinutes\tgenres\t"
               "averageRating\tnumVotes\n";
        out << "tt0000001\tmovie\tLoud\tLoud\t0\t2000\t\\N\t90\tDrama\t9.0\t5000\n";
        out << "tt0000002\tmovie\tQuiet\tQuiet\t0\t2000\t\\N\t90\tDrama\t9.0\t100\n";
    }
    auto rows = imdb::top_k(tmp, 10, 1000, 0.0f);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].primary_title, "Loud");
    std::error_code ec;
    fs::remove(tmp, ec);
}

TEST(TopK, OutputIsBestFirstUnderOperatorLess) {
    // Tie-break rule: at equal rating, the higher-votes row wins.
    const auto tmp = (fs::temp_directory_path() / "imdb_topk_unit3.tsv").string();
    {
        std::ofstream out(tmp, std::ios::binary);
        out << "tconst\ttitleType\tprimaryTitle\toriginalTitle\tisAdult\t"
               "startYear\tendYear\truntimeMinutes\tgenres\t"
               "averageRating\tnumVotes\n";
        out << "tt0000001\tmovie\tFewer\tFewer\t0\t2000\t\\N\t90\tDrama\t9.0\t1000\n";
        out << "tt0000002\tmovie\tMore\tMore\t0\t2000\t\\N\t90\tDrama\t9.0\t9000\n";
    }
    auto rows = imdb::top_k(tmp, 2, 0, 0.0f);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].primary_title, "More")
        << "with equal rating, higher num_votes wins tie-break";
    EXPECT_EQ(rows[1].primary_title, "Fewer");
    std::error_code ec;
    fs::remove(tmp, ec);
}
