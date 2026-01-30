wayland-scanner client-header \
  /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  xdg-shell.h

wayland-scanner private-code \
  /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  xdg-shell.c

gcc -c xdg-shell.c -o xdg-shell.o
g++ wgts.cpp xdg-shell.o -o wayshaders -lwayland-client -lwayland-egl -lEGL -lGL