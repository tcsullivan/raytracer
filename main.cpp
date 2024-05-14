#include <random>

inline double randomN()
{
    static std::uniform_real_distribution<double> distribution (0.0, 1.0);
    static std::mt19937 generator;
    return distribution(generator);
}

#include "color.h"
#include "ray.h"
#include "renderer.h"
#include "vec3.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <SDL2/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <thread>
#include <tuple>
#include <vector>

constexpr unsigned Width = 1000;
constexpr double   Aspect = 16.0 / 9.0;
constexpr unsigned Height = Width / Aspect;
constexpr unsigned Threads = 8;

enum class Material : int {
    Lambertian,
    Metal,
    Dielectric
};

struct View
{
    static constexpr auto lookat = point3(0, 0, -1); // Point camera is looking at
    static constexpr auto vup    = vec3(0, 1, 0);    // Camera-relative "up" direction

    float fieldOfView = 90.f;
    float focalLength;
    float viewportHeight;
    float viewportWidth;

    point3 camera;
    vec3 viewportX;
    vec3 viewportY;
    vec3 pixelDX;
    vec3 pixelDY;
    vec3 viewportUL;
    vec3 pixelUL;

    View() {
        recalculate();
    }

    void recalculate() {
        focalLength = (camera - lookat).length();
        viewportHeight = 2 * std::tan(fieldOfView * 3.14159265 / 180.0 / 2.0) * focalLength;
        viewportWidth  = viewportHeight * Aspect;

        const auto w = (camera - lookat).normalize();
        const auto u = cross(vup, w).normalize();
        const auto v = cross(w, u);

        viewportX = viewportWidth * u;
        viewportY = -viewportHeight * v;

        pixelDX = viewportX / Width;
        pixelDY = viewportY / Height;
        viewportUL = camera - focalLength * w - viewportX / 2 - viewportY / 2;
        pixelUL = viewportUL + 0.5 * (pixelDX + pixelDY);
    }

    ray getRay(int x, int y, bool addRandom = false) const {
        double X = x;
        double Y = y;

        if (addRandom) {
            X += randomN() - 0.5;
            Y += randomN() - 0.5;
        }

        auto pixel = pixelUL + X * pixelDX + Y * pixelDY;
        return ray(camera, pixel - camera);
    }
};

struct Sphere
{
    point3 center;
    double radius;
    Material M;
    color tint;

    std::pair<color, ray> scatter(const ray& r, double root) const {
        const auto p = r.at(root);
        auto normal = (p - center) / radius;

        if (M == Material::Lambertian) {
            return {tint, ray(p, normal + randomUnitSphere())};
        } else if (M == Material::Metal) {
            return {tint, ray(p, r.direction().reflect(normal))};
        } else if (M == Material::Dielectric) {
            constexpr auto index = 1.0 / 1.33;

            const bool front = r.direction().dot(normal) < 0;
            const auto ri = front ? 1.0 / index : index;
            if (!front)
                normal *= -1;

            const auto dir = r.direction().normalize();
            const double costh = std::fmin((-dir).dot(normal), 1);
            const double sinth = std::sqrt(1 - costh * costh);

            if (ri * sinth > 1)
                return {color(1, 1, 1), ray(p, dir.reflect(normal))};
            else
                return {color(1, 1, 1), ray(p, dir.refract(normal, ri))};
        } else {
            return {};
        }
    }

    std::optional<double> hit(const ray& r, double tmin, double tmax) const {
        const vec3 oc = center - r.origin();
        const auto a = r.direction().length_squared();
        const auto h = r.direction().dot(oc);
        const auto c = oc.length_squared() - radius * radius;
        const auto discriminant = h * h - a * c;

        if (discriminant < 0) {
            return {}; // No hit
        } else {
            const auto sqrtd = sqrt(discriminant);

            // Find the nearest root that lies in the acceptable range.
            auto root = (h - sqrtd) / a;
            if (root <= tmin || tmax <= root) {
                root = (h + sqrtd) / a;
                if (root <= tmin || tmax <= root)
                    return {};
            }

            return root;
        }
    }
};

struct World
{
    std::vector<Sphere> objects;

    void add(auto&&... args) {
        objects.emplace_back(args...);
    }

    std::optional<std::pair<double, Sphere>> hit(const ray& r) const {
        double closest = std::numeric_limits<double>::infinity();
        Sphere sphere;

        for (const auto& o : objects) {
            if (auto t = o.hit(r, 0.001, closest); t) {
                closest = *t;
                sphere = o;
            }
        }

        if (closest != std::numeric_limits<double>::infinity())
            return std::pair {closest, sphere};
        else
            return {};
    }
};

static World world;

static color ray_color(const ray& r, int depth = 50)
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

static View Camera;
static int SamplesPerPixel = 20;
static std::unique_ptr<Renderer<Threads>> renderer;
static std::chrono::time_point<std::chrono::high_resolution_clock> renderStart;
static std::chrono::duration<double> renderTime;

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

        ImGui::Render();
        //SDL_RenderSetScale(painter, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_RenderClear(painter);
        auto tex = SDL_CreateTextureFromSurface(painter, canvas);
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

