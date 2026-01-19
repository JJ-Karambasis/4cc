
internal i64*
jj_get_indentation_array(Application_Links *app, Arena *arena, Buffer_ID buffer, Range_i64 lines, Indent_Flag flags, i32 tab_width, i32 indent_width){
    ProfileScope(app, "get indentation array");
    i64 count = lines.max - lines.min + 1;
    i64 *indentations = push_array(arena, i64, count);
    i64 *shifted_indentations = indentations - lines.first;
    block_fill_u64(indentations, sizeof(*indentations)*count, (u64)(-1));
    
	#if 0
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Token_Array *tokens = scope_attachment(app, scope, attachment_tokens, Token_Array);
	#endif
    
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    Token_Array *tokens = &token_array;

	i64 scope_start_line = 1;
	i64 scope_line = 1;

	Token *scope_token = 0;

	if (tokens->tokens != 0) {

		Token *token = tokens->tokens;

		u64 scope_counter = 0;
		u64 paren_counter = 0;
		for (;;) {
			if (scope_line > lines.first)
				break;

			i64 function_end_line_pos = get_line_end_pos(app, buffer, scope_line);
			for (;;token += 1){
				if (token->pos + token->size > function_end_line_pos){
					break;
				}

				if (scope_counter == 0 && paren_counter == 0) {
					scope_token = token;
					scope_start_line = scope_line;
				}

				switch (token->kind){
					case TokenBaseKind_ScopeOpen:
					{
						scope_counter += 1;
					}break;
					case TokenBaseKind_ScopeClose:
					{
						paren_counter = 0;
						if (scope_counter > 0){
							scope_counter -= 1;
						}
					}break;
					case TokenBaseKind_ParentheticalOpen:
					{
						paren_counter += 1;
					}break;
					case TokenBaseKind_ParentheticalClose:
					{
						if (paren_counter > 0){
							paren_counter -= 1;
						}
					}break;
				}
			}

			scope_line++;
		}
	}

	i64 anchor_line = scope_start_line;
    Token *anchor_token = find_anchor_token(app, buffer, tokens, anchor_line);
    if (anchor_token != 0 &&
        anchor_token >= tokens->tokens &&
        anchor_token < tokens->tokens + tokens->count){
        i64 line = get_line_number_from_pos(app, buffer, anchor_token->pos);
        line = clamp_top(line, lines.first);
        
        Token_Iterator_Array token_it = token_iterator(0, tokens, anchor_token);
        
        Scratch_Block scratch(app, arena);
        Nest *nest = 0;
        Nest_Alloc nest_alloc = {};
        
        i64 line_last_indented = line - 1;
        i64 last_indent = 0;
        i64 actual_indent = 0;
        b32 in_unfinished_statement = false;
        
        Indent_Line_Cache line_cache = {};
        
        for (;;){
            Token *token = token_it_read(&token_it);
            
            if (line_cache.where_token_starts == 0 ||
                token->pos >= line_cache.one_past_last_pos){
                ProfileScope(app, "get line number");
                line_cache.where_token_starts = get_line_number_from_pos(app, buffer, token->pos);
                line_cache.one_past_last_pos = get_line_end_pos(app, buffer, line_cache.where_token_starts);
            }
            
            i64 current_indent = 0;
            if (nest != 0){
                current_indent = nest->indent;
            }
            i64 this_indent = current_indent;
            i64 following_indent = current_indent;
            
            b32 shift_by_actual_indent = false;
            b32 ignore_unfinished_statement = false;
            if (HasFlag(token->flags, TokenBaseFlag_PreprocessorBody)){
                this_indent = 0;
            }
            else{
                switch (token->kind){
                    case TokenBaseKind_ScopeOpen:
                    {
						if (nest && nest->kind == TokenBaseKind_ParentheticalOpen) {
							this_indent = nest->next ? nest->next->indent : 0;
							current_indent = this_indent;
							following_indent = this_indent;
						}

                        Nest *new_nest = indent__new_nest(arena, &nest_alloc);
                        sll_stack_push(nest, new_nest);
                        nest->kind = TokenBaseKind_ScopeOpen;
                        nest->indent = current_indent + indent_width;
                        following_indent = nest->indent;
                        ignore_unfinished_statement = true;
                    }break;
                    
                    case TokenBaseKind_ScopeClose:
                    {
                        for (;nest != 0 && nest->kind != TokenBaseKind_ScopeOpen;){
                            Nest *n = nest;
                            sll_stack_pop(nest);
                            indent__free_nest(&nest_alloc, n);
                        }
                        if (nest != 0 && nest->kind == TokenBaseKind_ScopeOpen){
                            Nest *n = nest;
                            sll_stack_pop(nest);
                            indent__free_nest(&nest_alloc, n);
                        }
                        this_indent = 0;

                        if (nest != 0){
                            this_indent = nest->indent;

							if (nest->kind == TokenBaseKind_ParentheticalOpen) {
								this_indent = nest->next ? nest->next->indent : 0;
							}
                        }
                        following_indent = this_indent;
                        ignore_unfinished_statement = true;
                    }break;
                    
                    case TokenBaseKind_ParentheticalOpen:
                    {
                        Nest *new_nest = indent__new_nest(arena, &nest_alloc);
                        sll_stack_push(nest, new_nest);
                        nest->kind = TokenBaseKind_ParentheticalOpen;
                        line_indent_cache_update(app, buffer, tab_width, &line_cache);
                        nest->indent = (token->pos - line_cache.indent_info.first_char_pos) + 1;
                        following_indent = nest->indent;
                        shift_by_actual_indent = true;
                        ignore_unfinished_statement = true;
                    }break;
                    
                    case TokenBaseKind_ParentheticalClose:
                    {
                        if (nest != 0 && nest->kind == TokenBaseKind_ParentheticalOpen){
                            Nest *n = nest;
                            sll_stack_pop(nest);
                            indent__free_nest(&nest_alloc, n);
                        }
                        following_indent = 0;
                        if (nest != 0){
                            following_indent = nest->indent;
                        }
                        //ignore_unfinished_statement = true;
                    }break;
                }
                
                if (token->sub_kind == TokenCppKind_BlockComment ||
                    token->sub_kind == TokenCppKind_LiteralStringRaw){
                    ignore_unfinished_statement = true;
                }
                
                if (in_unfinished_statement && !ignore_unfinished_statement){
                    this_indent += indent_width;
                }
            }
            
			#define EMIT(N) \
			Stmnt(if (lines.first <= line_it){shifted_indentations[line_it]=N;} \
				if (line_it == lines.end){goto finished;} \
					actual_indent = N; )
            
				i64 line_it = line_last_indented;
            if (lines.first <= line_cache.where_token_starts){
                for (;line_it < line_cache.where_token_starts;){
                    line_it += 1;
                    if (line_it == line_cache.where_token_starts){
                        EMIT(this_indent);
                    }
                    else{
                        EMIT(last_indent);
                    }
                }
            }
            else{
                actual_indent = this_indent;
                line_it = line_cache.where_token_starts;
            }
            
            i64 line_where_token_ends = get_line_number_from_pos(app, buffer, token->pos + token->size);
            if (lines.first <= line_where_token_ends){
                line_indent_cache_update(app, buffer, tab_width, &line_cache);
                i64 line_where_token_starts_shift = this_indent - line_cache.indent_info.indent_pos;
                for (;line_it < line_where_token_ends;){
                    line_it += 1;
                    i64 line_it_start_pos = get_line_start_pos(app, buffer, line_it);
                    Indent_Info line_it_indent_info = get_indent_info_line_number_and_start(app, buffer, line_it, line_it_start_pos, tab_width);
                    i64 new_indent = line_it_indent_info.indent_pos + line_where_token_starts_shift;
                    new_indent = clamp_bot(0, new_indent);
                    EMIT(new_indent);
                }
            }
            else{
                line_it = line_where_token_ends;
            }
			#undef EMIT
            
            if (shift_by_actual_indent){
                nest->indent += actual_indent;
                following_indent += actual_indent;
            }
            
            if (token->kind != TokenBaseKind_Comment){
                in_unfinished_statement = indent__unfinished_statement(token, nest);
                if (in_unfinished_statement){
                    following_indent += indent_width;
                }
            }
            
            last_indent = following_indent;
            line_last_indented = line_it;
            
            if (!token_it_inc_non_whitespace(&token_it)){
                break;
            }
        }
    }
    
    finished:;
    return(indentations);
}