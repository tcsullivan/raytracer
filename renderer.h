#include <algorithm>
#include <atomic>
#include <memory>
#include <ranges>
#include <semaphore>
#include <thread>

template<int Threads>
class Renderer
{
    std::counting_semaphore<Threads> Workers;
    std::atomic_uint processed;
    unsigned total;
    std::atomic_bool Stop;
    std::unique_ptr<std::thread> primary;

public:
    Renderer(auto func, unsigned Width, unsigned Height, auto pbuf):
        Workers(Threads), processed(0), total(Threads * 8)
    {
        Stop.store(false);

        auto threads = std::views::transform(
            std::views::chunk(
                std::views::cartesian_product(
                    std::views::iota(0u, Width),
                    std::views::iota(0u, Height),
                    std::views::single(pbuf)),
                Width * Height / total),
            [=, this](auto chunk) { return std::thread([=, this] { worker(func, chunk); }); });

        primary.reset(new std::thread([=, this] {
            for (auto th : threads) {
                Workers.acquire();
                th.detach();
            }
            for (auto i : std::views::iota(0, Threads))
                Workers.acquire();
            for (auto i : std::views::iota(0, Threads))
                Workers.release();
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


