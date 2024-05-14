constexpr unsigned Width = 1000;
constexpr double   Aspect = 16.0 / 9.0;
constexpr unsigned Height = Width / Aspect;
constexpr unsigned Threads = 8;

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

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <ranges>
#include <thread>

static World world;

static View Camera;
static int SamplesPerPixel = 20;
static std::unique_ptr<Renderer<Threads>> renderer;
static std::chrono::time_point<std::chrono::high_resolution_clock> renderStart;
static std::chrono::duration<double> renderTime;

static color ray_color(const ray& r, int depth = 50);
static void initiateRender(SDL_Surface *canvas);

int main()
{
    SDL_Init(SDL_INIT_VIDEO);
    auto window = SDL_CreateWindow("raytrace", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Width, Height, SDL_WINDOW_RESIZABLE);
    auto canvas = SDL_CreateRGBSurfaceWithFormat(0, Width, Height, 32, SDL_PIXELFORMAT_RGBA8888);
    auto painter = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC /*| SDL_RENDERER_ACCELERATED*/);

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForSDLRenderer(window, painter);
    ImGui_ImplSDLRenderer2_Init(painter);

    world.add(point3( 0.00, -100.50, -1.0), 100.0, Material::Lambertian, color(0.5, 1.0, 0.5));
    world.add(point3(-0.50,    0.00, -1.2),   0.5, Material::Dielectric, color(1.0, 0.8, 0.8));
    world.add(point3( 0.50,    0.00, -1.0),   0.5, Material::Metal,      color(0.5, 0.5, 0.5));
    world.add(point3(-0.05,   -0.35, -0.7),   0.1, Material::Metal,      color(0.8, 0.6, 0.0));

    std::cout << "Spawning threads..." << std::endl;
    initiateRender(canvas);

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
        ImGui::SliderFloat("fov", &Camera.fieldOfView, 10, 160);
        ImGui::SetNextItemWidth(100); ImGui::InputDouble("X", &Camera.camera.x(), 0.1, 0.05, "%.2lf");
        ImGui::SameLine(); ImGui::SetNextItemWidth(100); ImGui::InputDouble("Y", &Camera.camera.y(), 0.1, 0.05, "%.2lf");
        ImGui::SameLine(); ImGui::SetNextItemWidth(100); ImGui::InputDouble("Z", &Camera.camera.z(), 0.1, 0.05, "%.2lf");
        ImGui::SliderInt("samples", &SamplesPerPixel, 1, 200);
        if (ImGui::Button("recalculate")) {
            initiateRender(canvas);
        }
        ImGui::SameLine();
        if (ImGui::Button("exit")) {
            renderer->stop();
            run = false;
        }
        if (*renderer) {
            ImGui::SameLine();
            if (ImGui::Button("stop")) {
                renderer->stop();
            }
            ImGui::Text("wait... %u%%", renderer->progress());
        } else if (renderTime == std::chrono::duration<double>::zero()) {
            renderTime = std::chrono::high_resolution_clock::now() - renderStart;
        } else {
            ImGui::Text("%0.6lfs", renderTime.count());
        }
        ImGui::End();

        ImGui::Begin("balls", nullptr, ImGuiWindowFlags_AlwaysAutoResize); {
            char radius[] = "radius 0";
            char mat[] = "mat 0";
            char xpos[] = "x 0";
            char ypos[] = "y 0";
            char zpos[] = "z 0";
            for (auto& o : std::views::drop(world.objects, 1)) {
                ImGui::Combo(mat, reinterpret_cast<int *>(&o.M), "Lambertian\0Metal\0Dielectric\0");
                ImGui::SameLine(); ImGui::SetNextItemWidth(100); ImGui::InputDouble(radius, &o.radius, 0.1, 0.05, "%.2lf");
                ImGui::SetNextItemWidth(100); ImGui::InputDouble(xpos, &o.center.x(), 0.05, 0.05, "%.2lf");
                ImGui::SameLine(); ImGui::SetNextItemWidth(100); ImGui::InputDouble(ypos, &o.center.y(), 0.1, 0.05, "%.2lf");
                ImGui::SameLine(); ImGui::SetNextItemWidth(100); ImGui::InputDouble(zpos, &o.center.z(), 0.1, 0.05, "%.2lf");
                radius[7]++;
                mat[4]++;
                xpos[2]++;
                ypos[2]++;
                zpos[2]++;
            }
        } ImGui::End();

        auto tex = SDL_CreateTextureFromSurface(painter, canvas);

        ImGui::Render();
        SDL_RenderClear(painter);
        SDL_RenderCopy(painter, tex, nullptr, nullptr);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(painter);
        SDL_DestroyTexture(tex);

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
        const auto a = 0.5 * (unitDir.y() + 1.0);
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
    renderer.reset(new Renderer<Threads>(func, Width, Height, (uint32_t *)canvas->pixels));
}
