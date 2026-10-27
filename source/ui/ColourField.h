#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <vector>

// =================================================================================================
//  ColourField — the moving light behind the controls. The sound's colour is the picture's colour.
//
//  Each colour knob owns a hue and a few soft "forms" of light that drift on slow Lissajous paths:
//  turning a knob up makes its forms bigger and brighter, Motion makes them drift faster and smear,
//  Bark adds flickering crimson cores, Space adds a deep-blue haze and blur, Patina fades the
//  blacks and thickens the grain. Every note blooms as a glow above its key and fades with the
//  note; the bell layer adds sparkles (gold for celeste, ice blue for vibes).
//
//  How it's drawn, about 30 times a second:
//    1. the light, into a small float buffer (~144 px wide), with screen / soft-light blending,
//       a colour grade and a blur; then upscaled, which is most of the softness;
//    2. crisp details on top at full resolution: film specks, bell sparkles, dust scratches;
//    3. film grain (a noise tile at a random offset) and a vignette.
//  docs/mockup/nocturne.html is the reference for every number here.
// =================================================================================================
namespace keys::ui
{
class ColourField final : public juce::Component
{
public:
    /// What the field shows: the knobs, 0..1, plus which bell is playing.
    struct Inputs
    {
        float warmth = 0.62f, bark = 0.3f, body = 0.58f, patina = 0.35f, motion = 0.32f, space = 0.42f;
        float dust = 0.18f, blend = 0.38f;
        int bellVoice = 0;      ///< 0 celeste, 1 vibes, 2 both
        int bellLowestKey = 41;
    };

    struct Rgb
    {
        float r = 0, g = 0, b = 0;
    };

    ColourField();

    void setInputs(const Inputs& newInputs) noexcept { inputs = newInputs; }
    /// "Still background": the light barely moves and the grain freezes.
    void setStill(bool shouldBeStill) noexcept { still = shouldBeStill; }

    void noteOn(int note, float velocity);
    void noteOff(int note);
    void setPedal(bool down);

    /// Moves the animation on by `seconds` and redraws.
    void advance(double seconds);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Form
    {
        int knob; // index into knobValues()
        Rgb colour;
        float cx, cy, ax, ay, fx, fy, px, py, size, rotation, spin, stretch;
    };
    struct Speck
    {
        float x, y, alpha, radius, phase, speed;
    };
    struct Glow
    {
        int note;
        float x, y, velocity, energy, tau;
        Rgb colour;
        bool held = true, sustained = false;
    };
    struct Spark
    {
        float x, y, life, maxLife, phase, speed;
        Rgb colour;
    };

    std::array<float, 6> knobValues() const noexcept;
    float bellShare(int note) const noexcept;
    bool noteUsesVibes(int note) const noexcept;
    void renderLight();
    void drawForm(float x, float y, float radius, float rotation, float stretch, Rgb colour, float alpha);
    void buildBase();
    void buildOverlays();

    Inputs inputs;
    bool still = false;
    double clock = 0.0, seconds = 0.0;
    juce::Random random { 23 };

    std::vector<Form> forms;
    std::vector<Speck> specks;
    std::vector<Glow> glows;
    std::vector<Spark> sparks;
    bool pedalDown = false;

    // The low-resolution light.
    int lightWidth = 144, lightHeight = 96;
    std::vector<Rgb> base, light, scratch;
    std::vector<float> hazeFalloff, vignetteFalloff; ///< per pixel, fixed for a window size
    std::array<float, 256> falloff {}; ///< a form's brightness against squared distance from its centre
    juce::Image lightImage;

    // Full-resolution overlays, rebuilt when the size changes.
    juce::Image grainTile;
    juce::Point<int> grainOffset;
    bool scratchVisible = false;
    float scratchX = 0.0f, scratchLength = 0.0f, scratchAlpha = 0.0f;
};
} // namespace keys::ui
