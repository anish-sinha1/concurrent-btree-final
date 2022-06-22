#include "slice.h"

struct slice_key_index *slice_key_index(void *key, u64 index) {
    struct slice_key_index *ski = malloc(sizeof(struct slice_key_index));
    ski->key   = key;
    ski->index = index;
    return ski;
}

u64 ski_index(struct slice_key_index *ski) {
    return ski->index;
}

void *ski_key(struct slice_key_index *ski) {
    return ski->key;
}

static void slice_resize(struct slice *s, u64 capacity) {
    if (capacity == s->length || capacity < 10) return;
    s->keys     = realloc(s->keys, sizeof(void *) * capacity);
    s->capacity = capacity;
}

static void slice_autoresize(struct slice *s) {
    if (s->length < (u64) floor((double) s->capacity / 4)) slice_resize(s, s->capacity / 2);
    if (s->capacity < 10 && s->length == s->capacity) slice_resize(s, s->capacity * 2 + 1);
    else slice_resize(s, ceil((double) s->capacity * 1.5));
}


struct slice *slice() {
    struct slice *s = malloc(sizeof(struct slice));
    s->length   = 0;
    s->capacity = INITIAL_SLICE_CAPACITY;
    s->keys     = malloc(sizeof(void *) * s->capacity);
    return s;
}

void delete_slice(struct slice *s) {
    s->keys = NULL;
    free(s->keys);
    s->length   = 0;
    s->capacity = 0;
    s = NULL;
    free(s);
}

struct slice *make_slice(void *keys, u64 capacity, size_t size) {
    struct slice *s = slice();
    for (int     i  = 0; i < capacity; i++) s->keys[i] = keys + size * i;
    s->length = capacity;
    return s;
}

struct slice *subslice(struct slice *s, u64 start, u64 end) {
    struct slice *ss = slice();
    slice_resize(ss, end - start);
    memcpy(ss->keys, &(s->keys[start]), sizeof(void *) * (end - start));
    ss->length = end - start;
    return ss;
}

void slice_insert_front(struct slice *s, void *key) {
    slice_autoresize(s);
    memcpy(&(s->keys[1]), s->keys, sizeof(void *) * s->length);
    s->length++;
    s->keys[0] = key;
}

void slice_insert_back(struct slice *s, void *key) {
    slice_autoresize(s);
    s->keys[s->length++] = key;
}

void *slice_delete_front(struct slice *s) {
    void *key = s->keys[0];
    s->keys[0] = NULL;
    free(s->keys[0]);
    memcpy(s->keys, &(s->keys[1]), sizeof(void *) * (s->length - 1));
    s->length--;
    slice_autoresize(s);
    return key;
}

void *slice_delete_back(struct slice *s) {
    void *key = s->keys[s->length - 1];
    s->keys[s->length - 1] = NULL;
    free(s->keys[s->length - 1]);
    s->length--;
    slice_autoresize(s);
    return key;
}

void slice_put_index(struct slice *s, void *key, u64 index) {
    if (index > s->length) return;
    if (index == s->length) return slice_insert_back(s, key);
    if (index == 0) return slice_insert_front(s, key);
    slice_autoresize(s);
    memcpy(&(s->keys[index + 1]), &(s->keys[index]), sizeof(void *) * (s->length - index));
    s->keys[index] = key;
    s->length++;
}

void *slice_remove_index(struct slice *s, u64 index) {
    if (index >= s->length) return NULL;
    void *key = s->keys[index];
    s->keys[index] = NULL;
    free(s->keys[index]);
    memcpy(&(s->keys[index]), &(s->keys[index + 1]), sizeof(void *) * (s->length - index - 1));
    s->length--;
    return key;
}

void slice_set_index(struct slice *s, void *key, u64 index) {
    if (index >= s->length) return;
    s->keys[index] = key;
}

void *slice_get_index(struct slice *s, u64 index) {
    if (index >= s->length) return NULL;
    return s->keys[index];
}

void slice_fill(struct slice *s, void **keys, u64 num_keys) {
    memcpy(s->keys, keys, sizeof(void *) * num_keys);
    s->length = num_keys;
}

void slice_join(struct slice *s1, struct slice *s2) {
    if (s1->length + s2->length >= s1->capacity) slice_resize(s1, s1->length + s2->length);
    memcpy(&(s1->keys[s1->length]), s2->keys, sizeof(void *) * s2->length);
    s1->length += s2->length;
    delete_slice(s2);
}


struct slice_key_index *slice_find_index(const struct slice *s, const void *key) {
    u64 start = 0, end = s->length - 1;
    while (start <= end) {
        u64 mid = (start + end) / 2;
        if (s->compare(s->keys[mid], key) == 0) return slice_key_index(s->keys[mid], mid);
        if (s->compare(s->keys[mid], key) < 0) start = mid + 1;
        else end = mid - 1;
    }
    return slice_key_index(NULL, end + 1);
}

void slice_print(struct slice *s) {
    for (int i = 0; i < s->length; i++) s->print(s->keys[i]);
    printf("\n");
}

static void insertion_sort(struct slice *s) {
    int      j;
    for (int i = 0; i < s->length; i++) {
        void *key = s->keys[i];
        j              = i - 1;
        while (j >= 0 && s->compare(key, s->keys[j]) <= 0) {
            s->keys[j + 1] = s->keys[j];
            j--;
        }
        s->keys[j + 1] = key;
    }
}

static void merge(struct slice *s, struct slice *l, struct slice *r) {
    int i = 0, j = 0, k = 0;
    while (i < l->length && j < r->length) {
        if (s->compare(r->keys[j], l->keys[i]) >= 0) {
            slice_set_index(s, l->keys[i], k);
            i++;
        } else {
            slice_set_index(s, r->keys[j], k);
            j++;
        }
        k++;
    }
    while (i < l->length) {
        slice_set_index(s, l->keys[i], k);
        i++, k++;
    }
    while (j < r->length) {
        slice_set_index(s, r->keys[j], k);
        j++, k++;
    }
}

void csort(struct slice *s, int(*cmpfunc)(const void *, const void *)) {
    if (s->length < 44) return insertion_sort(s);
    unsigned int mid = s->length / 2;
    struct slice *l  = slice(mid);
    struct slice *r  = slice(s->length - mid);
    slice_fill(l, s->keys, mid);
    slice_fill(r, &(s->keys[mid]), s->length - mid);
    csort(l, cmpfunc);
    csort(r, cmpfunc);
    merge(s, l, r);
}