#include "Backdrop.h"

#include "KeysLookAndFeel.h"

namespace keys::ui
{
namespace
{
struct Form
{
    juce::uint32 argb;
    float x, y, radius, stretch, rotation, alpha;
};

// Positions are fractions of the window; radius is a fraction of its larger side.
constexpr Form kForms[] = {
    { 0xffd64a3a, 0.20f, 0.30f, 0.30f, 2.2f, 0.5f, 0.55f },  // coral (warmth)
    { 0xffe8263f, 0.48f, 0.55f, 0.16f, 1.8f, 1.1f, 0.40f },  // crimson (bark)
    { 0xffb02a84, 0.72f, 0.40f, 0.34f, 2.4f, -0.4f, 0.55f }, // magenta (body)
    { 0xff7e2496, 0.35f, 0.75f, 0.26f, 2.0f, 2.4f, 0.45f },  // plum
    { 0xff6a46dc, 0.85f, 0.20f, 0.22f, 2.6f, 0.9f, 0.35f },  // violet (motion)
    { 0xff2856b0, 0.80f, 0.80f, 0.36f, 1.7f, -0.8f, 0.55f }, // deep blue (space)
    { 0xff14606e, 0.08f, 0.90f, 0.40f, 1.4f, 0.2f, 0.60f },  // teal pool, low left
};

void drawForm(juce::Graphics& g, const Form& f, float w, float h)
{
    const float cx = f.x * w, cy = f.y * h, r = f.radius * std::max(w, h);
    const auto colour = juce::Colour(f.argb);
    juce::Graphics::ScopedSaveState state(g);
    g.addTransform(juce::AffineTransform::scale(f.stretch, 1.0f, cx, cy).followedBy(juce::AffineTransform::rotation(f.rotation, cx, cy)));
    juce::ColourGradient gradient(colour.withAlpha(f.alpha), cx, cy, colour.withAlpha(0.0f), cx + r, cy, true);
    gradient.addColour(0.35, colour.withAlpha(f.alpha * 0.55f));
    gradient.addColour(0.7, colour.withAlpha(f.alpha * 0.16f));
    g.setGradientFill(gradient);
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
}
} // namespace

juce::Image renderBackdrop(int width, int height, float scale)
{
    const int w = std::max(1, juce::roundToInt(static_cast<float>(width) * scale));
    const int h = std::max(1, juce::roundToInt(static_cast<float>(height) * scale));

    // 1) The light, at a quarter resolution: upscaling it later is what makes it soft.
    const int lw = std::max(8, w / 4), lh = std::max(8, h / 4);
    juce::Image low(juce::Image::ARGB, lw, lh, true);
    {
        juce::Graphics g(low);
        g.fillAll(colours::night);
        for (const auto& form : kForms)
            drawForm(g, form, static_cast<float>(lw), static_cast<float>(lh));
    }

    juce::Image image(juce::Image::ARGB, w, h, true);
    juce::Graphics g(image);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(low, juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)));

    // 2) Film grain, straight into the pixels.
    juce::Random random(1977);
    {
        juce::Image::BitmapData pixels(image, juce::Image::BitmapData::readWrite);
        for (int py = 0; py < h; ++py)
            for (int px = 0; px < w; ++px)
            {
                const auto c = pixels.getPixelColour(px, py);
                const float n = (random.nextFloat() - 0.5f) * 0.09f;
                pixels.setPixelColour(px, py, juce::Colour::fromFloatRGBA(juce::jlimit(0.0f, 1.0f, c.getFloatRed() + n),
                                                                          juce::jlimit(0.0f, 1.0f, c.getFloatGreen() + n),
                                                                          juce::jlimit(0.0f, 1.0f, c.getFloatBlue() + n), 1.0f));
            }
    }

    // 3) Dust specks, then a vignette.
    for (int i = 0; i < 90; ++i)
    {
        const float r = (0.4f + random.nextFloat() * 0.9f) * scale;
        g.setColour(colours::fg.withAlpha(0.12f + random.nextFloat() * 0.45f));
        g.fillEllipse(random.nextFloat() * static_cast<float>(w), random.nextFloat() * static_cast<float>(h), r * 2.0f, r * 2.0f);
    }
    const auto centre = juce::Point<float>(static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.42f);
    juce::ColourGradient vignette(juce::Colours::transparentBlack, centre.x, centre.y,
                                  juce::Colour(0xe6020408), 0.0f, 0.0f, true);
    vignette.addColour(0.45, juce::Colours::transparentBlack);
    g.setGradientFill(vignette);
    g.fillAll();
    return image;
}
} // namespace keys::ui
