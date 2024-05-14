CXXFLAGS := -std=c++23 -O3 -ggdb -g3 -march=native -mtune=native `sdl2-config --cflags` -Iimgui -Iimgui/backends
LDFLAGS := `sdl2-config --libs`

all: main

main: imgui/imgui.o imgui/imgui_draw.o imgui/imgui_widgets.o imgui/imgui_tables.o imgui/backends/imgui_impl_sdl2.o imgui/backends/imgui_impl_sdlrenderer2.o

image.ppm: main
	time ./main > image.ppm

clean:
	rm -f main image.ppm

view: image.ppm
	feh image.ppm

