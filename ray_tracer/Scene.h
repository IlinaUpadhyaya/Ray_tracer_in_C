#ifndef SCENE_H
#define SCENE_H



#include "Vector.h"
#define TOTAL_LIGHTS 2
typedef enum Material_t material_t;
typedef struct entity entity_t;

enum Material_t { DIFF, REFL, REFR, EMMISIVE }; // material types
enum type_of_entity { SPHERE, TRIANGLE, PLANE, OBJ };

typedef struct Ray {
	vec3_t origin;
	vec3_t direction;
	}ray_t;

typedef struct Hit {
	double t;
	int ignore;
	int hit_entity;
	vec3_t incident;
	vec3_t location;
	vec3_t color_hit;
	vec3_t normal;
	material_t material;
	double ior;
	}hit_t;

typedef struct PointLight {
	vec3_t location;
	vec3_t color;
	}point_light_t;



double sphere_intersect(entity_t *object, ray_t *ray, hit_t *hit);
double triangle_intersect(entity_t *object, ray_t *ray, hit_t *hit);
double plane_intersect(entity_t *object, ray_t *ray, hit_t *hit);
double object_intersect(entity_t *object, ray_t *ray, hit_t *hit);

typedef struct Bounding_Box {
	vec3_t points[6];
	vec3_t normals[6];
	}bounding_box_t;

typedef struct entity {
	enum type_of_entity e_type;

	vec3_t color;

	/*sphere*/
	double radius;
	vec3_t center; /*plane*/
	material_t material;

	/*triangle*/
	vec3_t a, b, c;
	vec3_t v0, v1;
	double d00, d01, d11;
	double inv_denom;

	vec3_t normal; /*plane*/ /*triangle*/
	char *file; /*obj file*/
	double scale;/*obj file*/
	double refr_index;

	double (*intersect)(entity_t *, ray_t *, hit_t *);

	int index;
	entity_t *triangles;
	int no_of_triangles;
	bounding_box_t *bounding_box;
	} entity_t;
#endif