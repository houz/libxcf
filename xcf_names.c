#define XCF_INTERNAL_INCLUDES
#include "xcf_names.h"

#include <stddef.h>

#define STR(s) #s

// don't use default: so we get a compiler warning when we miss something

const char *xcf_get_base_type_name(xcf_base_type_t type)
{
  switch(type)
  {
    case XCF_BASE_TYPE_RGB:       return STR(XCF_BASE_TYPE_RGB);
    case XCF_BASE_TYPE_GRAYSCALE: return STR(XCF_BASE_TYPE_GRAYSCALE);
    case XCF_BASE_TYPE_INDEXED:   return STR(XCF_BASE_TYPE_INDEXED);
  }

  return NULL;
}

const char *xcf_get_type_name(xcf_type_t type)
{
  switch(type)
  {
    case XCF_TYPE_RGB:             return STR(XCF_TYPE_RGB);
    case XCF_TYPE_RGB_ALPHA:       return STR(XCF_TYPE_RGB_ALPHA);
    case XCF_TYPE_GRAYSCALE:       return STR(XCF_TYPE_GRAYSCALE);
    case XCF_TYPE_GRAYSCALE_ALPHA: return STR(XCF_TYPE_GRAYSCALE_ALPHA);
    case XCF_TYPE_INDEXED:         return STR(XCF_TYPE_INDEXED);
    case XCF_TYPE_INDEXED_ALPHA:   return STR(XCF_TYPE_INDEXED_ALPHA);
  }

  return NULL;
}

const char *xcf_get_precision_name(xcf_precision_t precision)
{
  switch(precision)
  {
    case XCF_PRECISION_I_8_L:  return STR(XCF_PRECISION_I_8_L);
    case XCF_PRECISION_I_8_G:  return STR(XCF_PRECISION_I_8_G);
    case XCF_PRECISION_I_16_L: return STR(XCF_PRECISION_I_16_L);
    case XCF_PRECISION_I_16_G: return STR(XCF_PRECISION_I_16_G);
    case XCF_PRECISION_I_32_L: return STR(XCF_PRECISION_I_32_L);
    case XCF_PRECISION_I_32_G: return STR(XCF_PRECISION_I_32_G);
    case XCF_PRECISION_F_16_L: return STR(XCF_PRECISION_F_16_L);
    case XCF_PRECISION_F_16_G: return STR(XCF_PRECISION_F_16_G);
    case XCF_PRECISION_F_32_L: return STR(XCF_PRECISION_F_32_L);
    case XCF_PRECISION_F_32_G: return STR(XCF_PRECISION_F_32_G);
    case XCF_PRECISION_F_64_L: return STR(XCF_PRECISION_F_64_L);
    case XCF_PRECISION_F_64_G: return STR(XCF_PRECISION_F_64_G);
  }

  return NULL;
}

const char *xcf_get_property_name(xcf_props_t property)
{
  switch(property)
  {
    case XCF_PROP_END:             return STR(XCF_PROP_END);
    case XCF_PROP_COLORMAP:        return STR(XCF_PROP_COLORMAP);
    case XCF_PROP_OPACITY:         return STR(XCF_PROP_OPACITY);
    case XCF_PROP_MODE:            return STR(XCF_PROP_MODE);
    case XCF_PROP_VISIBLE:         return STR(XCF_PROP_VISIBLE);
    case XCF_PROP_OFFSETS:         return STR(XCF_PROP_OFFSETS);
    case XCF_PROP_COLOR:           return STR(XCF_PROP_COLOR);
    case XCF_PROP_COMPRESSION:     return STR(XCF_PROP_COMPRESSION);
    case XCF_PROP_PARASITES:       return STR(XCF_PROP_PARASITES);
    case XCF_PROP_FLOAT_OPACITY:   return STR(XCF_PROP_FLOAT_OPACITY);
    case XCF_PROP_COMPOSITE_MODE:  return STR(XCF_PROP_COMPOSITE_MODE);
    case XCF_PROP_COMPOSITE_SPACE: return STR(XCF_PROP_COMPOSITE_SPACE);
    case XCF_PROP_BLEND_SPACE:     return STR(XCF_PROP_BLEND_SPACE);
    case XCF_PROP_FLOAT_COLOR:     return STR(XCF_PROP_FLOAT_COLOR);
  }

  return NULL;
}

const char *xcf_get_compression_name(xcf_prop_compression_t compression)
{
  switch(compression)
  {
    case XCF_PROP_COMPRESSION_NONE: return STR(XCF_PROP_COMPRESSION_NONE);
    case XCF_PROP_COMPRESSION_RLE:  return STR(XCF_PROP_COMPRESSION_RLE);
    case XCF_PROP_COMPRESSION_ZLIB: return STR(XCF_PROP_COMPRESSION_ZLIB);
  }

  return NULL;
}

const char *xcf_get_composite_mode_name(xcf_prop_composite_mode_t mode)
{
  switch(mode)
  {
    case XCF_PROP_COMPOSITE_MODE_UNION:            return STR(XCF_PROP_COMPOSITE_MODE_UNION);
    case XCF_PROP_COMPOSITE_MODE_CLIP_TO_BACKDROP: return STR(XCF_PROP_COMPOSITE_MODE_CLIP_TO_BACKDROP);
    case XCF_PROP_COMPOSITE_MODE_CLIP_TO_LAYER:    return STR(XCF_PROP_COMPOSITE_MODE_CLIP_TO_LAYER);
    case XCF_PROP_COMPOSITE_MODE_INTERSECTION:     return STR(XCF_PROP_COMPOSITE_MODE_INTERSECTION);
  }

  return NULL;
}

const char *xcf_get_composite_blend_space_name(xcf_prop_composite_blend_space_t blend_space)
{
  switch(blend_space)
  {
    case XCF_PROP_COMPOSITE_BLEND_SPACE_RGB_L: return STR(XCF_PROP_COMPOSITE_BLEND_SPACE_RGB_L);
    case XCF_PROP_COMPOSITE_BLEND_SPACE_RGB_P: return STR(XCF_PROP_COMPOSITE_BLEND_SPACE_RGB_P);
    case XCF_PROP_COMPOSITE_BLEND_SPACE_LAB:   return STR(XCF_PROP_COMPOSITE_BLEND_SPACE_LAB);
  }

  return NULL;
}

const char *xcf_get_mode_name(xcf_prop_mode_t mode)
{
  switch(mode)
  {
    case XCF_PROP_MODE_LEGACY_NORMAL:         return STR(XCF_PROP_MODE_LEGACY_NORMAL);
    case XCF_PROP_MODE_LEGACY_DISSOLVE:       return STR(XCF_PROP_MODE_LEGACY_DISSOLVE);
    case XCF_PROP_MODE_LEGACY_BEHIND:         return STR(XCF_PROP_MODE_LEGACY_BEHIND);
    case XCF_PROP_MODE_LEGACY_MULTIPLY:       return STR(XCF_PROP_MODE_LEGACY_MULTIPLY);
    case XCF_PROP_MODE_LEGACY_SCREEN:         return STR(XCF_PROP_MODE_LEGACY_SCREEN);
    case XCF_PROP_MODE_LEGACY_OVERLAY:        return STR(XCF_PROP_MODE_LEGACY_OVERLAY);
    case XCF_PROP_MODE_LEGACY_DIFFERENCE:     return STR(XCF_PROP_MODE_LEGACY_DIFFERENCE);
    case XCF_PROP_MODE_LEGACY_ADDITION:       return STR(XCF_PROP_MODE_LEGACY_ADDITION);
    case XCF_PROP_MODE_LEGACY_SUBTRACT:       return STR(XCF_PROP_MODE_LEGACY_SUBTRACT);
    case XCF_PROP_MODE_LEGACY_DARKEN:         return STR(XCF_PROP_MODE_LEGACY_DARKEN);
    case XCF_PROP_MODE_LEGACY_LIGHTEN:        return STR(XCF_PROP_MODE_LEGACY_LIGHTEN);
    case XCF_PROP_MODE_LEGACY_HUE_HSV:        return STR(XCF_PROP_MODE_LEGACY_HUE_HSV);
    case XCF_PROP_MODE_LEGACY_SATURATION_HSV: return STR(XCF_PROP_MODE_LEGACY_SATURATION_HSV);
    case XCF_PROP_MODE_LEGACY_COLOR_HSL:      return STR(XCF_PROP_MODE_LEGACY_COLOR_HSL);
    case XCF_PROP_MODE_LEGACY_VALUE_HSV:      return STR(XCF_PROP_MODE_LEGACY_VALUE_HSV);
    case XCF_PROP_MODE_LEGACY_DIVIDE:         return STR(XCF_PROP_MODE_LEGACY_DIVIDE);
    case XCF_PROP_MODE_LEGACY_DODGE:          return STR(XCF_PROP_MODE_LEGACY_DODGE);
    case XCF_PROP_MODE_LEGACY_BURN:           return STR(XCF_PROP_MODE_LEGACY_BURN);
    case XCF_PROP_MODE_LEGACY_HARD_LIGHT:     return STR(XCF_PROP_MODE_LEGACY_HARD_LIGHT);
    case XCF_PROP_MODE_LEGACY_SOFT_LIGHT:     return STR(XCF_PROP_MODE_LEGACY_SOFT_LIGHT);
    case XCF_PROP_MODE_LEGACY_GRAIN_EXTRACT:  return STR(XCF_PROP_MODE_LEGACY_GRAIN_EXTRACT);
    case XCF_PROP_MODE_LEGACY_GRAIN_MERGE:    return STR(XCF_PROP_MODE_LEGACY_GRAIN_MERGE);
    case XCF_PROP_MODE_LEGACY_COLOR_ERASE:    return STR(XCF_PROP_MODE_LEGACY_COLOR_ERASE);
    case XCF_PROP_MODE_OVERLAY:               return STR(XCF_PROP_MODE_OVERLAY);
    case XCF_PROP_MODE_HUE_LCH:               return STR(XCF_PROP_MODE_HUE_LCH);
    case XCF_PROP_MODE_CHROMA_LCH:            return STR(XCF_PROP_MODE_CHROMA_LCH);
    case XCF_PROP_MODE_COLOR_LCH:             return STR(XCF_PROP_MODE_COLOR_LCH);
    case XCF_PROP_MODE_LIGHTNESS_LCH:         return STR(XCF_PROP_MODE_LIGHTNESS_LCH);
    case XCF_PROP_MODE_NORMAL:                return STR(XCF_PROP_MODE_NORMAL);
    case XCF_PROP_MODE_BEHIND:                return STR(XCF_PROP_MODE_BEHIND);
    case XCF_PROP_MODE_MULTIPLY:              return STR(XCF_PROP_MODE_MULTIPLY);
    case XCF_PROP_MODE_SCREEN:                return STR(XCF_PROP_MODE_SCREEN);
    case XCF_PROP_MODE_DIFFERENCE:            return STR(XCF_PROP_MODE_DIFFERENCE);
    case XCF_PROP_MODE_ADDITION:              return STR(XCF_PROP_MODE_ADDITION);
    case XCF_PROP_MODE_SUBTRACT:              return STR(XCF_PROP_MODE_SUBTRACT);
    case XCF_PROP_MODE_DARKEN:                return STR(XCF_PROP_MODE_DARKEN);
    case XCF_PROP_MODE_LIGHTEN:               return STR(XCF_PROP_MODE_LIGHTEN);
    case XCF_PROP_MODE_HUE_HSV:               return STR(XCF_PROP_MODE_HUE_HSV);
    case XCF_PROP_MODE_SATURATION_HSV:        return STR(XCF_PROP_MODE_SATURATION_HSV);
    case XCF_PROP_MODE_COLOR_HSL:             return STR(XCF_PROP_MODE_COLOR_HSL);
    case XCF_PROP_MODE_VALUE_HSV:             return STR(XCF_PROP_MODE_VALUE_HSV);
    case XCF_PROP_MODE_DIVIDE:                return STR(XCF_PROP_MODE_DIVIDE);
    case XCF_PROP_MODE_DODGE:                 return STR(XCF_PROP_MODE_DODGE);
    case XCF_PROP_MODE_BURN:                  return STR(XCF_PROP_MODE_BURN);
    case XCF_PROP_MODE_HARD_LIGHT:            return STR(XCF_PROP_MODE_HARD_LIGHT);
    case XCF_PROP_MODE_SOFT_LIGHT:            return STR(XCF_PROP_MODE_SOFT_LIGHT);
    case XCF_PROP_MODE_GRAIN_EXTRACT:         return STR(XCF_PROP_MODE_GRAIN_EXTRACT);
    case XCF_PROP_MODE_GRAIN_MERGE:           return STR(XCF_PROP_MODE_GRAIN_MERGE);
    case XCF_PROP_MODE_VIVID_LIGHT:           return STR(XCF_PROP_MODE_VIVID_LIGHT);
    case XCF_PROP_MODE_PIN_LIGHT:             return STR(XCF_PROP_MODE_PIN_LIGHT);
    case XCF_PROP_MODE_LINEAR_LIGHT:          return STR(XCF_PROP_MODE_LINEAR_LIGHT);
    case XCF_PROP_MODE_HARD_MIX:              return STR(XCF_PROP_MODE_HARD_MIX);
    case XCF_PROP_MODE_EXCLUSION:             return STR(XCF_PROP_MODE_EXCLUSION);
    case XCF_PROP_MODE_LINEAR_BURN:           return STR(XCF_PROP_MODE_LINEAR_BURN);
    case XCF_PROP_MODE_L_DARKEN:              return STR(XCF_PROP_MODE_L_DARKEN);
    case XCF_PROP_MODE_L_LIGHTEN:             return STR(XCF_PROP_MODE_L_LIGHTEN);
    case XCF_PROP_MODE_LUMINANCE:             return STR(XCF_PROP_MODE_LUMINANCE);
    case XCF_PROP_MODE_COLOR_ERASE:           return STR(XCF_PROP_MODE_COLOR_ERASE);
    case XCF_PROP_MODE_ERASE:                 return STR(XCF_PROP_MODE_ERASE);
    case XCF_PROP_MODE_MERGE:                 return STR(XCF_PROP_MODE_MERGE);
    case XCF_PROP_MODE_SPLIT:                 return STR(XCF_PROP_MODE_SPLIT);
    case XCF_PROP_MODE_PASS_THROUGH:          return STR(XCF_PROP_MODE_PASS_THROUGH);
  }

  return NULL;
}

const char *xcf_get_field_name(xcf_field_t field)
{
  switch(field)
  {
    case XCF_WIDTH:           return STR(XCF_WIDTH);
    case XCF_HEIGHT:          return STR(XCF_HEIGHT);
    case XCF_PROP:            return STR(XCF_PROP);
    case XCF_NAME:            return STR(XCF_NAME);
    case XCF_VERSION:         return STR(XCF_VERSION);
    case XCF_BASE_TYPE:       return STR(XCF_BASE_TYPE);
    case XCF_PRECISION:       return STR(XCF_PRECISION);
    case XCF_N_LAYERS:        return STR(XCF_N_LAYERS);
    case XCF_N_CHANNELS:      return STR(XCF_N_CHANNELS);
    // case XCF_TYPE:         return STR(XCF_TYPE);
    case XCF_OMIT_BASE_ALPHA: return STR(XCF_OMIT_BASE_ALPHA);
  }

  return NULL;
}

const char *xcf_get_state_name(xcf_state_t state)
{
  switch(state)
  {
    case XCF_STATE_IMAGE:                return STR(XCF_STATE_IMAGE);
    case XCF_STATE_MAIN:                 return STR(XCF_STATE_MAIN);
    case XCF_STATE_LAYER:                return STR(XCF_STATE_LAYER);
    case XCF_STATE_LAYER_INTERMEDIATE:   return STR(XCF_STATE_LAYER_INTERMEDIATE);
    case XCF_STATE_CHANNEL:              return STR(XCF_STATE_CHANNEL);
    case XCF_STATE_CHANNEL_INTERMEDIATE: return STR(XCF_STATE_CHANNEL_INTERMEDIATE);
    case XCF_STATE_DONE:                 return STR(XCF_STATE_DONE);
    case XCF_STATE_ERROR:                return STR(XCF_STATE_ERROR);
  }

  return NULL;
}

#undef STR
