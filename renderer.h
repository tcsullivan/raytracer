#ifndef RENDERER_H
#define RENDERER_H

#include <algorithm>
#include <atomic>
#include <memory>
#include <ranges>
#include <semaphore>
#include <thread>

class Renderer
{
public:
    static constexpr int MaxThreads = 64;

private:
    int N;
    std::counting_semaphore<MaxThreads> Workers;
    std::atomic_uint processed;
    unsigned total;
    std::atomic_bool Stop;
    std::unique_ptr<std::thread> primary;

public:
    Renderer(int n, auto func, unsigned width, unsigned height, auto pbuf):
        N(n), Workers(N), processed(0), total(N * 8)
    {
        Stop.store(false);

        auto threads = std::views::transform(
            std::views::chunk(
                std::views::cartesian_product(
                    std::views::iota(0u, width),
                    std::views::iota(0u, height),
                    std::views::single(pbuf)),
                width * height / total),
            [=, this](auto chunk) { return std::thread([=, this] { worker(func, chunk); }); });

        primary.reset(new std::thread([=, this] {
            for (auto th : threads) {
                Workers.acquire();
                th.detach();
            }
            for (int i : std::views::iota(0, N))
                Workers.acquire();
            Stop.store(true);
        }));
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
        if (primary->joinable())
            primary->join();
    }

private:
    void worker(auto func, auto chunk) {
        for (auto xyb : chunk) {
            if (Stop.load())
                break;
            std::apply(func, xyb);
        }

        processed++;
        Workers.release();
    }
};

#endif // RENDERER_H

