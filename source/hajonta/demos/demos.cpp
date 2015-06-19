#pragma once

static const demo_data menu_items[] = {
    {"Menu", (demo_func *)demo_menu},
    {"Rainbow", (demo_func *)demo_rainbow},
    {"Normals", (demo_func *)demo_normals},
    {"Collision", (demo_func *)demo_collision},
    {"Bounce", (demo_func *)demo_bounce},
    {"Rotate", (demo_func *)demo_rotate},
    {"Model", (demo_func *)demo_model},
    {"Firework", (demo_func *)demo_firework},
};

#include "hajonta/demos/rainbow.cpp"
#include "hajonta/demos/bounce.cpp"
#include "hajonta/demos/normals.cpp"
#include "hajonta/demos/collision.cpp"
#include "hajonta/demos/menu.cpp"
#include "hajonta/demos/rotate.cpp"
#include "hajonta/demos/model.cpp"
#include "hajonta/demos/firework.cpp"
