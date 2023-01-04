//
// Created by shara on 24/12/2022.
//

#include "Render.h"

#include <string.h>
#include <math.h>

#define MAXBOUNCES 5
#define PI 3.14159

#define INF 1e20
#define EPSILON 0.000001
#define GAMMA 1.23

#define TOTAL_LIGHTS 2

#define STAGE_1 1
#define STAGE_2 2
#define STAGE_3 3
#define FIELD_OF_VIEW_DEGREES 60


static double clamp(double x) { return x < 0 ? 0 : x > 1 ? 1 : x; }

static void clamp_color_vec(vec3_t *color) {
    color->x = clamp(color->x);
    color->y = clamp(color->y);
    color->z = clamp(color->z);
}

typedef struct Bounce {
    int diff;
    int refl;
    int refr;
} Bounce_t;

static void cast_ray(vec3_t *color, ray_t *ray, Bounce_t *bounce, entity_t entities[],
                     int total_entities, point_light_t lights[], int quality);


static void init_color(vec3_t *color, double r, double g, double b) {
    color->x = r;
    color->y = g;
    color->z = b;
}

static vec3_t BLUE;
static vec3_t RED;
static vec3_t GREEN;
static vec3_t GRAY;
static vec3_t LIGHT;
static vec3_t POINTLIGHT;

void init() {
    init_color(&BLUE, 0.5, 0.5, 1.0);
    init_color(&RED, 1.0, 0.5, 0.5);
    init_color(&GREEN, 0.5, 1.0, 0.5);
    init_color(&GRAY, 0.5, 0.5, 0.5);
    init_color(&LIGHT, 18.0, 18.0, 18.0);
    init_color(&POINTLIGHT, 1.0, 1.0, 1.0);
}

void setup_point_lights(point_light_t point_lights[]) {
    point_lights[0].color = GRAY;
    point_lights[0].location.x = 0.0;
    point_lights[0].location.y = 0.8;
    point_lights[0].location.z = 1.5;

    point_lights[1].color = GRAY;
    point_lights[1].location.x = -0.5;
    point_lights[1].location.y = 0.2;
    point_lights[1].location.z = 0.5;


}

static void apply_point_light_effect(hit_t *hit, vec3_t *color, entity_t entities[], int
total_entities,
                                     point_light_t lights[]) {

    entity_t *entity;
    point_light_t *light;
    int shadow = 0;
    vec3_t ray_direction;
    ray_t ray;
    double t = 0, distance_to_light;
    int i, j;
    for (i = 0; i < TOTAL_LIGHTS; i++) {
        light = &lights[i];
        ray_direction = light->location;;
        vec_subtract(&ray_direction, &hit->location);
        distance_to_light = vec_length(&ray_direction);

        normalize(&ray_direction);
        shadow = 0;

        ray.origin = hit->normal;
        const_mult(&ray.origin, 0.000001);
        vec_add(&ray.origin, &hit->location);

        ray.direction = ray_direction;
        hit_t new_hit;
        new_hit.ignore = 1;
        for (j = 0; j < total_entities; j++) {
            entity = &entities[j];
            t = entity->intersect(entity, &ray, &new_hit);
            if (t > 0 && t <= distance_to_light) {
                shadow = 1;
                break;
            }
        }
        if (shadow)continue;
        /*else update color from light*/
        vec3_t tmp = hit->color_hit;
        vec_mult(&tmp, &light->color);
        const_mult(&tmp, dot(&hit->normal, &ray_direction));
        vec_add(color, &tmp);
    }
}

static double get_reflectance(vec3_t *incident_dir, vec3_t *normal, double ior) {

    double cosi = dot(incident_dir, normal);

    double etai = 1.0, etat = ior;
    if (cosi > 0) {
        etai = etat;
        etat = 1.0;
    }

    double n = 1 - cosi * cosi;
    n = n > 0.0 ? n : 0.0;
    double sint = etai / etat * sqrt(n);
    if (sint >= 1) return 1.0;

    n = 1 - sint * sint;
    n = n > 0.0 ? n : 0.0;
    double cost = sqrt(n);
    cosi = cosi < 0 ? -cosi : cosi;

    double etat_cosi_prod = etat * cosi;
    double etai_cost_prod = etai * cost;
    double etai_cosi_prod = etai * cosi;
    double etat_cost_prod = etat * cost;


    double Rs = (etat_cosi_prod - etai_cost_prod) / (etat_cosi_prod + etai_cost_prod);
    double Rp = (etai_cosi_prod - etat_cost_prod) / (etai_cosi_prod + etat_cost_prod);
    return (Rs * Rs + Rp * Rp) / 2.0;
}

static void set_refracted_direction(vec3_t *refr_dir, vec3_t *incident_dir, vec3_t *normal, double
ior) {

    double cosi = dot(incident_dir, normal);
    double etai = 1.0, etat = ior;
    vec3_t n = *normal;

    if (cosi < 0) { cosi = -cosi; }
    else {
        etai = ior;
        etat = 1.0;
        const_mult(&n, -1.0);
    }

    double eta = etai / etat;
    double k = 1 - eta * eta * (1 - cosi * cosi);

    vec3_t tmp = n;
    if (k < 0.0) { memset(refr_dir, 0, sizeof(vec3_t)); }
    else {
        *refr_dir = *incident_dir;
        const_mult(&tmp, cosi);
        vec_add(refr_dir, &tmp);
        const_mult(refr_dir, eta);
        const_mult(&n, sqrt(k));
        vec_subtract(refr_dir, &n);
    }
}


static void apply_refraction(hit_t *hit, vec3_t *color, Bounce_t *bounce, entity_t entities[],
                             int total_entities, point_light_t lights[], int quality) {

    double reflectance = get_reflectance(&hit->incident, &hit->normal, hit->ior);
    int outside = dot(&hit->incident, &hit->normal) < 0 ? 1 : 0;
    vec3_t bias = hit->normal;
    const_mult(&bias, EPSILON);
    vec3_t tmp_color;
    bounce->refr += 1;

    if (reflectance < 1.0) {
        ray_t ray;
        memset(&ray, 0, sizeof(ray_t));

        set_refracted_direction(&ray.direction, &hit->incident, &hit->normal, hit->ior);
        normalize(&ray.direction);

        ray.origin = hit->location;
        if (outside) {
            vec_subtract(&ray.origin, &bias);
        } else {
            vec_add(&ray.origin, &bias);
        }
        memset(&tmp_color, 0, sizeof(vec3_t));

        cast_ray(&tmp_color, &ray, bounce, entities, total_entities, lights, quality);
        const_mult(&tmp_color, 1.0 - reflectance);
        vec_add(color, &tmp_color);
    }
    memset(&tmp_color, 0, sizeof(vec3_t));

    ray_t ray;
    ray.direction = hit->incident;
    vec3_t tmp = hit->normal;
    const_mult(&tmp, 2 * dot(&hit->incident, &hit->normal));
    vec_subtract(&ray.direction, &tmp);
    normalize(&ray.direction);

    ray.origin = hit->location;
    if (outside) {
        vec_add(&ray.origin, &bias);
    } else {
        vec_subtract(&ray.origin, &bias);
    }
    cast_ray(&tmp_color, &ray, bounce, entities, total_entities, lights, quality);
    const_mult(&tmp_color, reflectance);
    vec_add(color, &tmp_color);
}

static void apply_reflection(hit_t *hit, vec3_t *color, Bounce_t *bounce, entity_t entities[],
                             int total_entities, point_light_t lights[], int quality) {
    ray_t ray;
    ray.direction = hit->incident;
    vec3_t tmp = hit->normal;
    const_mult(&tmp, 2 * dot(&hit->incident, &hit->normal));
    vec_subtract(&ray.direction, &tmp);
    normalize(&ray.direction);

    ray.origin = hit->normal;
    const_mult(&ray.origin, EPSILON);
    vec_add(&ray.origin, &hit->location);

    bounce->refl += 1;
    cast_ray(color, &ray, bounce, entities, total_entities, lights, quality);
}

static void UniformSampleHemisphere(vec3_t *normal, vec3_t *dir) {
    // Uniform point on sphere
    // from http://mathworld.wolfram.com/SpherePointPicking.html

    double u = rand() / (1.0 + RAND_MAX);
    double v = rand() / (1.0 + RAND_MAX);

    double theta = 2.0 * PI * u;
    double phi = acos(2.0 * v - 1.0);

    double cosTheta = cos(theta);
    double sinTheta = sin(theta);
    double cosPhi = cos(phi);
    double sinPhi = sin(phi);


    dir->x = cosTheta * sinPhi;
    dir->y = sinTheta * sinPhi;
    dir->z = cosPhi;
    normalize(dir);
    // if our vector is facing the wrong way vs the normal, flip it!
    if (dot(dir, normal) <= 0.0)
        const_mult(dir, -1.0);
}

static void apply_diffusion(hit_t *hit, vec3_t *color, Bounce_t *bounce, entity_t entities[],
                            int total_entities, point_light_t lights[], int quality) {
    bounce->diff += 1;
    ray_t ray;
    UniformSampleHemisphere(&hit->normal, &ray.direction);

    ray.origin = hit->normal;
    const_mult(&ray.origin, EPSILON);
    vec_add(&ray.origin, &hit->location);

    hit_t nearest_hit;
    memset(&nearest_hit, 0, sizeof(nearest_hit));

    vec3_t new_color;
    init_vec(&new_color);

    cast_ray(&new_color, &ray, bounce, entities, total_entities, lights, quality);
    vec_mult(&hit->color_hit, &new_color);
    *color = hit->color_hit;
    const_mult(color, (dot(&hit->normal, &ray.direction)) * 2.0);

}

static int bounce_exceeds_threshold(Bounce_t *bounce) {
    return (bounce->diff > MAXBOUNCES || bounce->refl > MAXBOUNCES || bounce->refr > MAXBOUNCES);
}

static void cast_ray(vec3_t *color, ray_t *ray, Bounce_t *bounce, entity_t entities[],
                     int total_entities, point_light_t lights[], int quality) {
    // if this method is called in sequence (i.e. following the original ray passed in from the render() method for more than MAXBOUNCES then return back up the calling chain all the way back to render())
    Bounce_t new_bounce;

    if (bounce == NULL) {
        memset(&new_bounce, 0, sizeof(Bounce_t));
        bounce = &new_bounce;
    } else if (bounce_exceeds_threshold(bounce)) {
        return;
    }
    entity_t *entity;
    hit_t hit;
    int i;
    hit_t this_hit;
    this_hit.ignore = 0;
    double min_distance = INF;
    double distance = 0;
    for (i = 0; i < total_entities; i++) {
        entity = entities + i;
        distance = entity->intersect(entity, ray, &this_hit);
        if (distance > 0 && min_distance > distance) {
            min_distance = distance;
            hit = this_hit;
        }
    }
    if (min_distance == INF || min_distance <= 0.0) return;

    if (quality == STAGE_1) {
        *color = hit.color_hit;
        return;
    }

    if (hit.material == DIFF) {
        switch (quality) {
            case STAGE_3:
                apply_diffusion(&hit, color, bounce, entities, total_entities, lights, quality);
                break;
            case STAGE_2:
                apply_point_light_effect(&hit, color, entities, total_entities, lights);
                break;
        }
    } else if (hit.material == EMMISIVE) {
        *color = hit.color_hit;
    } else if (hit.material == REFL) {
        apply_reflection(&hit, color, bounce, entities, total_entities, lights, quality);
    } else if (hit.material == REFR) {
        apply_refraction(&hit, color, bounce, entities, total_entities, lights, quality);
    }
}

void *render(void *data) {
    int fieldOfView = FIELD_OF_VIEW_DEGREES;
    thread_data_t *t_data = (thread_data_t *) data;
    double aspectRatio = t_data->width / t_data->height;
    double fovLength = 2.0 * tan(fieldOfView / 2 * PI / 180.0);
    double ImagePlaneHeight = fovLength;
    double ImagePlaneWidth = ImagePlaneHeight * aspectRatio;
    entity_t *entities = t_data->entities;
    point_light_t *lights = t_data->point_lights;
    ray_t ray;
    vec3_t rayOrigin;
    init_vec(&rayOrigin);
    vec3_t pixel_color;
    int pixel_no = 0;
    double rays_per_pixel_side = t_data->rays_per_pixel_side;

    double denom = 2.0 * rays_per_pixel_side;
    for (int y = 0; y < t_data->height; y++) {
        for (int x = 0; x < t_data->width; x++) {
            if (pixel_no % t_data->no_of_threads != t_data->thread_no) {
                ++pixel_no;
                continue;
            }
            ++pixel_no;
            init_vec(&pixel_color);
            for (int j = 0; j < rays_per_pixel_side; j++) {
                for (int i = 0; i < rays_per_pixel_side; i++) {
                    double xx = x + (2 * i + 1) / denom;
                    double yy = y + (2 * j + 1) / denom;
                    double normX = xx / t_data->width;
                    double normY = yy / t_data->height;

                    ray.origin = rayOrigin;
                    ray.direction.x = ImagePlaneWidth * (normX - 0.5);
                    ray.direction.y = ImagePlaneHeight * (0.5 - normY);
                    ray.direction.z = 1.0;
                    normalize(&ray.direction);

                    vec3_t ray_color;
                    init_vec(&ray_color);
                    cast_ray(&ray_color, &ray, NULL, entities, t_data->total_entities, lights,
                             t_data->quality);
                    vec_add(&pixel_color, &ray_color);
                }
            }
            const_div(&pixel_color, rays_per_pixel_side * rays_per_pixel_side);
            // clamp pixel color between 0 and 255
            pixel_color.x = pow(pixel_color.x, 1.0 / GAMMA);
            pixel_color.y = pow(pixel_color.y, 1.0 / GAMMA);
            pixel_color.z = pow(pixel_color.z, 1.0 / GAMMA);
            clamp_color_vec(&pixel_color);
            *(t_data->pixels + y * t_data->width + x) = pixel_color;
        }
    }
    return (void *) NULL;
}


