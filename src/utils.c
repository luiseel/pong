#include "utils.h"

char * itos(int n) {
    char *str = malloc(sizeof(char) * 12);
    sprintf(str, "%d", n);
    return str;
}

char * ftos(float n) {
    char *str = malloc(sizeof(char) * 12); // check this number, I'm not really sure...
    sprintf(str, "%f", n);
    return str;
}
