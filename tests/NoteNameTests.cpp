#include "engine/BankBuilder.h"

#include <catch2/catch_test_macros.hpp>

using namespace keys;

TEST_CASE("Note names parse to MIDI numbers with C4 = 60", "[names]")
{
    CHECK(parseNoteName("c4") == 60);
    CHECK(parseNoteName("C4") == 60);
    CHECK(parseNoteName("g#4") == 68);
    CHECK(parseNoteName("A2") == 45);
    CHECK(parseNoteName("Bb3") == 58);
    CHECK(parseNoteName("b3") == 59);
    CHECK(parseNoteName("E1") == 28);
    CHECK(parseNoteName("e7") == 100);
    CHECK(parseNoteName("C-1") == 0);

    CHECK(parseNoteName("") == -1);
    CHECK(parseNoteName("h4") == -1);
    CHECK(parseNoteName("c") == -1);
    CHECK(parseNoteName("c4x") == -1);
}

TEST_CASE("Celeste file names give note and dynamic layer", "[names]")
{
    const auto soft = parseCelesteFileName("celeste-g#4-soft.flac");
    REQUIRE(soft);
    CHECK(soft->note == 68);
    CHECK(soft->layer == 0);

    const auto hard = parseCelesteFileName("celeste-c7-hard.flac");
    REQUIRE(hard);
    CHECK(hard->note == 96);
    CHECK(hard->layer == 1);

    CHECK_FALSE(parseCelesteFileName("celeste-mech.flac"));
    CHECK_FALSE(parseCelesteFileName("piano-c4-soft.flac"));
}

TEST_CASE("VCSL vibraphone file names give note and layer", "[names]")
{
    const auto a = parseVcslFileName("Vibes_soft_A2_v1_rr1_Main.wav");
    REQUIRE(a);
    CHECK(a->note == 45);
    CHECK(a->layer == 0);

    const auto c = parseVcslFileName("Vibes_soft_C3_v2_rr1_Main.wav");
    REQUIRE(c);
    CHECK(c->note == 48);
    CHECK(c->layer == 1);

    CHECK_FALSE(parseVcslFileName("Vibes_soft_A2.wav"));
    CHECK_FALSE(parseVcslFileName("LICENSE.txt"));
}
