#include "Vector.h"
#include <math.h>



void init_vec(vec3_t *vec) {
	vec->x = vec->y = vec->z = 0.0;
	}

double vec_length(vec3_t *v1) {
	return sqrt(v1->x * v1->x + v1->y * v1->y + v1->z * v1->z);
	}

void set_values(vec3_t *vec, double x, double y, double z) {
	vec->x = x;
	vec->y = y;
	vec->z = z;
	}

void normalize(vec3_t *v1) {
	double length = sqrt(v1->x * v1->x + v1->y * v1->y + v1->z * v1->z);
	const_div(v1, length);
	}

void cross(vec3_t *v1, vec3_t *v2, vec3_t *rv) {
	rv->x = v1->y * v2->z - v1->z * v2->y;
	rv->y = v1->z * v2->x - v1->x * v2->z;
	rv->z = v1->x * v2->y - v1->y * v2->x;
	}

double dot(vec3_t *v1, vec3_t *v2) {
	return v1->x * v2->x + v1->y * v2->y + v1->z * v2->z;
	}

void const_div(vec3_t *v1, double k) {
	v1->x /= k;
	v1->y /= k;
	v1->z /= k;
	}

void const_mult(vec3_t *v1, double k) {
	v1->x *= k;
	v1->y *= k;
	v1->z *= k;
	}

void vec_mult(vec3_t *v1, vec3_t *v2) {
	v1->x *= v2->x;
	v1->y *= v2->y;
	v1->z *= v2->z;
	}

void vec_add(vec3_t *v1, vec3_t *v2) {
	v1->x += v2->x;
	v1->y += v2->y;
	v1->z += v2->z;
	}

void vec_subtract(vec3_t *v1, vec3_t *v2) {
	v1->x -= v2->x;
	v1->y -= v2->y;
	v1->z -= v2->z;
	}


