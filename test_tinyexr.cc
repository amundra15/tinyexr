#include <cstdlib>
#include <cstdio>
#include <vector>

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

//#define SIMPLE_API_EXAMPLE

#ifdef SIMPLE_API_EXAMPLE

static void
SaveAsPFM(const char* filename, int width, int height, float* data)
{
#ifdef _WIN32
  FILE* fp = NULL;
  fopen_s(&fp, filename, "wb");
#else
  FILE* fp = fopen(filename, "wb");
#endif
  if (!fp) {
    fprintf(stderr, "failed to write a PFM file.\n");
    return;
  }

  fprintf(fp, "PF\n");
  fprintf(fp, "%d %d\n", width, height);
  fprintf(fp, "-1\n"); // -1: little endian, 1: big endian

  // RGBA -> RGB
  std::vector<float> rgb(width*height*3);

  for (int i = 0; i < width * height; i++) {
    rgb[3*i+0] = data[4*i+0];
    rgb[3*i+1] = data[4*i+1];
    rgb[3*i+2] = data[4*i+2];
  }
  
  fwrite(&rgb.at(0), sizeof(float), width * height * 3, fp);

  fclose(fp);
}

#else

static const char* GetPixelType(int id)
{
  if (id == TINYEXR_PIXELTYPE_HALF) {
    return "HALF";
  } else if (id == TINYEXR_PIXELTYPE_FLOAT) {
    return "FLOAT";
  } else if (id == TINYEXR_PIXELTYPE_UINT) {
    return "UINT";
  }

  return "???";
}


// Simple tile -> scanline converter. Assumes FLOAT pixel type for all channels.
static void
TiledImageToScanlineImage(EXRImage* src, const EXRHeader* header)
{
  size_t data_width  = header->data_window[2] - header->data_window[0] + 1;
  size_t data_height = header->data_window[3] - header->data_window[1] + 1;

  src->images = static_cast<unsigned char**>(malloc(sizeof(float*) * header->num_channels));
  for (size_t c = 0; c < static_cast<size_t>(header->num_channels); c++) {
    assert(header->pixel_types[c] == TINYEXR_PIXELTYPE_FLOAT);
    src->images[c] = static_cast<unsigned char*>(malloc(sizeof(float) * data_width * data_height));
    memset(src->images[c], 0, sizeof(float) * data_width * data_height);
  }

  for (size_t tile_idx = 0; tile_idx < static_cast<size_t>(src->num_tiles); tile_idx++) {

    int sx = src->tiles[tile_idx].offset_x * header->tile_size_x;
    int sy = src->tiles[tile_idx].offset_y * header->tile_size_y;
    int ex = src->tiles[tile_idx].offset_x * header->tile_size_x + src->tiles[tile_idx].width;
    int ey = src->tiles[tile_idx].offset_y * header->tile_size_y + src->tiles[tile_idx].height;

    for (size_t c = 0; c < static_cast<size_t>(header->num_channels); c++) {
      float *dst_image = reinterpret_cast<float*>(src->images[c]);
      const float *src_image = reinterpret_cast<const float*>(src->tiles[tile_idx].images[c]);
      for (size_t y = 0; y < static_cast<size_t>(ey - sy); y++) {
        for (size_t x = 0; x < static_cast<size_t>(ex - sx); x++) {
          dst_image[(y + sy) * data_width + (x + sx)] = src_image[y * header->tile_size_x + x];
        }
      }
    }

  }

}
#endif

int
main(int argc, char** argv)
{
  const char* outfilename = "output_test.exr";
  const char* err;

  if (argc < 2) {
    fprintf(stderr, "Needs input.exr.\n");
    exit(-1);
  }

  if (argc > 2) {
    outfilename = argv[2];
  }

#ifdef SIMPLE_API_EXAMPLE
  int width, height;
  float* image;
  int ret = LoadEXR(&image, &width, &height, argv[1], &err);
  if (ret != 0) {
    fprintf(stderr, "Load EXR err: %s\n", err);
    return ret;
  }
  (void)GetPixelType;
  (void)outfilename;
  SaveAsPFM("output.pfm", width, height, image);
#else
    
  EXRVersion exr_version;

  int ret = ParseEXRVersionFromFile(&exr_version, argv[1]);
  if (ret != 0) {
    fprintf(stderr, "Invalid EXR file: %s\n", argv[1]);
    return -1;
  }

  printf("version: tiled = %d, long_name = %d, non_image = %d, multipart = %d\n",
    exr_version.tiled,
    exr_version.long_name,
    exr_version.non_image,
    exr_version.multipart);
    
  if (exr_version.multipart) {

    EXRHeader **exr_headers; // list of EXRHeader pointers.
    int num_exr_headers;

    ret = ParseEXRMultipartHeaderFromFile(&exr_headers, &num_exr_headers, &exr_version, argv[1], &err);
    if (ret != 0) {
      fprintf(stderr, "Parse EXR err: %s\n", err);
      return ret;
    }

    printf("num parts = %d\n", num_exr_headers);

    for (size_t i = 0; i < static_cast<size_t>(num_exr_headers); i++) { 
      const EXRHeader &exr_header = *(exr_headers[i]);

      printf("Part: %lu\n", static_cast<unsigned long>(i));

      printf("dataWindow = %d, %d, %d, %d\n",
        exr_header.data_window[0],
        exr_header.data_window[1],
        exr_header.data_window[2],
        exr_header.data_window[3]);
      printf("displayWindow = %d, %d, %d, %d\n",
        exr_header.display_window[0],
        exr_header.display_window[1],
        exr_header.display_window[2],
        exr_header.display_window[3]);
      printf("screenWindowCenter = %f, %f\n",
        static_cast<double>(exr_header.screen_window_center[0]),
        static_cast<double>(exr_header.screen_window_center[1]));
      printf("screenWindowWidth = %f\n",
        static_cast<double>(exr_header.screen_window_width));
      printf("pixelAspectRatio = %f\n",
        static_cast<double>(exr_header.pixel_aspect_ratio));
      printf("lineOrder = %d\n",
        exr_header.line_order);

      if (exr_header.num_custom_attributes > 0) {
        printf("# of custom attributes = %d\n", exr_header.num_custom_attributes);
        for (int a = 0; a < exr_header.num_custom_attributes; a++) {
          printf("  [%d] name = %s, type = %s, size = %d\n", a,
            exr_header.custom_attributes[a].name,
            exr_header.custom_attributes[a].type,
            exr_header.custom_attributes[a].size);
          //if (strcmp(exr_header.custom_attributes[i].type, "float") == 0) {
          //  printf("    value = %f\n", *reinterpret_cast<float *>(exr_header.custom_attributes[i].value));
          //}
        }
      }
    }


    std::vector<EXRImage> images(num_exr_headers);
    for (int i =0; i < num_exr_headers; i++) {
      InitEXRImage(&images[i]);
    }

    ret = LoadEXRMultipartImageFromFile(&images.at(0), const_cast<const EXRHeader**>(exr_headers), num_exr_headers, argv[1], &err);
    if (ret != 0) {
      fprintf(stderr, "Load EXR err: %s\n", err);
      return ret;
    }

    printf("Loaded %d part images\n", num_exr_headers);
    printf("There is no saving feature for multi-part images, thus just exit an application...\n");

    for (int i =0; i < num_exr_headers; i++) {
      FreeEXRImage(&images.at(i));
    }

    for (int i =0; i < num_exr_headers; i++) {
      FreeEXRHeader(exr_headers[i]);
      free(exr_headers[i]);
    }
    free(exr_headers);

  } else { // single-part EXR

    EXRHeader exr_header;
    InitEXRHeader(&exr_header);

    ret = ParseEXRHeaderFromFile(&exr_header, &exr_version, argv[1], &err);
    if (ret != 0) {
      fprintf(stderr, "Parse EXR err: %s\n", err);
      return ret;
    }

    printf("dataWindow = %d, %d, %d, %d\n",
      exr_header.data_window[0],
      exr_header.data_window[1],
      exr_header.data_window[2],
      exr_header.data_window[3]);
    printf("displayWindow = %d, %d, %d, %d\n",
      exr_header.display_window[0],
      exr_header.display_window[1],
      exr_header.display_window[2],
      exr_header.display_window[3]);
    printf("screenWindowCenter = %f, %f\n",
      static_cast<double>(exr_header.screen_window_center[0]),
      static_cast<double>(exr_header.screen_window_center[1]));
    printf("screenWindowWidth = %f\n",
      static_cast<double>(exr_header.screen_window_width));
    printf("pixelAspectRatio = %f\n",
      static_cast<double>(exr_header.pixel_aspect_ratio));
    printf("lineOrder = %d\n",
      exr_header.line_order);

    if (exr_header.num_custom_attributes > 0) {
      printf("# of custom attributes = %d\n", exr_header.num_custom_attributes);
      for (int i = 0; i < exr_header.num_custom_attributes; i++) {
        printf("  [%d] name = %s, type = %s, size = %d\n", i,
          exr_header.custom_attributes[i].name,
          exr_header.custom_attributes[i].type,
          exr_header.custom_attributes[i].size);
        //if (strcmp(exr_header.custom_attributes[i].type, "float") == 0) {
        //  printf("    value = %f\n", *reinterpret_cast<float *>(exr_header.custom_attributes[i].value));
        //}
      }
    }

    // Read HALF channel as FLOAT.
    for (int i = 0; i < exr_header.num_channels; i++) {
      if (exr_header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
        exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
      }
    }

    EXRImage exr_image;
    InitEXRImage(&exr_image);

    ret = LoadEXRImageFromFile(&exr_image, &exr_header, argv[1], &err);
    if (ret != 0) {
      fprintf(stderr, "Load EXR err: %s\n", err);
      return ret;
    }

    printf("EXR: %d x %d\n", exr_image.width, exr_image.height);

    for (int i = 0; i < exr_header.num_channels; i++) {
      printf("pixelType[%d]: %s\n", i, GetPixelType(exr_header.pixel_types[i]));
      printf("chan[%d] = %s\n", i, exr_header.channels[i].name);
      printf("requestedPixelType[%d]: %s\n", i, GetPixelType(exr_header.requested_pixel_types[i]));
    }

#if 0 // example to write custom attribute
    int version_minor = 3;
    exr_header.num_custom_attributes = 1;
    exr_header.custom_attributes[0].name = strdup("tinyexr_version_minor");
    exr_header.custom_attributes[0].type = strdup("int");
    exr_header.custom_attributes[0].size = sizeof(int);
    exr_header.custom_attributes[0].value = (unsigned char*)malloc(sizeof(int));
    memcpy(exr_header.custom_attributes[0].value, &version_minor, sizeof(int));
#endif

    if (exr_header.tiled) {
      TiledImageToScanlineImage(&exr_image, &exr_header);
    }

    exr_header.compression_type = TINYEXR_COMPRESSIONTYPE_NONE;
    ret = SaveEXRImageToFile(&exr_image, &exr_header, outfilename, &err);
    if (ret != 0) {
      fprintf(stderr, "Save EXR err: %s\n", err);
      return ret;
    }
    printf("Saved exr file. [ %s ] \n", outfilename);

    FreeEXRHeader(&exr_header);
    FreeEXRImage(&exr_image);
  }
#endif

  return ret;
}