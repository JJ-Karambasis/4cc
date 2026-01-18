function ARGB_Color get_color_from_table(Color_Table table, Managed_ID id, int subindex) {
    ARGB_Color result = 0;
    FColor color = fcolor_id(id);
    if (color.a_byte == 0){
        if (color.id != 0){
            result = finalize_color(table, color.id, subindex);
        }
    }
    else{
        result = color.argb;
    }

    return result;
}

function ARGB_Color get_color_from_table(Color_Table table, Managed_ID id) {
    return get_color_from_table(table, id, 0);
}