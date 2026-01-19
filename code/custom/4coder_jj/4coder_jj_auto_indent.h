#ifndef FCODER_JJ_AUTO_INDENT_H
#define FCODER_JJ_AUTO_INDENT_H


internal i64*
jj_get_indentation_array(Application_Links *app, Arena *arena, Buffer_ID buffer, Range_i64 lines, Indent_Flag flags, i32 tab_width, i32 indent_width);

#endif