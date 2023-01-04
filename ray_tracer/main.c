



#include "Scene.h"
#include "jsmn.h"

#include "FileUtils.h"
#include "Render.h"
#include <pthread.h>

#include <string.h>
#include <stdio.h>


#define ROOT 0
#define NO_OF_THREADS 4
#define MAX_FLOAT 1e6
#define WIDTH 400
#define HEIGHT 400



static void init_triangle(entity_t *triangle) {
    triangle->intersect = triangle_intersect;
    // va = v2 - v1
    triangle->v0 = triangle->b;
    vec_subtract(&triangle->v0, &triangle->a);

    // vb = v3 - v1
    triangle->v1 = triangle->c;
    vec_subtract(&triangle->v1, &triangle->a);

    cross(&triangle->v0, &triangle->v1, &triangle->normal);
    normalize(&triangle->normal);

    triangle->center = triangle->a;
    vec_add(&triangle->center, &triangle->b);
    vec_add(&triangle->center, &triangle->c);
    const_div(&triangle->center, 3.0);

    triangle->d00 = dot(&triangle->v0, &triangle->v0);
    triangle->d01 = dot(&triangle->v0, &triangle->v1);
    triangle->d11 = dot(&triangle->v1, &triangle->v1);

    triangle->inv_denom = 1.0 / (triangle->d00 * triangle->d11 - triangle->d01 * triangle->d01);
}

static int jsoneq(char *json, jsmntok_t *tok, const char *s) {
    if (/*tok->type == JSMN_STRING &&*/ (int) strlen(s) == tok->end - tok->start &&
                                        strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

static void convert_token_to_float(char *buf, jsmntok_t *token, double *num) {
    char temp[10] = {0};
    memcpy(temp, buf + token->start, token->end - token->start);
    *num = (double) atof(temp);
}

static void convert_token_to_vec3(char *buf, jsmntok_t *token, vec3_t *vec) {
    char temp[10] = {0};
    char *t = temp;
    char *pointer = buf + token->start + 1;
    while (*pointer != ',') {
        *t++ = *pointer++;
    }
    vec->x = (double) atof(temp);

    memset(temp, 0, 10);
    ++pointer;
    t = temp;
    while (*pointer != ',') {
        *t++ = *pointer++;
    }
    vec->y = (double) atof(temp);

    ++pointer;
    memset(temp, 0, 10);
    t = temp;
    while (*pointer != ']') {
        *t++ = *pointer++;
    }
    vec->z = (double) atof(temp);
}

static void parse_line_to_vec(char *line, vec3_t *vec) {
    line += 1;
    char temp[10] = {0};
    char *pointer = temp;
    while (*line != ' ')*pointer++ = *line++;
    vec->x = (double) atof(temp);

    line += 1;
    memset(temp, 0, 10);
    pointer = temp;
    while (*line != ' ')*pointer++ = *line++;
    vec->y = (double) atof(temp);

    line += 1;
    memset(temp, 0, 10);
    pointer = temp;
    while (*line != 0)*pointer++ = *line++;
    vec->z = -(double) atof(temp);/*RHS to LHS conversion*/
}

static void init_boundingBox(bounding_box_t *box, double x1, double x2, double y1, double y2, double z1,
                      double z2) {

    vec3_t vec = {0};
    vec.x = x1;
    box->points[0] = vec;
    vec.x = x1 > 0 ? -1 : 1;
    box->normals[0] = vec;

    vec.x = x2;
    box->points[1] = vec;
    vec.x = x2 > 0 ? -1 : 1;
    box->normals[1] = vec;

    memset(&vec, 0, sizeof(vec3_t));
    vec.y = y1;
    box->points[2] = vec;
    vec.y = y1 > 0 ? -1 : 1;
    box->normals[2] = vec;

    vec.y = y2;
    box->points[3] = vec;
    vec.y = y2 > 0 ? -1 : 1;
    box->normals[3] = vec;

    memset(&vec, 0, sizeof(vec3_t));
    vec.z = z1;
    box->points[4] = vec;
    vec.z = z1 > 0 ? -1 : 1;
    box->normals[4] = vec;

    vec.z = z2;
    box->points[5] = vec;
    vec.z = z2 > 0 ? -1 : 1;
    box->normals[5] = vec;
}

static void load_obj_file(entity_t *entity) {
    char *fn = entity->file;


    /*first count no of verts*/
    int no_of_verts = 0;
    int no_of_faces = 0;
    FileLines_t *file = getLines(fn);
    int i;
    for (i = 0; i < file->no_of_lines; i++) {
        char *line = file->lines[i];
        //printf("%s\n", line);
        if (strlen(line) < 2) continue;
        if (line[0] == 'v' && line[1] == ' ')no_of_verts += 1;
        if (line[0] == 'f')no_of_faces += 1;
    }

    /*allocate verts memory*/
    vec3_t *vertices = (vec3_t *) (malloc(no_of_verts * sizeof(vec3_t)));

    /*now count faces and allocate triangles*/
    entity->no_of_triangles = no_of_faces;



    // bounding box params
    entity->bounding_box = (bounding_box_t *) malloc(sizeof(bounding_box_t));
    memset(entity->bounding_box, 0, sizeof(bounding_box_t));

    double x_min, y_min, z_min;
    x_min = y_min = z_min = MAX_FLOAT;
    double x_max, y_max, z_max;
    x_max = y_max = z_max = -MAX_FLOAT;

    /*allocate triangles memory*/
    entity->triangles = (entity_t *) (malloc(entity->no_of_triangles * sizeof(entity_t)));
    memset(entity->triangles, 0, entity->no_of_triangles * sizeof(entity_t));
    entity_t *triangle = entity->triangles;
    vec3_t *vertex = vertices;
    for (i = 0; i < file->no_of_lines; i++) {
        char *line = file->lines[i];
        if (strlen(line) < 2) continue;
        if (line[0] == 'v' && line[1] == ' ') {
            parse_line_to_vec(line + 1, vertex);
            vertex += 1;
        }
            // here we have a new triangle face
        else if (line[0] == 'f') {
            int indices[3];
            char *pointer = line + 1;
            while (*pointer != 0) {
                if (*pointer == '/') *pointer = 0;
                ++pointer;
            }
            pointer = line + 1;
            //printf("%s\n", pointer);
            int j;
            for (j = 0; j < 3; j++) {
                indices[j] = atoi(pointer);
                pointer += strlen(pointer);
                while (j < 2 && *pointer != ' ')++pointer;
            }


            //printf("%d: %d : %d\n", indices[0], indices[1], indices[2]);
            triangle->c = vertices[indices[0] - 1];
            const_mult(&(triangle->c), entity->scale);
            vec_add(&(triangle->c), &(entity->center));

            triangle->b = vertices[indices[1] - 1];
            const_mult(&(triangle->b), entity->scale);
            vec_add(&(triangle->b), &(entity->center));

            triangle->a = vertices[indices[2] - 1];
            const_mult(&(triangle->a), entity->scale);
            vec_add(&(triangle->a), &(entity->center));

            /*updating min and max aal bounding plane coordinates*/
            vec3_t *vertices[3];
            vertices[0] = &(triangle->a);
            vertices[1] = &(triangle->b);
            vertices[2] = &(triangle->c);

            for (j = 0; j < 3; j++) {
                if (x_min > vertices[j]->x) x_min = vertices[j]->x;
                if (x_max < vertices[j]->x) x_max = vertices[j]->x;

                if (y_min > vertices[j]->y) y_min = vertices[j]->y;
                if (y_max < vertices[j]->y) y_max = vertices[j]->y;

                if (z_min > vertices[j]->z) z_min = vertices[j]->z;
                if (z_max < vertices[j]->z) z_max = vertices[j]->z;
            }

            triangle->e_type = TRIANGLE;
            init_triangle(triangle);
            triangle->color = entity->color;
            triangle->material = entity->material;
            triangle->refr_index = entity->refr_index;
            ++triangle;
        }
    }
    init_boundingBox(entity->bounding_box, x_min, x_max, y_min, y_max, z_min, z_max);
    free(vertices);
    for (i = 0; i < file->no_of_lines; i++)free(file->lines[i]);
    free(file->lines);
    free(file);
}

/*parse into entity. fields can be any order*/
static void
parse_json_obj(char *buf, jsmntok_t *token, entity_t *entity, int *token_no, int total_tokens) {
    /*move forward one token past the object token passed in*/
    token += 1;
    *token_no += 1;
    while (*token_no < total_tokens && token->type != JSMN_OBJECT) {
        if (jsoneq(buf, token, "Type") == 0) {
            if (jsoneq(buf, token + 1, "Plane") == 0) {
                entity->e_type = PLANE;
            } else if (jsoneq(buf, token + 1, "Triangle") == 0) {
                entity->e_type = TRIANGLE;
            } else if (jsoneq(buf, token + 1, "Sphere") == 0) {
                entity->e_type = SPHERE;
            } else if (jsoneq(buf, token + 1, "Object") == 0) {
                entity->e_type = OBJ;
            }
        } else if (jsoneq(buf, token, "Material") == 0) {
            if (jsoneq(buf, token + 1, "Diffuse") == 0) {
                entity->material = DIFF;
            } else if (jsoneq(buf, token + 1, "Reflective") == 0) {
                entity->material = REFL;
            } else if (jsoneq(buf, token + 1, "Refractive") == 0) {
                entity->material = REFR;
            } else if (jsoneq(buf, token + 1, "Emissive") == 0) {
                entity->material = EMMISIVE;
            }
        } else if (jsoneq(buf, token, "Point") == 0) {
            convert_token_to_vec3(buf, token + 1, &entity->center);
        } else if (jsoneq(buf, token, "Normal") == 0) {
            convert_token_to_vec3(buf, token + 1, &entity->normal);
        } else if (jsoneq(buf, token, "Color") == 0) {
            convert_token_to_vec3(buf, token + 1, &entity->color);
        } else if (jsoneq(buf, token, "VertexA") == 0) {
            convert_token_to_vec3(buf, token + 1, &entity->a);
        } else if (jsoneq(buf, token, "VertexB") == 0) {
            convert_token_to_vec3(buf, token + 1, &entity->b);
        } else if (jsoneq(buf, token, "VertexC") == 0) {
            convert_token_to_vec3(buf, token + 1, &entity->c);
        } else if (jsoneq(buf, token, "Radius") == 0) {
            convert_token_to_float(buf, token + 1, &(entity->radius));
        } else if (jsoneq(buf, token, "Scale") == 0) {
            convert_token_to_float(buf, token + 1, &(entity->scale));
        } else if (jsoneq(buf, token, "RefractiveIndex") == 0) {
            convert_token_to_float(buf, token + 1, &(entity->refr_index));
        } else if (jsoneq(buf, token, "File") == 0) {
            entity->file = (char *) malloc((token + 1)->end - (token + 1)->start + 1);
            memset(entity->file, 0, (token + 1)->end - (token + 1)->start + 1);
            memcpy(entity->file, buf + (token + 1)->start, (token + 1)->end - (token + 1)->start);
        }

        *token_no += 1;
        token += 1;
    }

}

static void parse_settings_file(entity_t **entities, int *total_entities, char *fn, char **settings_buf) {

    jsmn_parser parser;
    FILE *fp;
    long lSize;
    fp = fopen(fn, "rb");
    if (!fp) perror(fn), exit(1);
    fseek(fp, 0L, SEEK_END);
    lSize = ftell(fp);
    rewind(fp);
    *settings_buf = calloc(1, lSize + 1);
    if (!*settings_buf) fclose(fp), fputs("memory alloc fails", stderr), exit(1);
    if (1 != fread(*settings_buf, lSize, 1, fp))
        fclose(fp), free(*settings_buf), fputs("entire read fails", stderr), exit(1);
    fclose(fp);
    jsmn_init(&parser);
    // find number of tokens required
    unsigned int no_of_tokens = jsmn_parse(&parser, *settings_buf, strlen(*settings_buf), NULL,
                                           0);
    jsmntok_t *tokens = (jsmntok_t *) calloc(no_of_tokens, sizeof(jsmntok_t));
    jsmn_init(&parser);
    int no_of_tokens_parsed = jsmn_parse(&parser, (const char *) *settings_buf,
                                         strlen(*settings_buf), tokens, no_of_tokens);
    if (no_of_tokens_parsed < 0) {
        printf("Failed to parse JSON: %d\n", no_of_tokens_parsed);
        return;
    }

    /*allocate memory for entities*/
    *total_entities = tokens->size;
    *entities = (entity_t *) malloc(sizeof(entity_t) * (*total_entities));
    memset(*entities, 0, sizeof(entity_t) * (*total_entities));
    int token_no = 1;
    jsmntok_t *token;
    entity_t *entity = *entities;;
    while (token_no < no_of_tokens_parsed) {
        token = tokens + token_no;
        //print_token(settingsBuffer, token);
        if (token->parent == ROOT) {
            parse_json_obj(*settings_buf, token, entity, &token_no, no_of_tokens_parsed);
            entity += 1;
        }
    }

    entity = *entities;
    int i;
    for (i = 0; i < *total_entities; i++) {
        if (entity->e_type == TRIANGLE)init_triangle(entity);
        else if (entity->e_type == PLANE)entity->intersect = plane_intersect;
        else if (entity->e_type == SPHERE)entity->intersect = sphere_intersect;
        else if (entity->e_type == OBJ) {
            entity->intersect = object_intersect;
            load_obj_file(entity);
        }
        entity += 1;
    }
    free(tokens);

}

int main(int argc, char *argv[]) {
    int i;
    entity_t *entities;
    int total_entities;
    char *fn_in;
    char *fn_out;
    int quality;
    int no_of_threads = NO_OF_THREADS;

    point_light_t point_lights[TOTAL_LIGHTS];
    int rays_per_pixels, width = WIDTH, height = HEIGHT;

    while (argc > 1 && argv[1][0] == '-') {
        switch (argv[1][1]) {
            case 'x':
                rays_per_pixels = atoi(&argv[1][2]);
                break;

            case 'w':
                width = atoi(&argv[1][2]);
                break;

            case 'h':
                height = atoi(&argv[1][2]);
                break;

            case 'i':
                fn_in = &argv[1][2];
                break;

            case 'o':
                fn_out = &argv[1][2];
                break;

            case 'q':
                quality = atoi(&argv[1][2]);
                break;

            case 't':
                no_of_threads = atoi(&argv[1][2]);
                break;

        }
        ++argv;
        --argc;
    }

    printf("rays_per_pixel: %d\n", rays_per_pixels * rays_per_pixels);
    printf("width         : %d\n", width);
    printf("height        : %d\n", height);

    char *settings_buf = NULL;
    parse_settings_file(&entities, &total_entities, fn_in, &settings_buf);
    vec3_t *pixels = (vec3_t *) malloc(
            (long unsigned int) width * (long unsigned int) height * sizeof(vec3_t));

    init();
    setup_point_lights(point_lights);

    /*divide up work and feed into threads*/

    thread_data_t thread_data[no_of_threads];

    for (i = 0; i < no_of_threads; i++) {
        thread_data[i].thread_no = i;
        thread_data[i].entities = entities;
        thread_data[i].total_entities = total_entities;
        thread_data[i].point_lights = point_lights;
        thread_data[i].rays_per_pixel_side = rays_per_pixels;
        thread_data[i].pixels = pixels;
        thread_data[i].width = width;
        thread_data[i].height = height;
        thread_data[i].quality = quality;
        thread_data[i].no_of_threads = no_of_threads;
    }

    pthread_t threads[no_of_threads];
    for (i = 0; i < no_of_threads; i++) {
        pthread_create(&(threads[i]), NULL,
                       &render, (void *) (&thread_data[i]));
    }
    /*wait for threads to join up and write output*/
    for (i = 0; i < no_of_threads; i++) {
        void *k;
        pthread_join(threads[i], &k);
    }

    FILE *f = fopen(fn_out, "w");         // Write image to PPM file.
    fprintf(f, "P3\n%d %d\n%d\n", width, height, 255);
    int r, g, b;
    for (int i = 0; i < width * height; i++) {
        r = (int) (pixels[i].x * 255);
        g = (int) (pixels[i].y * 255);
        b = (int) (pixels[i].z * 255);
        fprintf(f, "%d %d %d ", r, g, b);
    }
    fclose(f);
    entity_t *entity = entities;
    for (i = 0; i < total_entities; i++) {
        if (entity->triangles) free(entity->triangles);
        if (entity->file) free(entity->file);
        if (entity->bounding_box) free(entity->bounding_box);
        ++entity;
    }
    free(entities);
    free(pixels);
    free(settings_buf);
    return 0;
}
