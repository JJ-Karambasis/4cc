#ifndef FCODER_JJ_CUSTOM_H
#define FCODER_JJ_CUSTOM_H

#include "4coder_jj_colors.h"
#include "4coder_jj_auto_indent.h"

struct fcoder_jj_custom {
	ARGB_Color modal_margin_active_color;
	ARGB_Color normal_margin_active_color;
	b32 is_mark_set;
};

global fcoder_jj_custom G_jj_custom;
function fcoder_jj_custom* get_jj_custom() {
	return &G_jj_custom;
}

#endif