#include <SDL.h>
#include <math.h>
#include "vector.h"

void add_vectors(vector_2d_t *dest, vector_2d_t left, vector_2d_t right) {
    dest->x = left.x + right.x;
    dest->y = left.y + right.y;
}

void substract_vectors(vector_2d_t *dest, vector_2d_t left, vector_2d_t right) {
    dest->x = left.x - right.x;
    dest->y = left.y - right.y;
}

void normalize_vector(vector_2d_t *vector) {
    const float length = calculate_vector_length(*vector);
    if (length < 0.f) {
        return;
    }
    vector->x = vector->x / length;
    vector->y = vector->y / length;
}

float calculate_vector_length(vector_2d_t vector) {
    return sqrt(vector.x * vector.x + vector.y * vector.y);
}

void multiply_vector_by_scalar(vector_2d_t *vector, float scalar) {
    vector->x *= scalar;
    vector->y *= scalar;
}

void print_vector(vector_2d_t vector) {
    SDL_Log("vector_2d_t { x: %f, y: %f, length: %f }", vector.x, vector.y, calculate_vector_length(vector));
}
