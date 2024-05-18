#ifndef RENDERER_H
#define RENDERER_H

#include <algorithm>
#include <atomic>
#include <ranges>
#include <thread>

class Renderer
{
public:
    template<typename Fn>
    void start(Fn func, int tn) {
        actives.store(tn);
        processed.store(0);
        Stop.store(false);

        if (primary.joinable())
            primary.join();

        primary = std::thread(&Renderer::dispatchWorkers<Fn>, this, func);
    }

    void setBuffer(std::uint32_t *pixelbuf, unsigned w, unsigned h) {
        pixelBuffer = pixelbuf;
        width = w;
        height = h;
    }

    ~Renderer() {
        stop();
    }

    operator bool() const {
        return !Stop.load();
    }

    unsigned progress() const {
        return processed.load() * 100 / total;
    }

    void stop() {
        Stop.store(true);
        if (primary.joinable())
            primary.join();
    }

private:
    std::uint32_t *pixelBuffer = nullptr;
    unsigned width = 0, height = 0, total = 0;
    std::thread primary;
    std::atomic_uint actives;
    std::atomic_uint processed;
    std::atomic_bool Stop;

    template<typename Fn>
    void dispatchWorkers(Fn func) {
        const auto N = actives.load();
        total = N * 16;
        auto threads = std::views::transform(
            std::views::chunk(
                std::views::cartesian_product(
                    std::views::iota(0u, width),
                    std::views::iota(0u, height)),
                width * height / total),
            [=, this](auto chunk) { return std::thread([=, this] { worker(func, chunk); }); });

        for (auto th : threads) {
            while (actives.load() == 0)
                std::this_thread::yield();
            --actives;
            th.detach();
        }

        while (actives.load() < N)
            std::this_thread::yield();
        Stop.store(true);
    }

    void worker(auto func, auto chunk) {
        for (auto [x, y] : chunk) {
            if (Stop.load())
                break;
            pixelBuffer[y * width + x] = func(x, y);
        }

        ++processed;
        ++actives;
    }
};

#endif // RENDERER_H

