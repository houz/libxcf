#include "xcf.h"

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#if defined(_WIN32)
  #include <windows.h>
  #if BYTE_ORDER == LITTLE_ENDIAN
    #if defined(_MSC_VER)
      #define htobe16(x) _byteswap_ushort(x)
      #define htobe32(x) _byteswap_ulong(x)
      #define htobe64(x) _byteswap_uint64(x)
    #elif defined(__GNUC__) || defined(__clang__)
      #define htobe16(x) __builtin_bswap16(x)
      #define htobe32(x) __builtin_bswap32(x)
      #define htobe64(x) __builtin_bswap64(x)
    #endif
  #else
    #define htobe16(x) (x)
    #define htobe32(x) (x)
    #define htobe64(x) (x)
  #endif
#elif defined(__APPLE__)
  #include <libkern/OSByteOrder.h>
  #define htobe16(x) OSSwapHostToBigInt16(x)
  #define htobe32(x) OSSwapHostToBigInt32(x)
  #define htobe64(x) OSSwapHostToBigInt64(x)
#elif defined(__OpenBSD__)
  #include <sys/endian.h>
#elif  defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__)
  #include <sys/endian.h>
//   #define htobe16(x) noideawhat(x)
//   #define htobe32(x) noideawhat(x)
//   #define htobe64(x) noideawhat(x)
#else
  #include <endian.h>
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(a, b, c) ((a) < (b) ? (b) : ((a) > (c) ? (c) : (a)))

#ifndef PRINT_ERROR
#define PRINT_ERROR(msg, ...) fprintf(stderr, "[libxcf] " msg "\n", ##__VA_ARGS__)
#endif

#define CHECK_VERSION(_xcf, _check, _version, _msg)                        \
  if(_check)                                                               \
  {                                                                        \
    if(_xcf->image.version < _version)                                     \
    {                                                                      \
      PRINT_ERROR("error: %s requires at least version %d but %d is used", \
                  _msg, _version, _xcf->image.version);                    \
      _xcf->state = XCF_STATE_ERROR;                                       \
      return 0;                                                            \
    }                                                                      \
    else                                                                   \
      _xcf->min_version = MAX(_xcf->min_version, _version);                \
  }

#define CHECK_VERSION_LAYERMODE(_xcf, _mode, _version)                                                       \
  if(_xcf->child.p_mode >= _mode)                                                                            \
  {                                                                                                          \
    if(_xcf->image.version < _version)                                                                       \
    {                                                                                                        \
      PRINT_ERROR("error: layermode '%s' (%d) requires at least version %d but %d is used",                  \
                  xcf_get_mode_name(_xcf->child.p_mode), _xcf->child.p_mode, _version, _xcf->image.version); \
      _xcf->state = XCF_STATE_ERROR;                                                                         \
      return 0;                                                                                              \
    }                                                                                                        \
    else                                                                                                     \
      _xcf->min_version = MAX(_xcf->min_version, _version);                                                  \
  }

#define CHECK_IO(_xcf, _write, _expected) \
  if(_write != _expected)                 \
  {                                       \
    PRINT_ERROR("error: io error");       \
    _xcf->state = XCF_STATE_ERROR;        \
    return 0;                             \
  }

#define TILE_SIZE 64

typedef struct xcf_parasite_t
{
  char *name;
  uint32_t flags;
  uint32_t length;
  uint8_t *data;
  struct xcf_parasite_t *next;
} xcf_parasite_t;

struct xcf_t
{
  FILE *fd;
  xcf_state_t state; // this library is a state machine, see state.dot

  uint32_t n_layers, n_channels;
  uint32_t next_layer, next_channel; // the number of the next layer or channel to write

  uint32_t omit_base_alpha;

  int min_version; // the minimal version required for the features used. this gets bumped while writing the image

  // fields in the image header
  struct
  {
    int version; // the version used to write the file
    uint32_t width, height;
    xcf_base_type_t base_type;
    xcf_precision_t precision;

    // file offsets of the layer and channel lists
    uint32_t layer_list, channel_list;

    // some properties. instead of writing them in xcf_set() we postpone writing until finalizing the header so
    // we can have sane defaults while still allowing the user to set it
    // TODO: p_colormap // min_version for that is 1
    uint8_t p_compression; // we only support zlib and uncompressed. rle is missing

    // parasites. this is a single linked list
    xcf_parasite_t *parasites;
  } image;

  // header of the current layer or channel
  struct
  {
    int n;
    uint32_t width, height;
    char *name;

    // for layers this gets computed in xcf_write_layer_header()
    // for channels it is always XCF_TYPE_GRAYSCALE and set in xcf_add_channel()
    // it is used in xcf_add_data()
    xcf_type_t type;

    // common properties
    float p_opacity;
    uint32_t p_visible;

    // channel properties
    float p_color[3];

    // layer properties
    int32_t p_composite_mode;
    int32_t p_composite_space;
    int32_t p_blend_space;
    int32_t p_mode;
    int32_t p_offset_x, p_offset_y;

    // parasites. this is a single linked list
    xcf_parasite_t *parasites;
  } child;

};


// number of bytes used for internal pointers in the file. either 32 or 64, depending on version
static int xcf_pointer_size(XCF *xcf)
{
  if(abs(xcf->image.version) <= 10)
    return 4;
  else
    return 8;
}

static uint32_t xcf_strlen(const char *value)
{
  if(!value || !*value)
    return sizeof(uint32_t);
  return sizeof(uint32_t) + strlen(value) + 1;
}


// functions for writing to a file, taking endianess into account

static int xcf_write_uint8(XCF *xcf, const uint8_t value) __attribute__ ((warn_unused_result));
static int xcf_write_uint8(XCF *xcf, const uint8_t value)
{
  return fwrite(&value, sizeof(value), 1, xcf->fd) == 1;
}

static int xcf_write_uint32(XCF *xcf, const uint32_t value) __attribute__ ((warn_unused_result));
static int xcf_write_uint32(XCF *xcf, const uint32_t value)
{
  const uint32_t value_be = htobe32(value);
  return fwrite(&value_be, sizeof(value_be), 1, xcf->fd) == 1;
}

static int xcf_write_float(XCF *xcf, const float value) __attribute__ ((warn_unused_result));
static int xcf_write_float(XCF *xcf, const float value)
{
  union {float f; uint32_t i;} v;
  v.f = value;
  const uint32_t value_be = htobe32(v.i);
  return fwrite(&value_be, sizeof(value_be), 1, xcf->fd) == 1;
}

static int xcf_write_uint64(XCF *xcf, const uint64_t value) __attribute__ ((warn_unused_result));
static int xcf_write_uint64(XCF *xcf, const uint64_t value)
{
  const uint64_t value_be = htobe64(value);
  return fwrite(&value_be, sizeof(value_be), 1, xcf->fd) == 1;
}

static int xcf_write_pointer(XCF *xcf, const uint64_t value) __attribute__ ((warn_unused_result));
static int xcf_write_pointer(XCF *xcf, const uint64_t value)
{
  if(xcf_pointer_size(xcf) == 4)
    return xcf_write_uint32(xcf, value);
  else
    return xcf_write_uint64(xcf, value);
}

static int xcf_write_string(XCF *xcf, const char *value) __attribute__ ((warn_unused_result));
static int xcf_write_string(XCF *xcf, const char *value)
{
  if(!value || !*value)
    return xcf_write_uint32(xcf, 0);
  else
  {
    const size_t len = strlen(value);
    if(!xcf_write_uint32(xcf, len + 1)) return 0;
    return fwrite(value, 1, len + 1, xcf->fd) == len + 1;
  }
}


// handling of parasites

static int xcf_write_parasites(XCF *xcf, const xcf_parasite_t *head) __attribute__ ((warn_unused_result));
static int xcf_write_parasites(XCF *xcf, const xcf_parasite_t *head)
{
  // get total length of the data.
  // we don't pre-compute that in xcf_parasites_add() because it's an implementation detail not relevant there.
  uint32_t plength = 0;
  for(const xcf_parasite_t *parasite = head; parasite; parasite = parasite->next)
    plength += xcf_strlen(parasite->name) + sizeof(uint32_t) + sizeof(uint32_t) + parasite->length;

  if(!xcf_write_uint32(xcf, XCF_PROP_PARASITES)) return 0;
  if(!xcf_write_uint32(xcf, plength)) return 0;
  for(const xcf_parasite_t *parasite = head; parasite; parasite = parasite->next)
  {
    if(!xcf_write_string(xcf, parasite->name)) return 0;
    if(!xcf_write_uint32(xcf, parasite->flags)) return 0;
    if(!xcf_write_uint32(xcf, parasite->length)) return 0;
    if(fwrite(parasite->data, 1, parasite->length, xcf->fd) != parasite->length) return 0;
  }
  return 1;
}

// add a parasite to the list if it's not there or change the existing one if it's already present
// returns the new start of the list
static xcf_parasite_t *xcf_parasites_add(xcf_parasite_t *head, const char *name, const uint32_t flags,
                                         const uint32_t length, const uint8_t *data)
{
  // without a name there is nothing to add later
  if(!name) return head;

  xcf_parasite_t *parasite;

  if(!head)
  {
    parasite = (xcf_parasite_t *)malloc(sizeof(xcf_parasite_t));
    parasite->name = strdup(name);
    parasite->next = NULL;
    head = parasite;
  }
  else
  {
    xcf_parasite_t *last;
    for(last = NULL, parasite = head; parasite; last = parasite, parasite = parasite->next)
    {
      if(!strcmp(name, parasite->name))
      {
        // update a parasite that was set earlier
        free(parasite->data);
        parasite->data = NULL;
        parasite->length = 0;
        break;
      }
    }
    if(!parasite)
    {
      // allocate a new one and append it
      parasite = (xcf_parasite_t *)malloc(sizeof(xcf_parasite_t));
      parasite->name = strdup(name);
      parasite->next = NULL;
      last->next = parasite;
    }
  }

  parasite->flags = flags;
  parasite->length = length;
  parasite->data = (uint8_t *)malloc(length);
  memcpy(parasite->data, data, length);

  return head;
}

static void xcf_parasites_free(xcf_parasite_t *head)
{
  while(head)
  {
    xcf_parasite_t *next = head->next;
    free(head->name);
    free(head->data);
    head->name = NULL;
    head->data = NULL;
    head->next = NULL;
    free(head);
    head = next;
  }
}


// internal helpers

static int xcf_write_image_header(XCF *xcf)
{
  if(xcf->state != XCF_STATE_IMAGE)
  {
    PRINT_ERROR("error: the image header has already been written");
    xcf->state = XCF_STATE_ERROR;
    return 0;
  }

  if(xcf->image.p_compression == XCF_PROP_COMPRESSION_RLE)
  {
    PRINT_ERROR("error: rle compression is not supported");
    xcf->state = XCF_STATE_ERROR;
    return 0;
  }

  CHECK_VERSION(xcf, (xcf->image.precision != XCF_PRECISION_I_8_G), 7, "image precision other than 8 bit gamma");
  CHECK_VERSION(xcf, xcf->image.precision > XCF_PRECISION_I_8_G, 12, "image encoding other than 8 bit integer");
  CHECK_VERSION(xcf, xcf->image.p_compression == XCF_PROP_COMPRESSION_ZLIB, 8, "zlib compression")
  // estimate if the image will be really big from width, height, base_type, precision, n_channels and n_layers
  const size_t image_size_estimate = 0; // TODO
  CHECK_VERSION(xcf, (image_size_estimate >= ((int64_t) 1 << 32)), 11, "an image size bigger than 4GB");

  char version[9 + 4 + 1] = "gimp xcf ";
  const int v = abs(xcf->image.version);
  if(v == 0)
    strncpy(version + 9, "file", 5);
  else if(v <= 999)
    snprintf(version + 9, 5, "v%03d", v);
  else
  {
    PRINT_ERROR("error: version %d is too big", v);
    xcf->state = XCF_STATE_ERROR;
    return 0;
  }
  if(fwrite(version, 1, sizeof(version), xcf->fd) != sizeof(version))
  {
    PRINT_ERROR("error: can't write to file");
    xcf->state = XCF_STATE_ERROR;
    return 0;
  }

  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->image.width), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->image.height), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->image.base_type), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->image.precision), 1);

  // write properties and parasites
  // TODO: colormap -- min_version is 1 for that
  // compression
  CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_COMPRESSION), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, 1), 1);
  CHECK_IO(xcf, xcf_write_uint8(xcf, xcf->image.p_compression), 1);
  // parasites
  CHECK_IO(xcf, xcf_write_parasites(xcf, xcf->image.parasites), 1);

  // close the property list by adding PROP_END
  CHECK_IO(xcf, xcf_write_uint32(xcf, 0), 1); // type
  CHECK_IO(xcf, xcf_write_uint32(xcf, 0), 1); // size

  // add dummy pointer lists for layers and channels and remember the file offset so we can set it later
  xcf->image.layer_list = ftell(xcf->fd);
  CHECK_IO(xcf, fseek(xcf->fd, xcf_pointer_size(xcf) * xcf->n_layers, SEEK_CUR), 0);
  CHECK_IO(xcf, xcf_write_pointer(xcf, 0), 1);

  xcf->image.channel_list = ftell(xcf->fd);
  CHECK_IO(xcf, fseek(xcf->fd, xcf_pointer_size(xcf) * xcf->n_channels, SEEK_CUR), 0);
  CHECK_IO(xcf, xcf_write_pointer(xcf, 0), 1);

  xcf->state = XCF_STATE_MAIN;
  return 1;
}

// store a pointer to the current position in a list
static int xcf_register_pointer(XCF *xcf, uint32_t list_start, uint32_t n) __attribute__((warn_unused_result));
static int xcf_register_pointer(XCF *xcf, uint32_t list_start, uint32_t n)
{
  const uint64_t list_entry = list_start + n * xcf_pointer_size(xcf);
  uint64_t current_pos = ftell(xcf->fd);
  CHECK_IO(xcf, fseek(xcf->fd, list_entry, SEEK_SET), 0);
  CHECK_IO(xcf, xcf_write_pointer(xcf, current_pos), 1);
  CHECK_IO(xcf, fseek(xcf->fd, 0, SEEK_END), 0);
  return 1;
}

static int xcf_write_layer_header(XCF *xcf)
{
  if(xcf->state != XCF_STATE_LAYER)
  {
    PRINT_ERROR("error: there is no layer header to be written");
    xcf->state = XCF_STATE_ERROR;
    return 0;
  }

  // store pointer in the global layer list
  CHECK_IO(xcf, xcf_register_pointer(xcf, xcf->image.layer_list, xcf->child.n), 1);

  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.width), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.height), 1);
  // the type has to be the same as the one for the whole image.
  switch(xcf->image.base_type)
  {
    case XCF_BASE_TYPE_RGB:       xcf->child.type = XCF_TYPE_RGB_ALPHA;       break;
    case XCF_BASE_TYPE_GRAYSCALE: xcf->child.type = XCF_TYPE_GRAYSCALE_ALPHA; break;
    case XCF_BASE_TYPE_INDEXED:   xcf->child.type = XCF_TYPE_INDEXED_ALPHA;   break;
    default:
    {
      const char *name = xcf_get_base_type_name(xcf->image.base_type);
      if(name)
        PRINT_ERROR("error: unknown base type '%s'", name);
      else
        PRINT_ERROR("error: unknown base type %d", xcf->image.base_type);
      xcf->state = XCF_STATE_ERROR;
      return 0;
    }
  }
  // the base layer can have no alpha channel. omit it to get smaller files
  // this is configurable with XCF_OMIT_BASE_ALPHA so the user can have alpha data for the base layer!
  if(xcf->omit_base_alpha && xcf->next_layer == xcf->n_layers)
    xcf->child.type -= 1;

  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.type), 1);
  CHECK_IO(xcf, xcf_write_string(xcf, xcf->child.name), 1);

  // write properties and parasites
  // opacity
  CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_OPACITY), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, 4), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, CLAMP(xcf->child.p_opacity * 255, 0, 255)), 1);
  // mode
  // use XCF_PROP_MODE_NORMAL for version >= 10 else XCF_PROP_MODE_LEGACY_NORMAL if nothing was set explicitly
  if(xcf->child.p_mode < 0)
  {
    if(xcf->image.version >= 10)
      xcf->child.p_mode = XCF_PROP_MODE_NORMAL;
    else
      xcf->child.p_mode = XCF_PROP_MODE_LEGACY_NORMAL;
  }
  CHECK_VERSION_LAYERMODE(xcf, XCF_PROP_MODE_NORMAL, 10);
  CHECK_VERSION_LAYERMODE(xcf, XCF_PROP_MODE_OVERLAY, 9);
  CHECK_VERSION_LAYERMODE(xcf, XCF_PROP_MODE_LEGACY_SOFT_LIGHT, 2);
  CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_MODE), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, 4), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.p_mode), 1);
  // visible
  CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_VISIBLE), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, 4), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.p_visible), 1);
  // offsets
  CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_OFFSETS), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, 8), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.p_offset_x), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.p_offset_y), 1);
  // thses properties were added in 2.10.0 (i presume version 4?)
  // if the user set composite mode or space they should be >= 0 and affect min_version
  // if they are < 0 then only write them if version >= 4
  if(xcf->image.version >= 4
    || xcf->child.p_composite_mode >= 0
    || xcf->child.p_composite_space >= 0
    || xcf->child.p_blend_space >= 0)
  {
    CHECK_VERSION(xcf, xcf->child.p_composite_mode >= 0, 4, "setting a composite mode");
    CHECK_VERSION(xcf, xcf->child.p_composite_space >= 0, 4, "setting a composite space");
    CHECK_VERSION(xcf, xcf->child.p_blend_space >= 0, 4, "setting a blend space");
    // float opacity
    CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_FLOAT_OPACITY), 1);
    CHECK_IO(xcf, xcf_write_uint32(xcf, 4), 1);
    CHECK_IO(xcf, xcf_write_float(xcf, xcf->child.p_opacity), 1);
    // composite mode
    CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_COMPOSITE_MODE), 1);
    CHECK_IO(xcf, xcf_write_uint32(xcf, 4), 1);
    CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.p_composite_mode), 1);
    // composite space
    CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_COMPOSITE_SPACE), 1);
    CHECK_IO(xcf, xcf_write_uint32(xcf, 4), 1);
    CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.p_composite_space), 1);
    // blend space
    CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_BLEND_SPACE), 1);
    CHECK_IO(xcf, xcf_write_uint32(xcf, 4), 1);
    CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.p_blend_space), 1);
  }
  // parasites
  CHECK_IO(xcf, xcf_write_parasites(xcf, xcf->child.parasites), 1);

  // close the property list by adding PROP_END
  CHECK_IO(xcf, xcf_write_uint32(xcf, 0), 1); // type
  CHECK_IO(xcf, xcf_write_uint32(xcf, 0), 1); // size

  // the hierarchy struct comes rigth after the layer
  const uint64_t current_pos = ftell(xcf->fd);
  CHECK_IO(xcf, xcf_write_pointer(xcf, current_pos + 2 * xcf_pointer_size(xcf)), 1);
  CHECK_IO(xcf, xcf_write_pointer(xcf, 0), 1); // pointer to the layer mask, which we don't support

  xcf->state = XCF_STATE_LAYER_INTERMEDIATE;

  return 1;
}

static int xcf_write_channel_header(XCF *xcf)
{
  if(xcf->state != XCF_STATE_CHANNEL)
  {
    PRINT_ERROR("error: there is no channel header to be written");
    xcf->state = XCF_STATE_ERROR;
    return 0;
  }

  // store pointer in the global channel list
  CHECK_IO(xcf, xcf_register_pointer(xcf, xcf->image.channel_list, xcf->child.n), 1);

  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.width), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.height), 1);
  CHECK_IO(xcf, xcf_write_string(xcf, xcf->child.name), 1);

  // write properties and parasites
  // opacity
  CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_OPACITY), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, 4), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, CLAMP(xcf->child.p_opacity * 255, 0, 255)), 1);
  // visible
  CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_VISIBLE), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, 4), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, xcf->child.p_visible), 1);
  // color
  CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_COLOR), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, 3), 1);
  for(int c = 0; c < 3; c++)
    CHECK_IO(xcf, xcf_write_uint8(xcf, CLAMP(xcf->child.p_color[c] * 255, 0, 255)), 1);
  if(xcf->image.version >= 4)
  {
    // float opacity
    CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_FLOAT_OPACITY), 1);
    CHECK_IO(xcf, xcf_write_uint32(xcf, 4), 1);
    CHECK_IO(xcf, xcf_write_float(xcf, xcf->child.p_opacity), 1);
    // float color
    CHECK_IO(xcf, xcf_write_uint32(xcf, XCF_PROP_FLOAT_COLOR), 1);
    CHECK_IO(xcf, xcf_write_uint32(xcf, 12), 1);
    for(int c = 0; c < 3; c++)
      CHECK_IO(xcf, xcf_write_float(xcf, xcf->child.p_color[c]), 1);
  }
  // parasites
  CHECK_IO(xcf, xcf_write_parasites(xcf, xcf->child.parasites), 1);

  // close the property list by adding PROP_END
  CHECK_IO(xcf, xcf_write_uint32(xcf, 0), 1); // type
  CHECK_IO(xcf, xcf_write_uint32(xcf, 0), 1); // size

  // the hierarchy struct comes rigth after the layer
  const uint64_t current_pos = ftell(xcf->fd);
  CHECK_IO(xcf, xcf_write_pointer(xcf, current_pos + xcf_pointer_size(xcf)), 1);

  xcf->state = XCF_STATE_CHANNEL_INTERMEDIATE;

  return 1;
}

// data_channels is the number of color channels in the data passed in
// n_channels is the number of channels that get written
// these may differ to make it easier for the user to pass in image data that he already has
// channel_size is the number of bytes per channel per pixel. for a float rgb image it is 4
static int xcf_add_hierarchy(XCF *xcf, const void *data, const uint32_t width, const uint32_t height,
                             const int data_channels, const int n_channels, const int channel_size)
{
  int res = 0;
  const uint32_t bpp = n_channels * channel_size;

  unsigned char *data_fixed = (unsigned char *)data;
  int free_data_fixed = 0;
  // make sure that the data has the right number of channels
  if(n_channels != data_channels)
  {
//     printf("channels need adaption\n");

    const size_t data_bpp = data_channels * channel_size;
    data_fixed = (unsigned char *)calloc((size_t)width * height, n_channels * channel_size);
    free_data_fixed = 1;
    if(n_channels < data_channels)
    {
      // just drop all extra channels
      for(uint32_t y = 0; y < height; y++)
        for(uint32_t x = 0; x < width; x++)
          memcpy(data_fixed + y * width * bpp + x * bpp, ((uint8_t *)data) + y * width * data_bpp + x * data_bpp, bpp);
    }
    else
    {
      // add extra channels. the last one (alpha) will be fully opaque, the others will be 0
      unsigned char *alpha_data = (unsigned char *)malloc(channel_size);
      if(xcf->image.precision == XCF_PRECISION_F_16_L || xcf->image.precision == XCF_PRECISION_F_16_G)
        *((uint16_t *)alpha_data) = 0x3c00; // 1.0 in half float
      else if(xcf->image.precision == XCF_PRECISION_F_32_L || xcf->image.precision == XCF_PRECISION_F_32_G)
        *((float *)alpha_data) = 1.0;
      else if(xcf->image.precision == XCF_PRECISION_F_64_L || xcf->image.precision == XCF_PRECISION_F_64_G)
        *((double *)alpha_data) = 1.0;
      else
        memset(alpha_data, 0xff, channel_size);

      for(uint32_t y = 0; y < height; y++)
      {
        for(uint32_t x = 0; x < width; x++)
        {
          memcpy(data_fixed + y * width * bpp + x * bpp,
                 ((unsigned char *)data) + y * width * data_bpp + x * data_bpp,
                 data_bpp);
          // make the layer opaque if it has an alpha channel
          if(n_channels == 2 || n_channels == 4)
            memcpy(data_fixed + y * width * bpp + x * bpp + (n_channels - 1) * channel_size, alpha_data, channel_size);
        }
      }
      free(alpha_data);
    }
  }

  CHECK_IO(xcf, xcf_write_uint32(xcf, width), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, height), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, bpp), 1);

  const uint64_t current_pos = ftell(xcf->fd);
  CHECK_IO(xcf, xcf_write_pointer(xcf, current_pos + xcf_pointer_size(xcf) * 2), 1);
  // we omit the dummy level list. the xcf specs encourage writing it because GIMP
  // does so, too, but at the same time says that readers shouldn't use it
  CHECK_IO(xcf, xcf_write_pointer(xcf, 0), 1);

  // add level structure
  const uint32_t n_tiles = ceilf((float)width / TILE_SIZE) * ceilf((float)height / TILE_SIZE);
  CHECK_IO(xcf, xcf_write_uint32(xcf, width), 1);
  CHECK_IO(xcf, xcf_write_uint32(xcf, height), 1);

  // links to tiles. will be filled in later
  const uint32_t tiles_list = ftell(xcf->fd);
  CHECK_IO(xcf, fseek(xcf->fd, n_tiles * xcf_pointer_size(xcf), SEEK_CUR), 0);
  CHECK_IO(xcf, xcf_write_pointer(xcf, 0), 1);

  // add tiles
  const size_t tile_size = (size_t)bpp * TILE_SIZE * TILE_SIZE;
  const size_t dest_len = compressBound(tile_size);
  unsigned char *tile_compressed = (unsigned char *)malloc(dest_len);

  void *tile = malloc(tile_size);

  for(uint32_t y = 0, tile_number = 0; y < height; y += TILE_SIZE)
  {
    const uint32_t y_end = MIN(y + TILE_SIZE - 1, height - 1);
    const uint32_t tile_h = y_end - y + 1;
    for(uint32_t x = 0; x < width; x += TILE_SIZE, tile_number++)
    {
      // put the pointer into the tile list
      const uint64_t _current_pos = ftell(xcf->fd);
      CHECK_IO(xcf, fseek(xcf->fd, tiles_list + xcf_pointer_size(xcf) * tile_number, SEEK_SET), 0);
      CHECK_IO(xcf, xcf_write_pointer(xcf, _current_pos), 1);
      CHECK_IO(xcf, fseek(xcf->fd, 0, SEEK_END), 0);

      const uint32_t x_end = MIN(x + TILE_SIZE - 1, width - 1);
      const uint32_t tile_w = x_end - x + 1;

      // channel size can be 1 (8 bit), 2 (16 bit), 4 (32 bit) or 8 (64 bit)
      if(channel_size == 1)
      {
        uint8_t *data8 = (uint8_t *)data_fixed;
        uint8_t *tile8 = (uint8_t *)tile;
        for(uint32_t tile_y = 0; tile_y < tile_h; tile_y++)
        {
          for(uint32_t tile_x = 0; tile_x < tile_w; tile_x++)
          {
            for(int c = 0; c < n_channels; c++)
            {
              const uint8_t value = data8[(y + tile_y) * n_channels * width + (x + tile_x) * n_channels + c];
              tile8[tile_y * n_channels * tile_w + tile_x * n_channels + c] = value;
            }
          }
        }
      }
      else if(channel_size == 2)
      {
        uint16_t *data16 = (uint16_t *)data_fixed;
        uint16_t *tile16 = (uint16_t *)tile;
        for(uint32_t tile_y = 0; tile_y < tile_h; tile_y++)
        {
          for(uint32_t tile_x = 0; tile_x < tile_w; tile_x++)
          {
            for(int c = 0; c < n_channels; c++)
            {
              const uint16_t value = data16[(y + tile_y) * n_channels * width + (x + tile_x) * n_channels + c];
              tile16[tile_y * n_channels * tile_w + tile_x * n_channels + c] = htobe16(value);
            }
          }
        }
      }
      else if(channel_size == 4)
      {
        uint32_t *data32 = (uint32_t *)data_fixed;
        uint32_t *tile32 = (uint32_t *)tile;
        for(uint32_t tile_y = 0; tile_y < tile_h; tile_y++)
        {
          for(uint32_t tile_x = 0; tile_x < tile_w; tile_x++)
          {
            for(int c = 0; c < n_channels; c++)
            {
              const uint32_t value = data32[(y + tile_y) * n_channels * width + (x + tile_x) * n_channels + c];
              tile32[tile_y * n_channels * tile_w + tile_x * n_channels + c] = htobe32(value);
            }
          }
        }
      }
      else if(channel_size == 8)
      {
        uint64_t *data64 = (uint64_t *)data_fixed;
        uint64_t *tile64 = (uint64_t *)tile;
        for(uint32_t tile_y = 0; tile_y < tile_h; tile_y++)
        {
          for(uint32_t tile_x = 0; tile_x < tile_w; tile_x++)
          {
            for(int c = 0; c < n_channels; c++)
            {
              const uint64_t value = data64[(y + tile_y) * n_channels * width + (x + tile_x) * n_channels + c];
              tile64[tile_y * n_channels * tile_w + tile_x * n_channels + c] = htobe64(value);
            }
          }
        }
      }
      else
      {
        PRINT_ERROR("error: channel size of %d bytes is not supported", channel_size);
        goto end;
      }

      const size_t src_len = bpp * tile_w * tile_h;
      if(xcf->image.p_compression == XCF_PROP_COMPRESSION_ZLIB)
      {
        // use zlib to compress the tile
        unsigned long _dest_len = dest_len;
        const int zlib_res = compress(tile_compressed, &_dest_len, tile, src_len);
        if(zlib_res != Z_OK)
        {
          PRINT_ERROR("error: can't compress tile: %d", zlib_res);
          goto end;
        }
        if(fwrite(tile_compressed, 1, _dest_len, xcf->fd) != _dest_len)
        {
          PRINT_ERROR("error: can't write image data");
          goto end;
        }
      }
      else if(xcf->image.p_compression == XCF_PROP_COMPRESSION_NONE)
      {
        if(fwrite(tile, 1, src_len, xcf->fd) != src_len)
        {
          PRINT_ERROR("error: can't write image data");
          goto end;
        }
      }
    }
  }

  res = 1;

  end:
  free(tile);
  free(tile_compressed);
  if(free_data_fixed) free(data_fixed);
  if(!res) xcf->state = XCF_STATE_ERROR;
  return res;
}


// public api

XCF *xcf_open(const char *filename)
{
  XCF *xcf = (XCF *)calloc(1, sizeof(XCF));
  if(!xcf) return NULL;

  if(!(xcf->fd = fopen(filename, "wb")))
  {
    free(xcf);
    return NULL;
  }

  xcf->state = XCF_STATE_IMAGE;
  xcf->image.p_compression = XCF_PROP_COMPRESSION_ZLIB;
  xcf->min_version = 1;
  xcf->image.version = 12;
  xcf->omit_base_alpha = 1; // don't save an alpha channel in the base layer by default

  return xcf;
}

int xcf_close(XCF *xcf)
{
  if(!xcf) return 1;

  if(xcf->state == XCF_STATE_ERROR)
  {
    PRINT_ERROR("error: the file is in error state. better add some error handling.");
    return 0;
  }

  int res = 1;

  if(xcf->state == XCF_STATE_IMAGE)
    xcf_write_image_header(xcf);

  if(xcf->state != XCF_STATE_MAIN)
  {
    PRINT_ERROR("error: incomplete data written");
    res = 0;
  }

  if(xcf->n_layers != xcf->next_layer || xcf->n_channels != xcf->next_channel)
  {
    PRINT_ERROR("error: not all layers/channels were added. %u / %u layers and %u / %u channels written",
                xcf->next_layer, xcf->n_layers, xcf->next_channel, xcf->n_channels);
    res = 0;
  }

//   printf("version: %d\nmin_version: %d\npointer size: %d\nbase_type: %u\nprecision: %u\nwidth: %u\nheight: %u\nlayers: %u\nchannels: %u\n", xcf->image.version, xcf->min_version, xcf_pointer_size(xcf), xcf->image.base_type, xcf->image.precision, xcf->image.width, xcf->image.height, xcf->next_layer, xcf->next_channel);

  if(xcf->fd) fclose(xcf->fd);
  xcf->fd = NULL;
  free(xcf->child.name);
  xcf->child.name = NULL;
  xcf_parasites_free(xcf->image.parasites);
  xcf->image.parasites = NULL;
  xcf_parasites_free(xcf->child.parasites);
  xcf->child.parasites = NULL;
  xcf->state = XCF_STATE_ERROR; // just in case someone keeps using the memory
  free(xcf);

  return res;
}

// set fields or properties. depending on the current state it's setting image, layer or channel data
int xcf_set(XCF *xcf, xcf_field_t field, ...)
{
  if(xcf->state == XCF_STATE_ERROR)
  {
    PRINT_ERROR("error: the file is in error state. better add some error handling.");
    return 0;
  }

  va_list ap;
  va_start(ap, field);

  int res = 0;
  uint32_t propid = -1;

  // depending on the state of writing we are in, only some properties can be set
  // global image level
  if(xcf->state == XCF_STATE_IMAGE)
  {
    res = 1;
    switch(field)
    {
      case XCF_N_LAYERS:        xcf->n_layers = va_arg(ap, uint32_t);                break;
      case XCF_N_CHANNELS:      xcf->n_channels = va_arg(ap, uint32_t);              break;
      case XCF_OMIT_BASE_ALPHA: xcf->omit_base_alpha = va_arg(ap, uint32_t) ? 1 : 0; break;
      case XCF_VERSION:         xcf->image.version = va_arg(ap, int);                break;
      case XCF_BASE_TYPE:       xcf->image.base_type = va_arg(ap, xcf_base_type_t);  break;
      case XCF_WIDTH:           xcf->image.width = va_arg(ap, uint32_t);             break;
      case XCF_HEIGHT:          xcf->image.height = va_arg(ap, uint32_t);            break;
      case XCF_PRECISION:       xcf->image.precision = va_arg(ap, xcf_precision_t);  break;
      case XCF_PROP:
      {
        propid = va_arg(ap, uint32_t);
        switch(propid)
        {
          case XCF_PROP_END:
            // FIXME: shall we offer this at all?
            break;
          // TODO
          // case XCF_PROP_COLORMAP:
          // break;
          case XCF_PROP_COMPRESSION:
            xcf->image.p_compression = va_arg(ap, int);
            break;
          case XCF_PROP_PARASITES:
          {
            const char *name = va_arg(ap, char *);
            const uint32_t flags = va_arg(ap, uint32_t);
            const uint32_t length = va_arg(ap, uint32_t);
            const uint8_t *data = va_arg(ap, uint8_t *);
            xcf->image.parasites = xcf_parasites_add(xcf->image.parasites, name, flags, length, data);
            break;
          }
          default: res = 0;
        }
        break;
      }
      default: res = 0;
    }
  }

  // layer level
  if(xcf->state == XCF_STATE_LAYER)
  {
    res = 1;
    switch(field)
    {
      case XCF_WIDTH:  xcf->child.width = va_arg(ap, uint32_t);      break;
      case XCF_HEIGHT: xcf->child.height = va_arg(ap, uint32_t);     break;
      case XCF_NAME:   xcf->child.name = strdup(va_arg(ap, char *)); break;
      case XCF_PROP:
      {
        propid = va_arg(ap, uint32_t);
        switch(propid)
        {
          case XCF_PROP_END:
            // FIXME: shall we offer this at all?
            break;
          case XCF_PROP_OPACITY:
            xcf->child.p_opacity = va_arg(ap, uint32_t) / 255.0;
            xcf->child.p_opacity = CLAMP(xcf->child.p_opacity, 0.0, 1.0);
            break;
          case XCF_PROP_MODE:
            xcf->child.p_mode = va_arg(ap, uint32_t);
            break;
          case XCF_PROP_VISIBLE:
            xcf->child.p_visible = va_arg(ap, uint32_t) ? 1 : 0;
            break;
          case XCF_PROP_OFFSETS:
            xcf->child.p_offset_x = va_arg(ap, int32_t);
            xcf->child.p_offset_y = va_arg(ap, int32_t);
            break;
          case XCF_PROP_PARASITES:
          {
            const char *name = va_arg(ap, char *);
            const uint32_t flags = va_arg(ap, uint32_t);
            const uint32_t length = va_arg(ap, uint32_t);
            const uint8_t *data = va_arg(ap, uint8_t *);
            xcf->child.parasites = xcf_parasites_add(xcf->child.parasites, name, flags, length, data);
            break;
          }
          case XCF_PROP_FLOAT_OPACITY:
            xcf->child.p_opacity = va_arg(ap, double);
            xcf->child.p_opacity = CLAMP(xcf->child.p_opacity, 0.0, 1.0);
            break;
          case XCF_PROP_COMPOSITE_MODE:
            xcf->child.p_composite_mode = va_arg(ap, int32_t);
            break;
          case XCF_PROP_COMPOSITE_SPACE:
            xcf->child.p_composite_space = va_arg(ap, int32_t);
            break;
          case XCF_PROP_BLEND_SPACE:
            xcf->child.p_blend_space = va_arg(ap, int32_t);
            break;
          default: res = 0;
        }
        break;
      }
      default: res = 0;
    }
  }

  // channel level
  if(xcf->state == XCF_STATE_CHANNEL)
  {
    res = 1;
    switch(field)
    {
      // width and height have to be the same as in the parent, no need to allow setting it
      // case XCF_WIDTH:  xcf->child.width = va_arg(ap, uint32_t);   break;
      // case XCF_HEIGHT: xcf->child.height = va_arg(ap, uint32_t);  break;
      case XCF_NAME:   xcf->child.name = strdup(va_arg(ap, char *)); break;
      case XCF_PROP:
      {
        propid = va_arg(ap, uint32_t);
        switch(propid)
        {
          case XCF_PROP_OPACITY:
            xcf->child.p_opacity = va_arg(ap, uint32_t) / 255.0;
            xcf->child.p_opacity = CLAMP(xcf->child.p_opacity, 0.0, 1.0);
            break;
          case XCF_PROP_VISIBLE:
            xcf->child.p_visible = va_arg(ap, uint32_t) ? 1 : 0;
            break;
          case XCF_PROP_COLOR:
            for(int c = 0; c < 3; c++)
            {
              const float color = va_arg(ap, uint32_t) / 255.0;
              xcf->child.p_color[c] = CLAMP(color, 0.0, 1.0);
            }
            break;
          case XCF_PROP_PARASITES:
          {
            const char *name = va_arg(ap, char *);
            const uint32_t flags = va_arg(ap, uint32_t);
            const uint32_t length = va_arg(ap, uint32_t);
            const uint8_t *data = va_arg(ap, uint8_t *);
            xcf->child.parasites = xcf_parasites_add(xcf->child.parasites, name, flags, length, data);
            break;
          }
          case XCF_PROP_FLOAT_OPACITY:
            xcf->child.p_opacity = va_arg(ap, double);
            xcf->child.p_opacity = CLAMP(xcf->child.p_opacity, 0.0, 1.0);
            break;
          case XCF_PROP_FLOAT_COLOR:
            for(int c = 0; c < 3; c++)
            {
              const float color = va_arg(ap, double);
              xcf->child.p_color[c] = CLAMP(color, 0.0, 1.0);
            }
            break;
          default: res = 0;
        }
        break;
      }
      default: res = 0;
    }
  }

  va_end(ap);

  if(!res)
  {
    if(field != XCF_PROP)
    {
      const char *field_name = xcf_get_field_name(field);
      const char *state_name = xcf_get_state_name(xcf->state);
      if(field_name)
      {
        if(state_name)
          PRINT_ERROR("error: can't set '%s' in state '%s'", field_name, state_name);
        else
          PRINT_ERROR("error: can't set '%s' in state %d", field_name, xcf->state);
      }
      else
      {
        if(state_name)
          PRINT_ERROR("error: can't set %d in state '%s'", field, state_name);
        else
          PRINT_ERROR("error: can't set %d in state %d", field, xcf->state);
      }
    }
    else
    {
      const char *prop_name = xcf_get_property_name(propid);
      const char *state_name = xcf_get_state_name(xcf->state);
      if(prop_name)
      {
        if(state_name)
          PRINT_ERROR("error: can't set property '%s' in state '%s'", prop_name, state_name);
        else
          PRINT_ERROR("error: can't set property '%s' in state %d", prop_name, xcf->state);
      }
      else
      {
        if(state_name)
          PRINT_ERROR("error: can't set property %d in state '%s'", propid, state_name);
        else
          PRINT_ERROR("error: can't set property %d in state %d", propid, xcf->state);
      }
    }
    xcf->state = XCF_STATE_ERROR;
  }
  return res;
}

int xcf_add_layer(XCF *xcf)
{
  if(xcf->state == XCF_STATE_ERROR)
  {
    PRINT_ERROR("error: the file is in error state. better add some error handling.");
    return 0;
  }

  if(xcf->state == XCF_STATE_IMAGE)
    xcf_write_image_header(xcf);

  if(xcf->state != XCF_STATE_MAIN)
  {
    PRINT_ERROR("error: can't add a layer while already adding something");
    xcf->state = XCF_STATE_ERROR;
    return 0;
  }

  if(xcf->next_layer >= xcf->n_layers)
  {
    PRINT_ERROR("error: too many layers added, expecting only %d", xcf->n_layers);
    xcf->state = XCF_STATE_ERROR;
    return 0;
  }

  xcf->state = XCF_STATE_LAYER;

  free(xcf->child.name);
  xcf_parasites_free(xcf->child.parasites);
  memset(&xcf->child, 0, sizeof(xcf->child));
  xcf->child.n = xcf->next_layer;
  xcf->next_layer++;

  // set some defaults for the properties
  xcf->child.p_opacity = 1.0;
  xcf->child.p_visible = 1;

  xcf->child.p_composite_mode = -1;
  xcf->child.p_composite_space = -1;
  xcf->child.p_blend_space = -1;
  xcf->child.p_mode = -1; // -1 is either XCF_PROP_MODE_LEGACY_NORMAL or XCF_PROP_MODE_NORMAL
  xcf->child.p_offset_x = 0;
  xcf->child.p_offset_y = 0;

  return 1;
}

// TODO: handle layer masks
int xcf_add_channel(XCF *xcf)
{
  if(xcf->state == XCF_STATE_ERROR)
  {
    PRINT_ERROR("error: the file is in error state. better add some error handling.");
    return 0;
  }

  if(xcf->state == XCF_STATE_IMAGE)
    xcf_write_image_header(xcf);

  if(xcf->state != XCF_STATE_MAIN)
  {
    PRINT_ERROR("error: can't add a channel while already adding something");
    xcf->state = XCF_STATE_ERROR;
    return 0;
  }

  if(xcf->next_channel >= xcf->n_channels)
  {
    PRINT_ERROR("error: too many channels added, expecting only %d", xcf->n_channels);
    xcf->state = XCF_STATE_ERROR;
    return 0;
  }

  xcf->state = XCF_STATE_CHANNEL;

  free(xcf->child.name);
  xcf_parasites_free(xcf->child.parasites);
  memset(&xcf->child, 0, sizeof(xcf->child));
  xcf->child.n = xcf->next_channel;
  xcf->next_channel++;

  // channels are always grayscale, i.e., single channel (how confusing, having two concepts called "channel")
  xcf->child.type = XCF_TYPE_GRAYSCALE;

  // for channels the size has to be identical to the parent
  xcf->child.width = xcf->image.width;
  xcf->child.height = xcf->image.height;

  // set some defaults for the properties
  xcf->child.p_opacity = 1.0;
  xcf->child.p_visible = 1;

  xcf->child.p_color[0] = 0.0;
  xcf->child.p_color[1] = 0.0;
  xcf->child.p_color[2] = 0.0;

  return 1;
}

int xcf_add_data(XCF *xcf, const void *data, const int data_channels)
{
  if(xcf->state == XCF_STATE_ERROR)
  {
    PRINT_ERROR("error: the file is in error state. better add some error handling.");
    return 0;
  }

  int res = 0;

  if(xcf->state == XCF_STATE_LAYER)
    res = xcf_write_layer_header(xcf);
  else if(xcf->state == XCF_STATE_CHANNEL)
    res = xcf_write_channel_header(xcf);

  if(!res)
  {
    PRINT_ERROR("error: no open layer or channel to add data to");
    xcf->state = XCF_STATE_ERROR;
    return 0;
  }

  // add hierarchy structure
  int n_channels = 0;
  switch(xcf->child.type)
  {
    case XCF_TYPE_RGB:             n_channels = 3; break;
    case XCF_TYPE_RGB_ALPHA:       n_channels = 4; break;
    case XCF_TYPE_GRAYSCALE:       n_channels = 1; break;
    case XCF_TYPE_GRAYSCALE_ALPHA: n_channels = 2; break;
    case XCF_TYPE_INDEXED:         n_channels = 1; break;
    case XCF_TYPE_INDEXED_ALPHA:   n_channels = 2; break;
  }
  int channel_size = 0;
  switch(xcf->image.precision)
  {
    case XCF_PRECISION_I_8_L:
    case XCF_PRECISION_I_8_G:
      channel_size = 1;
      break;
    case XCF_PRECISION_I_16_L:
    case XCF_PRECISION_I_16_G:
    case XCF_PRECISION_F_16_L:
    case XCF_PRECISION_F_16_G:
      channel_size = 2;
      break;
    case XCF_PRECISION_I_32_L:
    case XCF_PRECISION_I_32_G:
    case XCF_PRECISION_F_32_L:
    case XCF_PRECISION_F_32_G:
      channel_size = 4;
      break;
    case XCF_PRECISION_F_64_L:
    case XCF_PRECISION_F_64_G:
      channel_size = 8;
      break;
  }

  res = xcf_add_hierarchy(xcf, data, xcf->child.width, xcf->child.height, data_channels, n_channels, channel_size);

  if(res) xcf->state = XCF_STATE_MAIN;

  return res;
}
