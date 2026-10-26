#include "engine/BankBuilder.h"
#include "engine/SampleBank.h"

#include <catch2/catch_test_macros.hpp>

using namespace keys;

namespace
{
std::unique_ptr<Sample> makeSample(int root, float level)
{
    auto s = std::make_unique<Sample>();
    s->audio.setSize(1, 64);
    s->audio.clear();
    s->audio.setSample(0, 10, level);
    s->rootNote = root;
    return s;
}

/// Three roots a major third apart, two velocity layers, like the celeste set.
SampleBank makeBank()
{
    SampleBank bank("test");
    std::vector<RootSample> roots;
    for (int root : { 60, 64, 68 })
        for (int layer = 0; layer < 2; ++layer)
            roots.push_back({ layer, makeSample(root, layer == 0 ? 0.25f : 0.5f) });
    mapRootsIntoBank(bank, std::move(roots), { 70, 127 });
    bank.finalize();
    return bank;
}
} // namespace

TEST_CASE("Keys between roots are split halfway", "[bank]")
{
    const auto bank = makeBank();
    CHECK(bank.find(60, 50)->sample->rootNote == 60);
    CHECK(bank.find(62, 50)->sample->rootNote == 60); // 62 is the midpoint: it stays with the lower root
    CHECK(bank.find(63, 50)->sample->rootNote == 64);
    CHECK(bank.find(66, 50)->sample->rootNote == 64);
    CHECK(bank.find(67, 50)->sample->rootNote == 68);
}

TEST_CASE("Velocity picks the layer", "[bank]")
{
    const auto bank = makeBank();
    const auto* soft = bank.find(64, 70);
    const auto* hard = bank.find(64, 71);
    REQUIRE(soft != nullptr);
    REQUIRE(hard != nullptr);
    CHECK(soft->hiVel == 70);
    CHECK(hard->loVel == 71);
    CHECK(soft->refVelocity == 70.0f);
}

TEST_CASE("Keys outside the sampled range use the nearest root", "[bank]")
{
    const auto bank = makeBank();
    CHECK(bank.find(20, 100)->sample->rootNote == 60);
    CHECK(bank.find(127, 100)->sample->rootNote == 68);
    CHECK(bank.lowestRoot() == 60);
    CHECK(bank.highestRoot() == 68);
}

TEST_CASE("A missing layer borrows the same key's other layer before another key", "[bank]")
{
    SampleBank bank("gaps");
    std::vector<RootSample> roots;
    roots.push_back({ 0, makeSample(60, 0.2f) }); // only a soft sample at 60
    roots.push_back({ 0, makeSample(64, 0.2f) });
    roots.push_back({ 1, makeSample(64, 0.4f) });
    mapRootsIntoBank(bank, std::move(roots), { 70, 127 });
    bank.finalize();

    const auto* zone = bank.find(60, 120);
    REQUIRE(zone != nullptr);
    CHECK(zone->sample->rootNote == 60);
}

TEST_CASE("normalizeTo scales the loudest layer to the target peak", "[bank]")
{
    auto bank = makeBank();
    bank.normalizeTo(0.25f);
    CHECK(bank.gain() == 0.5f); // loud layer peaks at 0.5
}

TEST_CASE("An empty bank answers nullptr", "[bank]")
{
    SampleBank bank("empty");
    bank.finalize();
    CHECK(bank.find(60, 100) == nullptr);
}
