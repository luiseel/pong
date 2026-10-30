#ifndef VECTOR_H
#define VECTOR_H

typedef struct {
    float x;
    float y;
} vector_2d_t;

void add_vectors(vector_2d_t *, vector_2d_t, vector_2d_t);
void substract_vectors(vector_2d_t *, vector_2d_t, vector_2d_t);
void normalize_vector(vector_2d_t *);
float calculate_vector_length(vector_2d_t);
void multiply_vector_by_scalar(vector_2d_t *, float);
void print_vector(vector_2d_t);

#endif
