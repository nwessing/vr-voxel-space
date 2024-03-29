#include "platform.h"
#include "stdint.h"
#include <stdio.h>
#include <stdlib.h>
/* #include "file.h" */

// Read a file into a char array, caller is responsible for
// free() call
char *read_file(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    error("could not load file %s\n", filename);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  uint32_t length = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *file_buffer = malloc(sizeof(char) * (length + 1));
  fread(file_buffer, 1, length, file);
  file_buffer[length] = '\0';

  fclose(file);

  return file_buffer;
}

uint32_t read_binary_file(const char *filename, uint8_t **data) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    error("could not load file %s\n", filename);
    return 0;
  }

  fseek(file, 0, SEEK_END);
  uint32_t length = ftell(file);
  fseek(file, 0, SEEK_SET);
  *data = malloc(sizeof(uint8_t) * length);
  fread(*data, sizeof(uint8_t), length, file);

  fclose(file);

  return length;
}
