#include "Scene.h"
#include <math.h>

#define EPSILON 1.0e-9
#define INF 1.0e6

double sphere_intersect(entity_t *sphere, ray_t *ray, hit_t *hit) {

    vec3_t origin_minus_center = ray->origin;
    vec_subtract(&origin_minus_center, &sphere->center);
    double b = 2.0 * dot(&ray->direction, &origin_minus_center);
    double x = vec_length(&origin_minus_center);
    double c = x * x - sphere->radius * sphere->radius;
    double delta = b * b - 4 * c;
    double t = 0;
    if (delta > 0.0) {
        double sqrt_delta = sqrt(delta);
        double t1 = (-b + sqrt_delta) / 2;
        double t2 = (-b - sqrt_delta) / 2;
        if (t1 * t2 <= 0.0) t = (t2 > t1 ? t2 : t1);
        else t = (t1 < t2 ? t1 : t2);
    }
    if (t <= 0.0) return 0.0;

    if (hit->ignore) return t;

    hit->t = t;
    hit->hit_entity = sphere->index;
    hit->color_hit = sphere->color;
    hit->material = sphere->material;
    hit->ior = sphere->refr_index;
    hit->incident = ray->direction;

    hit->location = hit->incident;
    const_mult(&hit->location, t);
    vec_add(&hit->location, &ray->origin);

    hit->normal = hit->location;
    vec_subtract(&hit->normal, &sphere->center);
    normalize(&hit->normal);

    return t;
}

double triangle_intersect(entity_t *triangle, ray_t *ray, hit_t *hit) {
    double dot_prod = dot(&triangle->normal, &ray->direction);
    if (dot_prod == 0.0) return 0.0;

    vec3_t center_minus_origin = triangle->center;
    vec_subtract(&center_minus_origin, &ray->origin);
    double t = dot(&center_minus_origin, &triangle->normal);
    t /= dot(&ray->direction, &triangle->normal);
    if (t <= 0) return 0.0;

    vec3_t v2 = ray->direction;
    const_mult(&v2, t);
    vec_add(&v2, &ray->origin);

    vec_subtract(&v2, &triangle->a);
    double d20 = dot(&v2, &triangle->v0);
    double d21 = dot(&v2, &triangle->v1);

    double v = (triangle->d11 * d20 - triangle->d01 * d21);
    v *= triangle->inv_denom;
    if (v < 0) return 0.0;

    double w = (triangle->d00 * d21 - triangle->d01 * d20);
    w *= triangle->inv_denom;
    if (w < 0) return 0.0;

    double u = 1.0 - w - v;
    if (u < 0) return 0.0;

    if (hit->ignore) return t; // only interested in t (not in hit details)

    hit->t = t;
    hit->hit_entity = triangle->index;
    hit->color_hit = triangle->color;
    hit->incident = ray->direction;
    hit->normal = triangle->normal;
    hit->material = triangle->material;
    hit->ior = triangle->refr_index;
    hit->location = ray->direction;
    const_mult(&hit->location, t);
    vec_add(&hit->location, &ray->origin);
    return t;
}

double plane_intersect(entity_t *plane, ray_t *ray, hit_t *hit) {
    double dot_prod = dot(&plane->normal, &ray->direction);
    if (dot_prod == 0.0) return 0.0;

    vec3_t center_minus_origin = plane->center;
    vec_subtract(&center_minus_origin, &ray->origin);
    double t = dot(&center_minus_origin, &plane->normal);
    t /= dot(&ray->direction, &plane->normal);
    if (t <= 0) return 0.0;

    if (hit->ignore) return t;

    hit->t = t;
    hit->hit_entity = plane->index;
    hit->incident = ray->direction;
    hit->color_hit = plane->color;
    hit->normal = plane->normal;
    hit->material = plane->material;
    hit->location = ray->direction;
    const_mult(&hit->location, t);
    vec_add(&hit->location, &ray->origin);
    return t;
}

static int intersect(double a1, double a2, double b1, double b2) {
    double maxA = a1 > a2 ? a1 : a2;
    double minB = b1 < b2 ? b1 : b2;
    if (maxA < minB) return 0;

    double minA = a1 < a2 ? a1 : a2;
    double maxB = b1 > b2 ? b1 : b2;
    if (maxB < minA) return 0;

    return 1;
}

static double getIntersect(vec3_t *center, vec3_t *normal, ray_t *ray) {
    vec3_t temp = *center;
    vec_subtract(&temp, &(ray->origin));
    return (dot(&temp, normal) / dot(&(ray->direction), normal));
}

/*first check if it intersects bounding box.
then check each triangle and report back nearest hit
distance*/
double object_intersect(entity_t *object, ray_t *ray, hit_t *hit) {
    int i;
    double dot_prod;
    /*first check bounding box intersection*/
    for (i = 0; i < 6; i++) {
        dot_prod = dot(&(object->bounding_box->normals[i]), &(ray->direction));
        if (dot_prod == 0.0) return 0.0;
    }
    double tX1 = getIntersect(&(object->bounding_box->points[0]),
                              &(object->bounding_box->normals[0]), ray);
    double tX2 = getIntersect(&(object->bounding_box->points[1]),
                              &(object->bounding_box->normals[1]), ray);
    double tY1 = getIntersect(&(object->bounding_box->points[2]),
                              &(object->bounding_box->normals[2]), ray);
    double tY2 = getIntersect(&(object->bounding_box->points[3]),
                              &(object->bounding_box->normals[3]), ray);
    if (intersect(tX1, tX2, tY1, tY2) == 0) return 0.0;
    double tZ1 = getIntersect(&(object->bounding_box->points[4]),
                              &(object->bounding_box->normals[4]), ray);
    double tZ2 = getIntersect(&(object->bounding_box->points[5]),
                              &(object->bounding_box->normals[5]), ray);
    if (intersect(tY1, tY2, tZ1, tZ2) == 0) return 0.0;
    if (intersect(tX1, tX2, tZ1, tZ2) == 0) return 0.0;

    entity_t *triangle;
    double distance = INF;
    hit_t this_hit;
    this_hit.ignore = 0;
    for (i = 0; i < object->no_of_triangles; i++) {
        triangle = object->triangles + i;
        double t = triangle_intersect(triangle, ray, &this_hit);
        if (t > 0.0 && distance > t) {
            *hit = this_hit;
            distance = t;
        }
    }
    if (distance < INF) {
        return distance;
    } else return 0.0;
}