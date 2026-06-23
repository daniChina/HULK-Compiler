#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int tag;
    char data[8];
} BoxedValue;

enum { HULK_TAG_BOOL = 0, HULK_TAG_NUMBER = 1, HULK_TAG_STRING = 2 };

typedef struct {
    double start;
    double end;
} HulkRange;

void* hulk_range_create(double start, double end) {
    HulkRange* range = (HulkRange*)malloc(sizeof(HulkRange));
    if (range != NULL) {
        range->start = start;
        range->end = end;
    }
    return range;
}

void hulk_runtime_init(void) {}

void hulk_runtime_error_at(int line, int col, const char* msg) {
    if (line > 0 && col > 0) {
        fprintf(stderr, "(%d,%d) RUNTIME: %s\n", line, col, msg);
    } else {
        fprintf(stderr, "RUNTIME: %s\n", msg);
    }
    exit(1);
}

void hulk_runtime_cast_error(void) {
    hulk_runtime_error_at(0, 0, "invalid cast");
}

void hulk_runtime_case_error(void) {
    hulk_runtime_error_at(0, 0, "case sin rama coincidente");
}

static char* boxed_string_ptr(BoxedValue* boxed) {
    char** slot = (char**)(boxed->data);
    return *slot;
}

void hulk_print_double(double value) {
    printf("%g\n", value);
}

void hulk_print_bool(int value) {
    printf("%s\n", value ? "true" : "false");
}

void hulk_print_null(void) {
    printf("Null\n");
}

void hulk_print_newline(void) {
    putchar('\n');
}

void hulk_print_instance(void* instance) {
    (void)instance;
    printf("<instance>\n");
}

void* hulk_box_number(double value) {
    BoxedValue* boxed = (BoxedValue*)malloc(sizeof(BoxedValue));
    if (boxed != NULL) {
        boxed->tag = HULK_TAG_NUMBER;
        double* slot = (double*)(boxed->data);
        *slot = value;
    }
    return boxed;
}

double hulk_unbox_number(void* boxed_ptr) {
    if (boxed_ptr == NULL) {
        fprintf(stderr, "Runtime error: unbox number from null\n");
        exit(1);
    }
    BoxedValue* boxed = (BoxedValue*)boxed_ptr;
    if (boxed->tag != HULK_TAG_NUMBER) {
        fprintf(stderr, "Runtime error: invalid unbox number\n");
        exit(1);
    }
    double* slot = (double*)(boxed->data);
    return *slot;
}

void* hulk_box_bool(int value) {
    BoxedValue* boxed = (BoxedValue*)malloc(sizeof(BoxedValue));
    if (boxed != NULL) {
        boxed->tag = HULK_TAG_BOOL;
        int* slot = (int*)(boxed->data);
        *slot = value;
    }
    return boxed;
}

void hulk_print_boxed(BoxedValue* boxed) {
    if (boxed == NULL) {
        hulk_print_null();
        return;
    }
    if (boxed->tag == HULK_TAG_STRING) {
        char* text = boxed_string_ptr(boxed);
        if (text == NULL) {
            putchar('\n');
        } else {
            printf("%s\n", text);
        }
        return;
    }
    if (boxed->tag == HULK_TAG_NUMBER) {
        double* slot = (double*)(boxed->data);
        hulk_print_double(*slot);
        return;
    }
    if (boxed->tag == HULK_TAG_BOOL) {
        int* slot = (int*)(boxed->data);
        hulk_print_bool(*slot);
        return;
    }
    putchar('\n');
}

static char* hulk_strdup(const char* src) {
    if (src == NULL) {
        return NULL;
    }
    size_t len = strlen(src);
    char* copy = (char*)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, src, len + 1);
    }
    return copy;
}

BoxedValue* hulk_string_concat(BoxedValue* left, BoxedValue* right) {
    const char* a = (left != NULL && left->tag == HULK_TAG_STRING) ? boxed_string_ptr(left) : "";
    const char* b = (right != NULL && right->tag == HULK_TAG_STRING) ? boxed_string_ptr(right) : "";
    size_t len = strlen(a) + strlen(b);
    char* merged = (char*)malloc(len + 1);
    if (merged == NULL) {
        return NULL;
    }
    memcpy(merged, a, strlen(a));
    memcpy(merged + strlen(a), b, strlen(b) + 1);

    BoxedValue* boxed = (BoxedValue*)malloc(sizeof(BoxedValue));
    if (boxed == NULL) {
        free(merged);
        return NULL;
    }
    boxed->tag = HULK_TAG_STRING;
    char** slot = (char**)(boxed->data);
    *slot = merged;
    return boxed;
}

int hulk_string_equals(BoxedValue* left, BoxedValue* right) {
    if (left == NULL || right == NULL) {
        return 0;
    }
    if (left->tag != HULK_TAG_STRING || right->tag != HULK_TAG_STRING) {
        return 0;
    }
    const char* a = boxed_string_ptr(left);
    const char* b = boxed_string_ptr(right);
    if (a == NULL || b == NULL) {
        return a == b;
    }
    return strcmp(a, b) == 0;
}

int hulk_boxed_equals(BoxedValue* left, BoxedValue* right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }
    if (left->tag != right->tag) {
        return 0;
    }
    if (left->tag == HULK_TAG_NUMBER) {
        double* a = (double*)(left->data);
        double* b = (double*)(right->data);
        return *a == *b;
    }
    if (left->tag == HULK_TAG_STRING) {
        return hulk_string_equals(left, right);
    }
    if (left->tag == HULK_TAG_BOOL) {
        int* a = (int*)(left->data);
        int* b = (int*)(right->data);
        return *a == *b;
    }
    return 0;
}

BoxedValue* hulk_string_concat_ws(BoxedValue* left, BoxedValue* right) {
    const char* a = (left != NULL && left->tag == HULK_TAG_STRING) ? boxed_string_ptr(left) : "";
    const char* b = (right != NULL && right->tag == HULK_TAG_STRING) ? boxed_string_ptr(right) : "";
    size_t len = strlen(a) + 1 + strlen(b);
    char* merged = (char*)malloc(len + 1);
    if (merged == NULL) {
        return NULL;
    }
    if (strlen(a) == 0) {
        memcpy(merged, b, strlen(b) + 1);
    } else if (strlen(b) == 0) {
        memcpy(merged, a, strlen(a) + 1);
    } else {
        memcpy(merged, a, strlen(a));
        merged[strlen(a)] = ' ';
        memcpy(merged + strlen(a) + 1, b, strlen(b) + 1);
    }

    BoxedValue* boxed = (BoxedValue*)malloc(sizeof(BoxedValue));
    if (boxed == NULL) {
        free(merged);
        return NULL;
    }
    boxed->tag = HULK_TAG_STRING;
    char** slot = (char**)(boxed->data);
    *slot = merged;
    return boxed;
}
