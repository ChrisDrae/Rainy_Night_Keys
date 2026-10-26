#include "DemoLibrary.h"

#include "BankBuilder.h"
#include "TineModel.h"

#include <algorithm>
#include <thread>

namespace keys
{
std::unique_ptr<SampleBank> buildDemoTines(const DemoTineOptions& options, const std::atomic<bool>* cancel)
{
    struct Job
    {
        int root = 60;
        int layer = 0;
        std::unique_ptr<Sample> sample;
    };

    std::vector<Job> jobs;
    for (int root = options.lowestRoot; root <= options.highestRoot; root += std::max(1, options.rootStep))
        for (int layer = 0; layer < static_cast<int>(options.layerTopVelocities.size()); ++layer)
            jobs.push_back({ root, layer, nullptr });

    // Render in parallel: each worker grabs the next job index until none are left.
    std::atomic<size_t> nextJob { 0 };
    auto worker = [&]
    {
        for (size_t j = nextJob++; j < jobs.size(); j = nextJob++)
        {
            if (cancel != nullptr && cancel->load())
                return;
            auto& job = jobs[j];
            const float velocity = static_cast<float>(options.layerTopVelocities[static_cast<size_t>(job.layer)]) / 127.0f;
            const double seconds = std::min(options.maxSeconds, tineRenderSeconds(job.root));
            const int frames = static_cast<int>(seconds * options.sampleRate);

            auto sample = std::make_unique<Sample>();
            sample->audio.setSize(1, frames);
            sample->sampleRate = options.sampleRate;
            sample->rootNote = job.root;
            sample->name = "demo tine " + juce::String(job.root) + " v" + juce::String(job.layer + 1);
            renderTine(sample->audio.getWritePointer(0), frames, options.sampleRate, job.root, velocity,
                       static_cast<uint32_t>(job.root * 7 + job.layer));
            job.sample = std::move(sample);
        }
    };

    const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
    const unsigned count = options.threads > 0 ? static_cast<unsigned>(options.threads) : std::min(hardware, 8u);
    std::vector<std::thread> workers;
    for (unsigned t = 1; t < count; ++t)
        workers.emplace_back(worker);
    worker(); // this thread helps too
    for (auto& w : workers)
        w.join();

    if (cancel != nullptr && cancel->load())
        return nullptr;

    auto bank = std::make_unique<SampleBank>("Demo tines");
    std::vector<RootSample> roots;
    for (auto& job : jobs)
        roots.push_back({ job.layer, std::move(job.sample) });
    mapRootsIntoBank(*bank, std::move(roots), options.layerTopVelocities);
    bank->finalize();
    bank->normalizeTo(0.32f);
    return bank;
}

std::unique_ptr<SampleBank> buildModeledBell(double sampleRate)
{
    const std::vector<int> layers { 70, 127 };
    std::vector<RootSample> roots;
    for (int root = 60; root <= 104; root += 4)
    {
        for (int layer = 0; layer < 2; ++layer)
        {
            const int frames = static_cast<int>(3.0 * sampleRate);
            auto sample = std::make_unique<Sample>();
            sample->audio.setSize(1, frames);
            sample->sampleRate = sampleRate;
            sample->rootNote = root;
            sample->name = "modeled bell " + juce::String(root);
            renderModeledBell(sample->audio.getWritePointer(0), frames, sampleRate, root, layer == 0 ? 0.5f : 1.0f,
                              static_cast<uint32_t>(root));
            roots.push_back({ layer, std::move(sample) });
        }
    }
    auto bank = std::make_unique<SampleBank>("Modeled bell");
    mapRootsIntoBank(*bank, std::move(roots), layers);
    bank->finalize();
    bank->normalizeTo(0.22f);
    return bank;
}
} // namespace keys
