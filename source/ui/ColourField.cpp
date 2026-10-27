#include "ColourField.h"

#include "KeysLookAndFeel.h"
#include "engine/TineModel.h"

namespace keys::ui
{
namespace
{
constexpr int kLightWidth = 144;      // the light's buffer width; its height follows the window
constexpr float kTwoPi = 6.2831853f;
constexpr float kDesignWidth = 1040.0f; // the mockup's CSS pixels map to the window at this width

using Rgb = ColourField::Rgb;

Rgb rgb(juce::uint32 argb)
{
    const juce::Colour c(argb);
    return { c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue() };
}

Rgb rgb(juce::Colour c)
{
    return { c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue() };
}

Rgb mix(Rgb a, Rgb b, float t)
{
    return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

/// "source-over" a flat colour with alpha `a`.
void over(Rgb& c, Rgb s, float a)
{
    c.r += (s.r - c.r) * a;
    c.g += (s.g - c.g) * a;
    c.b += (s.b - c.b) * a;
}

/// "screen": light adds to light, never past white.
void screen(Rgb& c, Rgb s, float a)
{
    c.r += a * s.r * (1.0f - c.r);
    c.g += a * s.g * (1.0f - c.g);
    c.b += a * s.b * (1.0f - c.b);
}

/// "soft-light", per the W3C compositing spec.
float softLight(float cb, float cs)
{
    if (cs <= 0.5f)
        return cb - (1.0f - 2.0f * cs) * cb * (1.0f - cb);
    const float d = cb <= 0.25f ? ((16.0f * cb - 12.0f) * cb + 4.0f) * cb : std::sqrt(cb);
    return cb + (2.0f * cs - 1.0f) * (d - cb);
}

float clamp01(float x)
{
    return std::clamp(x, 0.0f, 1.0f);
}

const Rgb kTine = rgb(0xffff707c);
const Rgb kGlowCore = rgb(0xffffece6);
const Rgb kBarkCore = rgb(0xffff3248);
} // namespace

ColourField::ColourField()
{
    setInterceptsMouseClicks(false, false);
    setOpaque(true);

    // One or two forms of light per colour knob, scattered across the window.
    const std::pair<int, juce::uint32> defs[] = { { 0, 0xffd64a3a }, { 0, 0xffc2412f }, { 1, 0xffe8263f },
                                                  { 2, 0xffb02a84 }, { 2, 0xff7e2496 }, { 3, 0xff93708a },
                                                  { 4, 0xff6a46dc }, { 5, 0xff2856b0 }, { 5, 0xff14606e } };
    const float count = static_cast<float>(std::size(defs));
    for (size_t i = 0; i < std::size(defs); ++i)
    {
        auto r = [this] { return random.nextFloat(); };
        Form f {};
        f.knob = defs[i].first;
        f.colour = rgb(defs[i].second);
        f.cx = (static_cast<float>(i) + 0.5f) / count * 0.9f + 0.05f + (r() - 0.5f) * 0.1f;
        f.cy = 0.2f + r() * 0.55f;
        f.ax = 0.08f + r() * 0.18f;
        f.ay = 0.06f + r() * 0.14f;
        f.fx = 0.4f + r() * 0.8f;
        f.fy = 0.3f + r() * 0.7f;
        f.px = r() * kTwoPi;
        f.py = r() * kTwoPi;
        f.size = 0.22f + r() * 0.16f;
        f.rotation = r() * 3.14f;
        f.spin = (r() - 0.5f) * 0.5f;
        f.stretch = 1.5f + r() * 1.4f;
        forms.push_back(f);
    }
    for (int i = 0; i < 110; ++i)
        specks.push_back({ random.nextFloat(), random.nextFloat(), 0.15f + random.nextFloat() * 0.55f,
                           0.4f + random.nextFloat() * 0.9f, random.nextFloat() * kTwoPi, 0.5f + random.nextFloat() * 2.0f });

    // A form's brightness against its squared distance from the centre: the mockup's gradient stops
    // (1 → 0.55 at 35 % → 0.16 at 70 % → 0), looked up by d² so drawing needs no square roots.
    for (size_t i = 0; i < falloff.size(); ++i)
    {
        const float d = std::sqrt(static_cast<float>(i) / static_cast<float>(falloff.size() - 1));
        falloff[i] = d < 0.35f ? 1.0f - 0.45f * d / 0.35f
                     : d < 0.7f ? 0.55f - 0.39f * (d - 0.35f) / 0.35f
                                : 0.16f * (1.0f - (d - 0.7f) / 0.3f);
    }
}

std::array<float, 6> ColourField::knobValues() const noexcept
{
    return { inputs.warmth, inputs.bark, inputs.body, inputs.patina, inputs.motion, inputs.space };
}

bool ColourField::noteUsesVibes(int note) const noexcept
{
    return inputs.bellVoice == 1 || (inputs.bellVoice == 2 && note < 60);
}

float ColourField::bellShare(int note) const noexcept
{
    const float tine = inputs.blend <= 0.5f ? 1.0f : (1.0f - inputs.blend) * 2.0f;
    const float range = clamp01((note - inputs.bellLowestKey + 6) / 6.0f);
    const float bell = (inputs.blend >= 0.5f ? 1.0f : inputs.blend * 2.0f) * range;
    return bell / std::max(0.001f, tine + bell);
}

// -------------------------------------------------------------------------------------------------
//  Notes
// -------------------------------------------------------------------------------------------------
void ColourField::noteOn(int note, float velocity)
{
    const float share = bellShare(note);
    const Rgb bellColour = rgb(noteUsesVibes(note) ? colours::vibes : colours::bell);
    const float natural = static_cast<float>(keys::tineDecaySeconds(note) * 1.1);
    const float x = std::clamp((note - 30) / 70.0f, 0.04f, 0.96f);
    const float y = 0.58f + (random.nextFloat() - 0.5f) * 0.08f;
    glows.push_back({ note, x, y, velocity, 0.25f + 0.75f * velocity, std::clamp(natural / 4.0f, 0.6f, 2.6f),
                      mix(kTine, bellColour, share * 0.7f) });
    if (glows.size() > 60)
        glows.erase(glows.begin());

    const float bell = inputs.blend >= 0.5f ? 1.0f : inputs.blend * 2.0f;
    const int count = juce::roundToInt(bell * clamp01((note - inputs.bellLowestKey + 6) / 6.0f) * (3.0f + velocity * 7.0f));
    for (int i = 0; i < count; ++i)
    {
        const float life = (noteUsesVibes(note) ? 1.6f : 0.8f) + random.nextFloat() * 1.4f;
        sparks.push_back({ x + (random.nextFloat() - 0.5f) * 0.08f, y + (random.nextFloat() - 0.7f) * 0.3f, life, life,
                           random.nextFloat() * kTwoPi, 6.0f + random.nextFloat() * 10.0f, bellColour });
    }
}

void ColourField::noteOff(int note)
{
    for (auto& glow : glows)
        if (glow.note == note && glow.held)
        {
            glow.held = false;
            if (pedalDown)
                glow.sustained = true;
            else
                glow.tau = std::min(glow.tau, 0.35f);
        }
}

void ColourField::setPedal(bool down)
{
    pedalDown = down;
    if (down)
        return;
    for (auto& glow : glows)
        if (glow.sustained)
        {
            glow.sustained = false;
            glow.tau = std::min(glow.tau, 0.35f);
        }
}

// -------------------------------------------------------------------------------------------------
//  Animation
// -------------------------------------------------------------------------------------------------
void ColourField::advance(double dt)
{
    dt = std::clamp(dt, 0.0, 0.1);
    seconds += dt;
    clock += dt * (still ? 0.004 : 0.05 + inputs.motion * 0.8);

    const float step = static_cast<float>(dt);
    for (auto& glow : glows)
    {
        glow.energy *= std::exp(-step / glow.tau);
        glow.y -= step * 0.012f;
    }
    std::erase_if(glows, [](const Glow& g) { return g.energy < 0.01f; });
    for (auto& spark : sparks)
    {
        spark.life -= step;
        spark.y -= step * 0.02f;
    }
    std::erase_if(sparks, [](const Spark& s) { return s.life <= 0.0f; });

    if (!still)
        grainOffset = { random.nextInt(180), random.nextInt(180) };
    scratchVisible = inputs.dust > 0.25f && !still && random.nextFloat() < (inputs.dust - 0.25f) * 0.5f;
    if (scratchVisible)
    {
        scratchX = random.nextFloat();
        scratchLength = 0.3f + random.nextFloat() * 0.7f;
        scratchAlpha = 0.08f + random.nextFloat() * 0.15f;
    }

    renderLight();
    repaint();
}

void ColourField::resized()
{
    lightWidth = kLightWidth;
    lightHeight = std::max(48, juce::roundToInt(static_cast<float>(kLightWidth) * static_cast<float>(getHeight())
                                                / static_cast<float>(std::max(1, getWidth()))));
    const auto size = static_cast<size_t>(lightWidth * lightHeight);
    base.assign(size, {});
    hazeFalloff.assign(size, 0.0f);
    vignetteFalloff.assign(size, 0.0f);
    light.assign(size, {});
    scratch.assign(size, {});
    lightImage = juce::Image(juce::Image::RGB, lightWidth, lightHeight, false, juce::SoftwareImageType());
    buildBase();
    buildOverlays();
    renderLight(); // the first frame is complete before any animation runs
}

void ColourField::buildBase()
{
    // Navy, with a teal pool low on the left and dark indigo top right.
    const float W = static_cast<float>(lightWidth), H = static_cast<float>(lightHeight), M = std::max(W, H);
    const Rgb navy = rgb(colours::night), teal = rgb(0xff0c3a3c), indigo = rgb(0xff141846);
    for (int y = 0; y < lightHeight; ++y)
        for (int x = 0; x < lightWidth; ++x)
        {
            const float px = static_cast<float>(x) + 0.5f, py = static_cast<float>(y) + 0.5f;
            Rgb c = navy;
            over(c, teal, 0.75f * clamp01(1.0f - std::hypot(px - W * 0.12f, py - H * 0.9f) / (M * 0.75f)));
            over(c, indigo, 0.7f * clamp01(1.0f - std::hypot(px - W * 0.85f, py - H * 0.15f) / (M * 0.7f)));
            base[static_cast<size_t>(y * lightWidth + x)] = c;

            // Space's haze glows around a point low on the right.
            hazeFalloff[static_cast<size_t>(y * lightWidth + x)] = clamp01(1.0f - std::hypot(px - W * 0.75f, py - H * 0.7f) / (M * 0.8f));

            // The vignette: an ellipse 115 % × 90 % of the window around a point a little above the
            // middle, clear inside 38 %. (The light is drawn 10 % larger than the window, so map each
            // pixel back to window fractions first.)
            const float wx = (px / W) * 1.1f - 0.05f, wy = (py / H) * 1.1f - 0.05f;
            const float d = std::hypot((wx - 0.5f) / 1.15f, (wy - 0.42f) / 0.9f);
            vignetteFalloff[static_cast<size_t>(y * lightWidth + x)] = clamp01((d - 0.38f) / 0.62f);
        }
}

void ColourField::buildOverlays()
{
    // Film grain: a noise tile whose light pixels brighten and dark pixels darken (like "overlay").
    juce::Random noise(1977);
    grainTile = juce::Image(juce::Image::ARGB, 180, 180, true, juce::SoftwareImageType());
    {
        juce::Image::BitmapData pixels(grainTile, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 180; ++y)
            for (int x = 0; x < 180; ++x)
            {
                const float n = noise.nextFloat() - 0.5f;
                pixels.setPixelColour(x, y, n > 0.0f ? juce::Colours::white.withAlpha(n * 0.45f)
                                                     : juce::Colours::black.withAlpha(-n * 0.9f));
            }
    }
}

void ColourField::drawForm(float x, float y, float radius, float rotation, float stretch, Rgb colour, float alpha)
{
    if (alpha <= 0.002f || radius <= 0.2f)
        return;
    // The form is an ellipse (radius × stretch along its rotated axis, radius across): only visit
    // the pixels inside its bounding box.
    const float c = std::cos(rotation), s = std::sin(rotation);
    const float a = radius * stretch, b = radius;
    const float extentX = std::sqrt(a * a * c * c + b * b * s * s), extentY = std::sqrt(a * a * s * s + b * b * c * c);
    const int x0 = std::max(0, static_cast<int>(x - extentX)), x1 = std::min(lightWidth - 1, static_cast<int>(x + extentX) + 1);
    const int y0 = std::max(0, static_cast<int>(y - extentY)), y1 = std::min(lightHeight - 1, static_cast<int>(y + extentY) + 1);
    const float invStretch = 1.0f / stretch, invR2 = 1.0f / (radius * radius);
    const float lastIndex = static_cast<float>(falloff.size() - 1);
    for (int py = y0; py <= y1; ++py)
    {
        const float dy = static_cast<float>(py) + 0.5f - y;
        Rgb* row = light.data() + py * lightWidth;
        for (int px = x0; px <= x1; ++px)
        {
            const float dx = static_cast<float>(px) + 0.5f - x;
            const float u = (dx * c + dy * s) * invStretch, v = dy * c - dx * s;
            const float d2 = (u * u + v * v) * invR2;
            if (d2 >= 1.0f)
                continue;
            screen(row[px], colour, alpha * falloff[static_cast<size_t>(d2 * lastIndex)]);
        }
    }
}

void ColourField::renderLight()
{
    if (light.empty())
        return;
    const float W = static_cast<float>(lightWidth), H = static_cast<float>(lightHeight), M = std::max(W, H);
    const float t = static_cast<float>(clock);
    const auto values = knobValues();
    std::copy(base.begin(), base.end(), light.begin());

    // 1. Light forms, one or two per knob, smeared by Motion.
    const float smear = inputs.motion;
    for (const auto& f : forms)
    {
        const float v = values[static_cast<size_t>(f.knob)];
        if (v < 0.01f)
            continue;
        const float x = W * (f.cx + f.ax * std::sin(f.px + t * f.fx));
        const float y = H * (f.cy + f.ay * std::sin(f.py + t * f.fy * 1.3f));
        const float r = M * f.size * (0.45f + 0.8f * v) * (0.7f + inputs.body * 0.6f);
        const float rot = f.rotation + t * f.spin, st = f.stretch * (1.0f + smear * 0.8f);
        const float a = (0.14f + 0.6f * v) * (f.knob == 3 ? 0.6f : 1.0f);
        drawForm(x, y, r, rot, st, f.colour, a);
        if (smear > 0.05f)
            for (int k = 1; k <= 3; ++k)
            {
                const float d = r * 0.28f * smear * static_cast<float>(k);
                drawForm(x - std::cos(rot) * d, y - std::sin(rot) * d, r * (1.0f - static_cast<float>(k) * 0.08f), rot, st,
                         f.colour, a * std::pow(0.45f, static_cast<float>(k)));
            }
    }

    // 2. Bark: two crimson cores that flicker.
    if (inputs.bark > 0.02f)
        for (int i = 0; i < 2; ++i)
        {
            const float fi = static_cast<float>(i);
            const float flicker = 0.55f + 0.45f * std::sin(t * 9.0f + fi * 2.3f) * std::sin(t * 5.1f + fi);
            drawForm(W * (0.38f + fi * 0.26f + 0.05f * std::sin(t * 0.7f + fi)), H * (0.42f + 0.1f * std::sin(t * 0.9f + fi * 1.7f)),
                     W * (0.06f + 0.06f * inputs.bark), t * 0.2f + fi, 1.6f, kBarkCore, inputs.bark * 0.55f * flicker);
        }

    // 3. A glow for every note: a wide soft bloom and a small bright core.
    for (const auto& glow : glows)
    {
        const float r = W * (0.05f + 0.08f * glow.velocity) * (0.8f + inputs.body * 0.5f);
        drawForm(glow.x * W, glow.y * H, r * 1.8f, 1.57f, 1.3f, glow.colour, glow.energy * 0.45f);
        drawForm(glow.x * W, glow.y * H, r * 0.45f, 0.0f, 1.0f, kGlowCore, glow.energy * 0.45f);
    }

    // 4. The grade: Warmth's wash, Space's haze, Patina's faded blacks, then saturation, contrast
    //    and sepia (the mockup's CSS filter), and finally a blur that grows with Space.
    const float p = inputs.patina;
    const Rgb wash = rgb(colours::warmth), haze = rgb(0xff2856b0), lift = rgb(0xff463c42);
    const float washAlpha = inputs.warmth * 0.22f, hazeAlpha = inputs.space * 0.32f, liftAlpha = p * 0.14f;
    const float sat = 1.2f - p * 0.45f + inputs.bark * 0.15f, contrast = 1.06f - p * 0.12f, sepia = p * 0.2f;
    for (int y = 0; y < lightHeight; ++y)
        for (int x = 0; x < lightWidth; ++x)
        {
            Rgb& c = light[static_cast<size_t>(y * lightWidth + x)];
            c.r += (softLight(c.r, wash.r) - c.r) * washAlpha;
            c.g += (softLight(c.g, wash.g) - c.g) * washAlpha;
            c.b += (softLight(c.b, wash.b) - c.b) * washAlpha;
            screen(c, haze, hazeAlpha * hazeFalloff[static_cast<size_t>(y * lightWidth + x)]);
            over(c, lift, liftAlpha);

            const Rgb s { (0.213f + 0.787f * sat) * c.r + (0.715f - 0.715f * sat) * c.g + (0.072f - 0.072f * sat) * c.b,
                          (0.213f - 0.213f * sat) * c.r + (0.715f + 0.285f * sat) * c.g + (0.072f - 0.072f * sat) * c.b,
                          (0.213f - 0.213f * sat) * c.r + (0.715f - 0.715f * sat) * c.g + (0.072f + 0.928f * sat) * c.b };
            Rgb k { (s.r - 0.5f) * contrast + 0.5f, (s.g - 0.5f) * contrast + 0.5f, (s.b - 0.5f) * contrast + 0.5f };
            k = { clamp01(k.r), clamp01(k.g), clamp01(k.b) };
            const Rgb sep { 0.393f * k.r + 0.769f * k.g + 0.189f * k.b, 0.349f * k.r + 0.686f * k.g + 0.168f * k.b,
                            0.272f * k.r + 0.534f * k.g + 0.131f * k.b };
            c = mix(k, { clamp01(sep.r), clamp01(sep.g), clamp01(sep.b) }, sepia);
        }

    // The vignette, deeper with Patina.
    const Rgb edge = rgb(0xff020408);
    const float vignetteDepth = 0.92f * (0.5f + p * 0.5f);
    for (size_t i = 0; i < light.size(); ++i)
        over(light[i], edge, vignetteDepth * vignetteFalloff[i]);

    // Gaussian blur, separable: sigma follows Space (4..13 px in the window, scaled to this buffer).
    const float sigma = (4.0f + inputs.space * 9.0f) * W / kDesignWidth;
    const int radius = std::clamp(static_cast<int>(std::ceil(sigma * 2.5f)), 1, 8);
    std::array<float, 17> kernel {};
    float total = 0.0f;
    for (int i = -radius; i <= radius; ++i)
        total += kernel[static_cast<size_t>(i + radius)] = std::exp(-0.5f * static_cast<float>(i * i) / (sigma * sigma));
    for (int i = 0; i <= 2 * radius; ++i)
        kernel[static_cast<size_t>(i)] /= total;
    auto blurPass = [&](const std::vector<Rgb>& from, std::vector<Rgb>& to, bool horizontal)
    {
        for (int y = 0; y < lightHeight; ++y)
            for (int x = 0; x < lightWidth; ++x)
            {
                Rgb sum;
                for (int i = -radius; i <= radius; ++i)
                {
                    const int sx = horizontal ? std::clamp(x + i, 0, lightWidth - 1) : x;
                    const int sy = horizontal ? y : std::clamp(y + i, 0, lightHeight - 1);
                    const Rgb& src = from[static_cast<size_t>(sy * lightWidth + sx)];
                    const float w = kernel[static_cast<size_t>(i + radius)];
                    sum.r += src.r * w;
                    sum.g += src.g * w;
                    sum.b += src.b * w;
                }
                to[static_cast<size_t>(y * lightWidth + x)] = sum;
            }
    };
    blurPass(light, scratch, true);
    blurPass(scratch, light, false);

    juce::Image::BitmapData pixels(lightImage, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < lightHeight; ++y)
        for (int x = 0; x < lightWidth; ++x)
        {
            const Rgb& c = light[static_cast<size_t>(y * lightWidth + x)];
            pixels.setPixelColour(x, y, juce::Colour::fromFloatRGBA(clamp01(c.r), clamp01(c.g), clamp01(c.b), 1.0f));
        }
}

// -------------------------------------------------------------------------------------------------
//  Painting
// -------------------------------------------------------------------------------------------------
void ColourField::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth(), h = area.getHeight();
    const float unit = w / kDesignWidth; // one mockup pixel

    // The light, upscaled (10 % larger than the window, so the blurred edges stay outside).
    g.fillAll(colours::night);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(lightImage, area.withSizeKeepingCentre(w * 1.1f, h * 1.1f));

    // Film specks: more with Dust, twinkling unless the background is still.
    const float time = static_cast<float>(seconds);
    const int speckCount = std::min(static_cast<int>(specks.size()), juce::roundToInt(30.0f + inputs.dust * 80.0f));
    for (int i = 0; i < speckCount; ++i)
    {
        const auto& sp = specks[static_cast<size_t>(i)];
        const float twinkle = still ? 1.0f : 0.6f + 0.4f * std::sin(sp.phase + time * sp.speed);
        const float r = sp.radius * unit;
        g.setColour(colours::fg.withAlpha(sp.alpha * twinkle));
        g.fillEllipse(sp.x * w - r, sp.y * h - r, r * 2.0f, r * 2.0f);
    }

    // Bell sparkles: a halo and a bright point, twinkling as they fade.
    for (const auto& s : sparks)
    {
        const float twinkle = 0.5f + 0.5f * std::sin(s.phase + time * s.speed);
        const float alpha = clamp01(s.life / s.maxLife) * (0.35f + 0.65f * twinkle);
        const float x = s.x * w, y = s.y * h, halo = 7.0f * unit;
        const auto colour = juce::Colour::fromFloatRGBA(s.colour.r, s.colour.g, s.colour.b, 1.0f);
        g.setGradientFill(juce::ColourGradient(colour.withAlpha(alpha * 0.55f), x, y, colour.withAlpha(0.0f), x + halo, y, true));
        g.fillEllipse(x - halo, y - halo, halo * 2.0f, halo * 2.0f);
        g.setColour(juce::Colour(0xfffffaf0).withAlpha(alpha));
        g.fillEllipse(x - 1.1f * unit, y - 1.1f * unit, 2.2f * unit, 2.2f * unit);
    }

    if (scratchVisible)
    {
        g.setColour(colours::fg.withAlpha(scratchAlpha));
        g.fillRect(scratchX * w, 0.0f, unit, h * scratchLength);
    }

    // Grain, heavier with Patina and Dust.
    g.setTiledImageFill(grainTile, grainOffset.x, grainOffset.y, 0.14f + inputs.patina * 0.26f + inputs.dust * 0.1f);
    g.fillRect(area);
}
} // namespace keys::ui
