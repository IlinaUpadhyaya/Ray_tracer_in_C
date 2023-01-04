
#ifndef CLION_RENDER_H
#define CLION_RENDER_H

#include "Scene.h"

typedef struct Thread_Data {
    vec3_t *pixels;
    int width;
    int height;
    int thread_no;
    int rays_per_pixel_side;
    entity_t *entities;
    int total_entities;
    point_light_t *point_lights;
    int quality;
    int no_of_threads;
} thread_data_t;

void setup_point_lights(point_light_t point_lights[]);

void init();

void *render(void *data);

#endif //CLION_RENDER_H
