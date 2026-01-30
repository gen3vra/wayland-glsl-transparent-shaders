i libwayland-dev libwayland-egl1 libwayland-egl++1 libegl1-mesa-dev libgl-dev wayland-protocols

Generate headers then
`gcc -c xdg-shell.c -o xdg-shell.o`

Build:
`g++ wgts.cpp xdg-shell.o -o wayshaders -lwayland-client -lwayland-egl -lEGL -lGL`

