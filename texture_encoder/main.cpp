#include <stdio.h>

#include "astcenc.h"
#include <fstream>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include "stb_image_write.h"

#define MAP_COUNT 30
static const char *maps[MAP_COUNT] = {
    "maps/C1W.png",  "maps/C2W.png",  "maps/C3.png",   "maps/C4.png",
    "maps/C5W.png",  "maps/C6W.png",  "maps/C7W.png",  "maps/C8.png",
    "maps/C9W.png",  "maps/C10W.png", "maps/C11W.png", "maps/C12W.png",
    "maps/C13.png",  "maps/C14.png",  "maps/C14W.png", "maps/C15.png",
    "maps/C16W.png", "maps/C17W.png", "maps/C18W.png", "maps/C19W.png",
    "maps/C20W.png", "maps/C21.png",  "maps/C22W.png", "maps/C23W.png",
    "maps/C24W.png", "maps/C25W.png", "maps/C26W.png", "maps/C27W.png",
    "maps/C28W.png", "maps/C29W.png"};

static const unsigned int thread_count = 1;
static const unsigned int block_x = 6;
static const unsigned int block_y = 6;
static const unsigned int block_z = 1;
static const astcenc_profile profile = ASTCENC_PRF_LDR;
static const float quality = ASTCENC_PRE_EXHAUSTIVE;
static const astcenc_swizzle swizzle{ASTCENC_SWZ_R, ASTCENC_SWZ_G,
                                     ASTCENC_SWZ_B, ASTCENC_SWZ_A};
/* ============================================================================
        ASTC compressed file loading
============================================================================ */
struct astc_header {
  uint8_t magic[4];
  uint8_t block_x;
  uint8_t block_y;
  uint8_t block_z;
  uint8_t dim_x[3]; // dims = dim[0] + (dim[1] << 8) + (dim[2] << 16)
  uint8_t dim_y[3]; // Sizes are given in texels;
  uint8_t dim_z[3]; // block count is inferred
};
static const uint32_t ASTC_MAGIC_ID = 0x5CA1AB13;

/**
 * @brief The payload stored in a compressed ASTC image.
 */
struct astc_compressed_image {
  /** @brief The block width in texels. */
  unsigned int block_x;

  /** @brief The block height in texels. */
  unsigned int block_y;

  /** @brief The block depth in texels. */
  unsigned int block_z;

  /** @brief The image width in texels. */
  unsigned int dim_x;

  /** @brief The image height in texels. */
  unsigned int dim_y;

  /** @brief The image depth in texels. */
  unsigned int dim_z;

  /** @brief The binary data payload. */
  uint8_t *data;

  /** @brief The binary data length in bytes. */
  size_t data_len;
};

int store_cimage(const astc_compressed_image &img, const char *filename) {
  astc_header hdr;
  hdr.magic[0] = ASTC_MAGIC_ID & 0xFF;
  hdr.magic[1] = (ASTC_MAGIC_ID >> 8) & 0xFF;
  hdr.magic[2] = (ASTC_MAGIC_ID >> 16) & 0xFF;
  hdr.magic[3] = (ASTC_MAGIC_ID >> 24) & 0xFF;

  hdr.block_x = static_cast<uint8_t>(img.block_x);
  hdr.block_y = static_cast<uint8_t>(img.block_y);
  hdr.block_z = static_cast<uint8_t>(img.block_z);

  hdr.dim_x[0] = img.dim_x & 0xFF;
  hdr.dim_x[1] = (img.dim_x >> 8) & 0xFF;
  hdr.dim_x[2] = (img.dim_x >> 16) & 0xFF;

  hdr.dim_y[0] = img.dim_y & 0xFF;
  hdr.dim_y[1] = (img.dim_y >> 8) & 0xFF;
  hdr.dim_y[2] = (img.dim_y >> 16) & 0xFF;

  hdr.dim_z[0] = img.dim_z & 0xFF;
  hdr.dim_z[1] = (img.dim_z >> 8) & 0xFF;
  hdr.dim_z[2] = (img.dim_z >> 16) & 0xFF;

  std::ofstream file(filename, std::ios::out | std::ios::binary);
  if (!file) {
    printf("ERROR: File open failed '%s'\n", filename);
    return 1;
  }

  file.write((char *)&hdr, sizeof(astc_header));
  file.write((char *)img.data, img.data_len);
  return 0;
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  for (int32_t i = 0; i < MAP_COUNT; ++i) {
    const char *filename = maps[i];
    int32_t original_image_width, original_image_height, image_c;
    uint8_t *original_image_data = (uint8_t *)stbi_load(
        filename, &original_image_width, &original_image_height, &image_c, 4);
    if (original_image_data == nullptr) {
      printf("ERROR: image %s not found\n", filename);
      return EXIT_FAILURE;
    }

    uint32_t mip_level = 0;
    int32_t prev_image_width = original_image_width,
            prev_image_height = original_image_height,
            image_width = original_image_width,
            image_height = original_image_height;
    uint8_t *prev_image_data = original_image_data;
    while (true) {
      uint8_t *image_data{};
      if (mip_level == 0) {
        image_data = prev_image_data;
      } else {
        image_data =
            reinterpret_cast<uint8_t *>(malloc(image_width * image_height * 4));

        printf("Resizing %d-component from %dx%d to %dx%d\n", 4,
               prev_image_height, prev_image_height, image_width, image_height);
        if (!stbir_resize_uint8(prev_image_data, prev_image_width,
                                prev_image_height, 0, image_data, image_width,
                                image_height, 0, 4)) {
          printf("ERROR: Could not resize %s from %dx%d to %dx%d\n", filename,
                 prev_image_width, prev_image_height, image_width,
                 image_height);
          return EXIT_FAILURE;
        }
        // stbi_write_png("astc/mipttest.png", image_width, image_height,
        // 4,
        //                image_data, image_width * 4);
        // return 1;
      }

      unsigned int block_count_x = (image_width + block_x - 1) / block_x;
      unsigned int block_count_y = (image_height + block_y - 1) / block_y;
      astcenc_config config;
      config.block_x = block_x;
      config.block_y = block_y;
      config.profile = profile;
      astcenc_error status;
      status = astcenc_config_init(profile, block_x, block_y, block_z, quality,
                                   0, &config);
      if (status != ASTCENC_SUCCESS) {
        printf("ERROR: Codec config init failed: %s\n",
               astcenc_get_error_string(status));
        return 1;
      }

      astcenc_context *context;
      status = astcenc_context_alloc(&config, thread_count, &context);
      if (status != ASTCENC_SUCCESS) {
        printf("ERROR: Codec context alloc failed: %s\n",
               astcenc_get_error_string(status));
        return 1;
      }

      astcenc_image image;
      image.dim_x = image_width;
      image.dim_y = image_height;
      image.dim_z = 1;
      image.data_type = ASTCENC_TYPE_U8;
      // uint8_t *slices = image_data;
      image.data = reinterpret_cast<void **>(&image_data);

      // Space needed for 16 bytes of output per compressed block
      size_t comp_len = block_count_x * block_count_y * 16;
      uint8_t *comp_data = new uint8_t[comp_len];

      status = astcenc_compress_image(context, &image, &swizzle, comp_data,
                                      comp_len, 0);
      if (status != ASTCENC_SUCCESS) {
        printf("ERROR: Codec compress failed: %s\n",
               astcenc_get_error_string(status));
        return 1;
      }

      astc_compressed_image meta{};
      meta.dim_x = image_width;
      meta.dim_y = image_height;
      meta.dim_z = 1;
      meta.block_x = block_x;
      meta.block_y = block_y;
      meta.block_z = 1;
      meta.data = comp_data;
      meta.data_len = comp_len;
      std::stringstream output_name_stream{};
      output_name_stream << filename;
      if (mip_level != 0) {
        output_name_stream << mip_level;
      }
      output_name_stream << ".astc";
      std::string output = output_name_stream.str();
      store_cimage(meta, output.c_str());
      delete[] comp_data;
      printf("%s image size = %zu\n", output.c_str(), comp_len);

      if (prev_image_data != original_image_data) {
        free(prev_image_data);
      }

      prev_image_height = image_height;
      prev_image_width = image_width;
      prev_image_data = image_data;

      if (image_width == 1 && image_height == 1) {
        free(image_data);
        break;
      }

      if (image_width != 1) {
        image_width = image_width >> 1;
      }

      if (image_height != 1) {
        image_height = image_height >> 1;
      }
      ++mip_level;
    }

    stbi_image_free(original_image_data);
  }

  printf("Done!\n");
}
