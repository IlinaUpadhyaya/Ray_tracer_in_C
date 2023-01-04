#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>


typedef struct Vec3 {
	double x;
	double y;
	double z;
	}vec3_t;

void init_vec(vec3_t* vec);

double vec_length(vec3_t* v1);

void set_values(vec3_t *vec, double x, double y, double z);

void normalize(vec3_t *v1) ;

void cross(vec3_t *v1, vec3_t *v2, vec3_t *rv) ;

double dot(vec3_t *v1, vec3_t *v2) ;

void const_div(vec3_t *v1, double k) ;

void const_mult(vec3_t *v1, double k) ;

void vec_mult(vec3_t *v1, vec3_t *v2) ;

void vec_add(vec3_t *v1, vec3_t *v2) ;

void vec_subtract(vec3_t *v1, vec3_t *v2) ;

#endif




