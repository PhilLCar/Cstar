#include <array.h>

Array *newArray(size_t element_size)
{
  Array *array = malloc(sizeof(Array));
  void  *content = malloc(element_size);
  if (array != NULL && content != NULL) {
    array->content      = content;
    array->element_size = element_size;
    array->size         = 0;
    array->capacity     = 1;
  } else {
    if (array   == NULL) free(array);
    if (content == NULL) free(content);
    array = NULL;
  }
  return array;
}

void deleteArray(Array **array)
{
  if (array != NULL) {
    void *content = (*array)->content;
    if (content != NULL) {
      free(content);
    }
    free(*array);
    *array = NULL;
  }
}

int resize(Array *array, int new_size)
{
  int success = 0;
  if (array->size <= new_size) {
    void *tmp = realloc(array->content, new_size * array->element_size);
    if (tmp != NULL) {
      array->content  = tmp;
      array->capacity = new_size;
      success = 1;
    }
  }
  return success;
}

void push(Array *array, void *data)
{
  if (array->size == array->capacity) {
    if (!resize(array, array->capacity * 2)) return;
  }
  memcpy((char*)array->content + (array->element_size * array->size++),
	       (char*)data,
	       array->element_size);
}

void *pop(Array *array)
{
  void *index = NULL;
  if (array->size > 0) {
    int size     = array->size--;
    int capacity = array->capacity;
    index = (char*)array->content + (array->size * array->element_size);
    if (size > 1 && size < capacity / 4) {
      resize(array, capacity / 2);
    }
  }
  return index;
}

void *at(Array* array, int index) {
  if (index >= 0 && index < array->size) {
    return (char*)array->content + (index * array->element_size);
  }
  return NULL;
}

void *rem(Array* array, int index) {
  void *rem = NULL;
  if (index >= 0 && index < array->size) {
    void *tmp = malloc(array->element_size);
    memcpy(tmp, (char*)array->content + (index * array->element_size), array->element_size);
    memcpy((char*)array->content + (index       * array->element_size),
           (char*)array->content + ((index + 1) * array->element_size),
           (array->size - index) * array->element_size);
    rem = (char*)array->content + (--array->size * array->element_size);
    memcpy(rem, tmp, array->element_size);
    free(tmp);
  }
  return rem;
}