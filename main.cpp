constexpr unsigned Width = 1000;
constexpr double   Aspect = 16.0 / 9.0;
constexpr unsigned Height = Width / Aspect;

#include "color.h"
#include "ray.h"
#include "renderer.h"
#include "sphere.h"
#include "vec3.h"
#include "view.h"
#include "world.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <ranges>
#include <thread>
#include <utility>

static View Camera;
static World world;
static int threads = 4;
static int SamplesPerPixel = 20;
static int SamplesPerPixelTmp = 20;
static float Daylight = 0.5f;
static std::unique_ptr<Renderer> renderer;
static std::chrono::time_point<std::chrono::high_resolution_clock> renderStart;
static std::chrono::duration<double> renderTime;

static color ray_color(const ray& r, int depth = 50);
static void initiateRender(SDL_Surface *canvas);
static void showObjectControls(int index, Sphere& o);
static void addRandomObject();
static void preview(SDL_Surface *canvas);

int main()
{
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    auto window = SDL_CreateWindow("raytrace", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Width, Height, SDL_WINDOW_RESIZABLE);
    auto canvas = SDL_CreateRGBSurfaceWithFormat(0, Width, Height, 32, SDL_PIXELFORMAT_RGBA8888);
    auto painter = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC /*| SDL_RENDERER_ACCELERATED*/);

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForSDLRenderer(window, painter);
    ImGui_ImplSDLRenderer2_Init(painter);

    world.add(point3(0.00, -100.50, -1.0), 100.0,
        Material::Lambertian, color(0.5, 1.0, 0.5));
    for (auto i : std::views::iota(0, 10))
        addRandomObject();

    std::cout << "Spawning threads..." << std::endl;
    initiateRender(canvas);
    auto tex = SDL_CreateTextureFromSurface(painter, canvas);

    std::cout << "Entering render..." << std::endl;
    bool run = true;
    for (SDL_Event event; run;) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                renderer->stop();
                run = false;
            }
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        if (ImGui::SliderFloat("fov", &Camera.fieldOfView, 10, 160))
            preview(canvas);
        ImGui::SameLine(); ImGui::SetNextItemWidth(80);
        ImGui::InputInt("T", &threads);
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputDouble("X", &Camera.camera.x(), 0.1, 0.05, "%.2lf"))
            preview(canvas);
        ImGui::SameLine(); ImGui::SetNextItemWidth(100);
        if (ImGui::InputDouble("Y", &Camera.camera.y(), 0.1, 0.05, "%.2lf"))
            preview(canvas);
        ImGui::SameLine(); ImGui::SetNextItemWidth(100);
        if (ImGui::InputDouble("Z", &Camera.camera.z(), 0.1, 0.05, "%.2lf"))
            preview(canvas);
        if (ImGui::SliderInt("samples", &SamplesPerPixel, 1, 200)) {
            SamplesPerPixelTmp = SamplesPerPixel;
        }
        ImGui::SliderFloat("shade", &Daylight, 0.25f, 1.f);

        if (ImGui::Button("recalculate")) {
            initiateRender(canvas);
        }
        ImGui::SameLine();
        if (ImGui::Button("export")) {
            std::string filename ("screenshot_");
            filename += std::to_string(int(randomN() * 1000000));
            filename += ".png";
            IMG_SavePNG(canvas, filename.c_str());
            std::cout << "saved " << filename << std::endl;
        }
        ImGui::SameLine();
        if (ImGui::Button("exit")) {
            renderer->stop();
            run = false;
        }

        if (*renderer) {
            SDL_DestroyTexture(tex);
            tex = SDL_CreateTextureFromSurface(painter, canvas);

            ImGui::SameLine();
            if (ImGui::Button("stop")) {
                renderer->stop();
            }
            ImGui::Text("wait... %u%%", renderer->progress());
        } else if (renderTime == std::chrono::duration<double>::zero()) {
            SDL_DestroyTexture(tex);
            tex = SDL_CreateTextureFromSurface(painter, canvas);

            renderTime = std::chrono::high_resolution_clock::now() - renderStart;
            SamplesPerPixel = SamplesPerPixelTmp;
        } else {
            ImGui::Text("%0.6lfs", renderTime.count());
        }
        ImGui::End();

        ImGui::Begin("balls", nullptr, ImGuiWindowFlags_NoResize); {
            std::ranges::for_each(
                std::views::zip(std::views::iota(0),
                                std::views::drop(world.objects, 1)),
                [](auto io) { std::apply(showObjectControls, io); });

            if (ImGui::Button("add")) {
                addRandomObject();
                initiateRender(canvas);
            }
            if (ImGui::Button("del")) {
                world.objects.pop_back();
                initiateRender(canvas);
            }
        } ImGui::End();

        ImGui::Render();
        SDL_RenderClear(painter);
        SDL_RenderCopy(painter, tex, nullptr, nullptr);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(painter);

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_FreeSurface(canvas);
    SDL_DestroyRenderer(painter);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

color ray_color(const ray& r, int depth)
{
    if (depth <= 0)
        return {};

    if (auto hit = world.hit(r); hit) {
        const auto& [closest, sphere] = *hit;
        const auto [atten, scat] = sphere.scatter(r, closest);
        return atten * ray_color(scat, depth - 1);
    } else {
        const auto unitDir = r.direction().normalize();
        const auto a = Daylight * (unitDir.y() + 1.0);
        return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
    }
}

void initiateRender(SDL_Surface *canvas)
{
    renderStart = std::chrono::high_resolution_clock::now();
    renderTime = std::chrono::duration<double>::zero();

    auto func = [format = canvas->format](auto x, auto y, auto pbuf) {
        auto col = std::ranges::fold_left(std::views::iota(0, SamplesPerPixel), color(),
            [y, x](color c, int i) { return c + ray_color(Camera.getRay(x, y, true)); });

        col = col / SamplesPerPixel * 255;
        pbuf[y * Width + x] = SDL_MapRGBA(format, col.x(), col.y(), col.z(), 255);
    };

    Camera.recalculate();
    threads = std::clamp(threads, 1, Renderer::MaxThreads);
    renderer.reset(new Renderer(threads, func, Width, Height,
        (uint32_t *)canvas->pixels));
}

void showObjectControls(int index, Sphere& o)
{
    const auto idx = std::to_string(index);

    ImGui::SetNextItemWidth(200);
    ImGui::Combo((std::string("mat") + idx).c_str(), reinterpret_cast<int *>(&o.M),
        "Lambertian\0Metal\0Dielectric\0");
    ImGui::SameLine(); ImGui::SetNextItemWidth(100);
    ImGui::InputDouble((std::string("radius") + idx).c_str(),
        &o.radius, 0.1, 0.05, "%.2lf");
    ImGui::SetNextItemWidth(100);
    ImGui::InputDouble((std::string("x") + idx).c_str(),
        &o.center.x(), 0.05, 0.05, "%.2lf");
    ImGui::SameLine(); ImGui::SetNextItemWidth(100);
    ImGui::InputDouble((std::string("y") + idx).c_str(),
        &o.center.y(), 0.1, 0.05, "%.2lf");
    ImGui::SameLine(); ImGui::SetNextItemWidth(100);
    ImGui::InputDouble((std::string("z") + idx).c_str(),
        &o.center.z(), 0.1, 0.05, "%.2lf");
}

void addRandomObject()
{
    const point3 pos = vec3::random() * vec3(6, 0.8, 3) - vec3(3, 0, 3.8);
    const color col = vec3::random();
    const auto mat = (int)(randomN() * (int)Material::Undefined);
    world.add(pos, randomN() * 0.3 + 0.05, (Material)mat, col);
}

void preview(SDL_Surface *canvas)
{
    if (SamplesPerPixel != 1)
        SamplesPerPixelTmp = std::exchange(SamplesPerPixel, 1);
    initiateRender(canvas);
}

