#pragma once

#ifndef XCF_INTERNAL_INCLUDES
#error only "xcf.h" should be included diretly
#endif

#include "xcf.h"

const char *xcf_get_base_type_name(xcf_base_type_t type);
const char *xcf_get_type_name(xcf_type_t type);
const char *xcf_get_precision_name(xcf_precision_t precision);
const char *xcf_get_property_name(xcf_props_t property);
const char *xcf_get_compression_name(xcf_prop_compression_t compression);
const char *xcf_get_composite_mode_name(xcf_prop_composite_mode_t mode);
const char *xcf_get_composite_blend_space_name(xcf_prop_composite_blend_space_t blend_space);
const char *xcf_get_mode_name(xcf_prop_mode_t mode);
const char *xcf_get_field_name(xcf_field_t field);
const char *xcf_get_state_name(xcf_state_t state);
