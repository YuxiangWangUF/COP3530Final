// SPDX-License-Identifier: MIT
// Movie data model for the IMDb Top-250 recommender.
//
// Field layout mirrors title.basics.tsv (the official IMDb dump) so the
// streaming loader can fill the struct directly without an intermediate
// remap. The joined rating and vote fields come from title.ratings.tsv.

#pragma once

#include <cstdint>
#include <string>

namespace imdb {

struct Movie {
    /// IMDb primary key, e.g. "tt0111161". Empty when the source lacked
    /// one (malformed row). Used for join with title.ratings.tsv.
    std::string tconst;

    /// primaryTitle from title.basics.tsv. UTF-8 bytes; we do not
    /// reinterpret encoding -- downstream code that needs to display
    /// titles does its own normalization.
    std::string primary_title;

    /// startYear. 0 means unknown (`\N` in the dump).
    int start_year = 0;

    /// runtimeMinutes. -1 means unknown (`\N`).
    int runtime_minutes = -1;

    /// averageRating (joined from title.ratings.tsv). 0.0f means unrated.
    float rating = 0.0f;

    /// numVotes. uint32_t supports up to ~4.3B; the busiest IMDb titles
    /// sit at single-digit millions, so we have four orders of magnitude
    /// of headroom.
    std::uint32_t num_votes = 0;

    /// Comma-separated genre list, e.g. "Drama,Romance". Empty means
    /// unknown. Kept as a raw string to match the dump format and to
    /// avoid a per-row allocation in the streaming loader.
    std::string genres;

    /// Total ordering: higher rating first, then more votes, then earlier
    /// year, then lexicographic title. Used by the algorithm layer's Top-K
    /// heap (MinHeap<Movie, std::greater<Movie>> picks the highest).
    /// operator< is required to be a strict total order; we always break
    /// ties lexicographically on title so two unrated movies never compare
    /// equal.
    friend bool operator<(const Movie& a, const Movie& b) noexcept {
        if (a.rating != b.rating) return a.rating > b.rating;
        if (a.num_votes != b.num_votes) return a.num_votes > b.num_votes;
        if (a.start_year != b.start_year) return a.start_year < b.start_year;
        if (a.primary_title != b.primary_title) return a.primary_title < b.primary_title;
        return a.tconst < b.tconst;
    }

    friend bool operator==(const Movie& a, const Movie& b) noexcept {
        return a.tconst == b.tconst && a.primary_title == b.primary_title;
    }
};

}  // namespace imdb