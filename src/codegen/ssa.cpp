struct ssaModule;
struct ssaProcedure;
struct ssaBlock;
struct ssaValue;


struct ssaModule {
	CheckerInfo *info;
	BaseTypeSizes sizes;
	gbAllocator allocator;

	Map<ssaValue *> values;  // Key: Entity *
	Map<ssaValue *> members; // Key: String
	i32 global_string_index;
};


struct ssaBlock {
	i32 id;
	AstNode *node;
	Scope *scope;
	String label;
	ssaProcedure *parent;

	gbArray(ssaValue *) instructions;
	gbArray(ssaValue *) values;
};

struct ssaProcedure {
	ssaModule *module;
	String name;
	Entity *entity;
	Type *type;
	DeclarationInfo *decl;
	AstNode *type_expr;
	AstNode *body;

	gbArray(ssaValue *) blocks;
	ssaBlock *curr_block;
	gbArray(ssaValue *) anonymous_procedures;
};



struct ssaTypeName {
	Entity *entity;
	Type *type;
};
struct ssaGlobal {
	b32 generated;
	Entity *entity;
	Type *type;
	ssaValue *value;
};
struct ssaConstant {
	Type *type;
	ExactValue value;
};



enum ssaInstructionKind {
	ssaInstruction_Invalid,

	ssaInstruction_Local,
	ssaInstruction_Store,
	ssaInstruction_Load,
	ssaInstruction_GetElementPtr,

	ssaInstruction_Convert,

	ssaInstruction_BinaryOp,

	ssaInstruction_Count,
};

struct ssaInstruction {
	ssaInstructionKind kind;

	ssaBlock *parent;
	Type *type;
	TokenPos pos;

	union {
		struct {
			Entity *entity;
			Type *type;
		} local;
		struct {
			ssaValue *address;
			ssaValue *value;
		} store;
		struct {
			ssaValue *address;
		} load;
		struct {
			ssaValue *address;
			Type *result_type;
			Type *element_type;
			isize index_count;
			isize indices[2];
		} get_element_ptr;



		struct {
			Token op;
			ssaValue *left, *right;
		} binary_op;
	};
};


enum ssaValueKind {
	ssaValue_Invalid,

	ssaValue_TypeName,
	ssaValue_Global,
	ssaValue_Procedure,
	ssaValue_Constant,

	ssaValue_Block,
	ssaValue_Instruction,

	ssaValue_Count,
};

struct ssaValue {
	ssaValueKind kind;
	i32 id;

	union {
		ssaTypeName    type_name;
		ssaGlobal      global;
		ssaProcedure   procedure;
		ssaConstant    constant;
		ssaBlock       block;
		ssaInstruction instruction;
	};
};

enum ssaLvalueKind {
	ssaLvalue_Blank,
	ssaLvalue_Address,

	ssaLvalue_Count,
};

struct ssaLvalue {
	ssaLvalueKind kind;
	union {
		struct {} blank;
		struct {
			ssaValue *value;
			AstNode *expr;
		} address;
	};
};

void ssa_module_init(ssaModule *m, Checker *c) {
	m->allocator = gb_heap_allocator();
	m->info = &c->info;
	m->sizes = c->sizes;

	map_init(&m->values,  m->allocator);
	map_init(&m->members, m->allocator);
}

void ssa_module_destroy(ssaModule *m) {
	map_destroy(&m->values);
	map_destroy(&m->members);
}

void ssa_module_add_value(ssaModule *m, Entity *e, ssaValue *v) {
	map_set(&m->values, hash_pointer(e), v);
}


Type *ssa_value_type(ssaValue *value);
void  ssa_value_set_type(ssaValue *value, Type *type);

Type *ssa_instruction_type(ssaInstruction *instr) {
	switch (instr->kind) {
	case ssaInstruction_Local:
		return instr->local.type;
	case ssaInstruction_Store:
		return ssa_value_type(instr->store.address);
	case ssaInstruction_Load:
		return ssa_value_type(instr->load.address);
	}
	return NULL;
}

void ssa_instruction_set_type(ssaInstruction *instr, Type *type) {
	switch (instr->kind) {
	case ssaInstruction_Local:
		instr->local.type = type;
		break;
	case ssaInstruction_Store:
		ssa_value_set_type(instr->store.value, type);
		break;
	case ssaInstruction_Load:
		// NOTE(bill): Do nothing
		break;
	}
}

Type *ssa_value_type(ssaValue *value) {
	switch (value->kind) {
	case ssaValue_TypeName:
		return value->type_name.type;
	case ssaValue_Global:
		return value->global.type;
	case ssaValue_Procedure:
		return value->procedure.type;
	case ssaValue_Constant:
		return value->constant.type;
	case ssaValue_Instruction:
		return ssa_instruction_type(&value->instruction);
	}
	return NULL;
}


void ssa_value_set_type(ssaValue *value, Type *type) {
	switch (value->kind) {
	case ssaValue_TypeName:
		value->type_name.type = type;
		break;
	case ssaValue_Global:
		value->global.type = type;
		break;
	case ssaValue_Procedure:
		value->procedure.type = type;
		break;
	case ssaValue_Constant:
		value->constant.type = type;
		break;
	case ssaValue_Instruction:
		ssa_instruction_set_type(&value->instruction, type);
		break;
	}
}



ssaValue *ssa_build_expression(ssaProcedure *proc, AstNode *expr);
ssaValue *ssa_build_single_expression(ssaProcedure *proc, AstNode *expr, TypeAndValue *tv);
ssaLvalue ssa_build_address(ssaProcedure *proc, AstNode *expr);
ssaValue *ssa_emit_conversion(ssaProcedure *proc, ssaValue *value, Type *a_type);






ssaValue *ssa_alloc_value(gbAllocator a, ssaValueKind kind) {
	ssaValue *v = gb_alloc_item(a, ssaValue);
	v->kind = kind;
	return v;
}

ssaValue *ssa_alloc_instruction(gbAllocator a, ssaInstructionKind kind) {
	ssaValue *v = ssa_alloc_value(a, ssaValue_Instruction);
	v->instruction.kind = kind;
	return v;
}

ssaValue *ssa_make_value_type_name(gbAllocator a, Entity *e) {
	ssaValue *v = ssa_alloc_value(a, ssaValue_TypeName);
	v->type_name.entity = e;
	v->type_name.type = e->type;
	return v;
}

ssaValue *ssa_make_value_global(gbAllocator a, Entity *e, ssaValue *value) {
	ssaValue *v = ssa_alloc_value(a, ssaValue_Global);
	v->global.entity = e;
	v->global.type = e->type;
	v->global.value = value;
	return v;
}



ssaValue *ssa_make_instruction_local(ssaProcedure *p, Entity *e) {
	ssaValue *v = ssa_alloc_instruction(p->module->allocator, ssaInstruction_Local);
	ssaInstruction *i = &v->instruction;
	i->local.entity = e;
	i->local.type = e->type;
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	ssa_module_add_value(p->module, e, v);
	return v;
}


ssaValue *ssa_make_instruction_store(ssaProcedure *p, ssaValue *address, ssaValue *value) {
	ssaValue *v = ssa_alloc_instruction(p->module->allocator, ssaInstruction_Store);
	ssaInstruction *i = &v->instruction;
	i->store.address = address;
	i->store.value = value;
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	return v;
}

ssaValue *ssa_make_instruction_load(ssaProcedure *p, ssaValue *address) {
	ssaValue *v = ssa_alloc_instruction(p->module->allocator, ssaInstruction_Load);
	ssaInstruction *i = &v->instruction;
	i->load.address = address;
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	return v;
}

ssaValue *ssa_make_instruction_get_element_ptr(ssaProcedure *p, ssaValue *address) {
	ssaValue *v = ssa_alloc_instruction(p->module->allocator, ssaInstruction_GetElementPtr);
	ssaInstruction *i = &v->instruction;
	i->get_element_ptr.address = address;
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	return v;
}

ssaValue *ssa_make_instruction_binary_op(ssaProcedure *p, Token op, ssaValue *left, ssaValue *right) {
	ssaValue *v = ssa_alloc_instruction(p->module->allocator, ssaInstruction_BinaryOp);
	ssaInstruction *i = &v->instruction;
	i->binary_op.op = op;
	i->binary_op.left = left;
	i->binary_op.right = right;
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	return v;
}


ssaValue *ssa_make_value_constant(gbAllocator a, Type *type, ExactValue value) {
	ssaValue *v = ssa_alloc_value(a, ssaValue_Constant);
	v->constant.type  = type;
	v->constant.value = value;
	return v;
}

ssaValue *ssa_make_value_procedure(gbAllocator a, Entity *e, DeclarationInfo *decl, ssaModule *m) {
	ssaValue *v = ssa_alloc_value(a, ssaValue_Procedure);
	v->procedure.module = m;
	v->procedure.entity = e;
	v->procedure.type   = e->type;
	v->procedure.decl   = decl;
	v->procedure.name   = e->token.string;
	return v;
}

ssaValue *ssa_make_value_block(gbAllocator a, ssaProcedure *proc, AstNode *node, Scope *scope, String label) {
	ssaValue *v = ssa_alloc_value(a, ssaValue_Block);
	v->block.label  = label;
	v->block.node   = node;
	v->block.scope  = scope;
	v->block.parent = proc;

	gb_array_init(v->block.instructions, gb_heap_allocator());
	gb_array_init(v->block.values,       gb_heap_allocator());

	return v;
}



ssaValue *ssa_add_global_string(ssaProcedure *proc, ExactValue value) {
	GB_ASSERT(value.kind == ExactValue_String);
	gbAllocator a = gb_heap_allocator();


	isize max_len = 4+8+1;
	u8 *str = cast(u8 *)gb_alloc_array(a, u8, max_len);
	isize len = gb_snprintf(cast(char *)str, max_len, ".str%x", proc->module->global_string_index);
	proc->module->global_string_index++;

	String name = make_string(str, len-1);
	Token token = {Token_String};
	token.string = name;
	Type *type = &basic_types[Basic_string];
	Entity *entity = make_entity_constant(a, NULL, token, type, value);
	ssaValue *v = ssa_make_value_constant(a, type, value);


	ssaValue *g = ssa_make_value_global(a, entity, v);
	g->global.generated = true;

	map_set(&proc->module->values, hash_pointer(entity), g);
	map_set(&proc->module->members, hash_string(name), g);

	return g;
}


ssaValue *ssa_add_block(ssaProcedure *proc, AstNode *node, String label) {
	gbAllocator a = proc->module->allocator;
	Scope *scope = NULL;
	Scope **found = map_get(&proc->module->info->scopes, hash_pointer(node));
	if (found) scope = *found;
	ssaValue *block = ssa_make_value_block(a, proc, node, scope, label);
	gb_array_append(proc->blocks, block);
	return block;
}



void ssa_begin_procedure_body(ssaProcedure *proc) {
	gb_array_init(proc->blocks, gb_heap_allocator());
	ssaValue *b = ssa_add_block(proc, proc->body, make_string("entry"));
	proc->curr_block = &b->block;
}


void ssa_end_procedure_body(ssaProcedure *proc) {

// Number registers
	i32 reg_id = 0;
	gb_for_array(i, proc->blocks) {
		ssaBlock *b = &proc->blocks[i]->block;
		gb_for_array(j, b->instructions) {
			ssaValue *value = b->instructions[j];
			ssaInstruction *instr = &value->instruction;
			if (instr->kind == ssaInstruction_Store) {
				continue;
			}
			value->id = reg_id;
			reg_id++;
		}
	}
}


b32 ssa_is_blank_identifier(AstNode *i) {
	GB_ASSERT(i->kind == AstNode_Identifier);
	return are_strings_equal(i->identifier.token.string, make_string("_"));
}


ssaValue *ssa_block_emit(ssaBlock *b, ssaValue *instr) {
	instr->instruction.parent = b;
	gb_array_append(b->instructions, instr);
	return instr;
}

ssaValue *ssa_emit(ssaProcedure *proc, ssaValue *instr) {
	return ssa_block_emit(proc->curr_block, instr);
}


ssaValue *ssa_add_local(ssaProcedure *proc, Entity *e) {
	ssaValue *instr = ssa_make_instruction_local(proc, e);
	ssa_emit(proc, instr);
	return instr;
}

ssaValue *ssa_add_local_for_identifier(ssaProcedure *proc, AstNode *name) {
	Entity **found = map_get(&proc->module->info->definitions, hash_pointer(name));
	if (found) {
		return ssa_add_local(proc, *found);
	}
	return NULL;
}


ssaValue *ssa_emit_store(ssaProcedure *p, ssaValue *address, ssaValue *value) {
	ssaValue *store = ssa_make_instruction_store(p, address, value);
	ssa_emit(p, store);
	return store;
}

ssaValue *ssa_emit_load(ssaProcedure *p, ssaValue *address) {
	ssaValue *v = ssa_make_instruction_load(p, address);
	Type *t = ssa_value_type(address);
	ssa_value_set_type(v, type_deref(t));
	ssa_emit(p, v);
	return v;
}

ssaValue *ssa_lvalue_store(ssaLvalue lval, ssaProcedure *p, ssaValue *value) {
	switch (lval.kind) {
	case ssaLvalue_Address:
		return ssa_emit_store(p, lval.address.value, value);
	}
	GB_PANIC("Illegal lvalue store");
	return NULL;
}

ssaValue *ssa_lvalue_load(ssaLvalue lval, ssaProcedure *p) {
	switch (lval.kind) {
	case ssaLvalue_Address:
		return ssa_emit_load(p, lval.address.value);
	}
	GB_PANIC("Illegal lvalue load");
	return NULL;
}


ssaValue *ssa_lvalue_address(ssaLvalue lval, ssaProcedure *p) {
	switch (lval.kind) {
	case ssaLvalue_Address:
		return lval.address.value;
	}
	return NULL;
}

Type *ssa_lvalue_type(ssaLvalue lval) {
	switch (lval.kind) {
	case ssaLvalue_Address:
		return type_deref(ssa_value_type(lval.address.value));
	}
	return NULL;
}

ssaValue *ssa_emit_conversion(ssaProcedure *proc, ssaValue *value, Type *t) {
	Type *src_type = ssa_value_type(value);
	if (are_types_identical(t, src_type))
		return value;

	Type *dst = get_base_type(t);
	Type *src = get_base_type(src_type);

	if (value->kind == ssaValue_Constant) {
		if (dst->kind == Type_Basic)
			return ssa_make_value_constant(proc->module->allocator, t, value->constant.value);
	}


	GB_PANIC("TODO(bill): ssa_emit_conversion");

	return NULL;
}

ssaValue *ssa_emit_arith(ssaProcedure *proc, Token op, ssaValue *left, ssaValue *right, Type *type) {
	switch (op.kind) {
	case Token_Add:
	case Token_Sub:
	case Token_Mul:
	case Token_Quo:
	case Token_Mod:
	case Token_And:
	case Token_Or:
	case Token_Xor:
	case Token_AndNot:
		left  = ssa_emit_conversion(proc, left, type);
		right = ssa_emit_conversion(proc, right, type);
		break;
	}

	ssaValue *v = ssa_make_instruction_binary_op(proc, op, left, right);
	return ssa_emit(proc, v);
}

ssaValue *ssa_build_single_expression(ssaProcedure *proc, AstNode *expr, TypeAndValue *tv) {
	switch (expr->kind) {
	case AstNode_Identifier: {
		Entity *e = *map_get(&proc->module->info->uses, hash_pointer(expr));
		if (e->kind == Entity_Builtin) {
			// TODO(bill): Entity_Builtin
			return NULL;
		}

		auto *found = map_get(&proc->module->values, hash_pointer(e));
		if (found) {
			return ssa_emit_load(proc, *found);
		}
	} break;

	case AstNode_DereferenceExpression: {
		ssaLvalue addr = ssa_build_address(proc, expr->dereference_expression.operand);
		return ssa_lvalue_load(addr, proc);
	} break;

	case AstNode_UnaryExpression: {
		auto *ue = &expr->unary_expression;
		switch (ue->op.kind) {
		case Token_Pointer:
			return ssa_lvalue_address(ssa_build_address(proc, ue->operand), proc);
		case Token_Add:
			return ssa_build_expression(proc, ue->operand);
		case Token_Sub: {
			// NOTE(bill): -x == 0 - x
			ExactValue zero = make_exact_value_integer(0);
			ssaValue *left = ssa_make_value_constant(proc->module->allocator, tv->type, zero);
			ssaValue *right = ssa_build_expression(proc, ue->operand);
			return ssa_emit_arith(proc, ue->op, left, right, tv->type);
		} break;
		case Token_Xor: { // Bitwise not
			// NOTE(bill): "not" x == x "xor" -1
			ExactValue neg_one = make_exact_value_integer(-1);
			ssaValue *left = ssa_build_expression(proc, ue->operand);
			ssaValue *right = ssa_make_value_constant(proc->module->allocator, tv->type, neg_one);
			return ssa_emit_arith(proc, ue->op, left, right, tv->type);
		} break;
		case Token_Not: // Boolean not
			GB_PANIC("Token_Not");
			return NULL;

		}
	} break;

	case AstNode_BinaryExpression: {
		auto *be = &expr->binary_expression;
		switch (be->op.kind) {
		case Token_Add:
		case Token_Sub:
		case Token_Mul:
		case Token_Quo:
		case Token_Mod:
		case Token_And:
		case Token_Or:
		case Token_Xor:
		case Token_AndNot:
			return ssa_emit_arith(proc, be->op,
			                      ssa_build_expression(proc, be->left),
			                      ssa_build_expression(proc, be->right),
			                      tv->type);
		}
	} break;
	case AstNode_ProcedureLiteral:
		break;
	case AstNode_CastExpression:
		break;
	case AstNode_CallExpression:
		break;
	case AstNode_SliceExpression:
		break;
	case AstNode_IndexExpression: {

	} break;
	case AstNode_SelectorExpression:
		break;
	}

	GB_PANIC("Unexpected expression");
	return NULL;
}


ssaValue *ssa_build_expression(ssaProcedure *proc, AstNode *expr) {
	expr = unparen_expression(expr);

	TypeAndValue *tv = map_get(&proc->module->info->types, hash_pointer(expr));
	if (tv) {
		if (tv->value.kind != ExactValue_Invalid) {
			if (tv->value.kind == ExactValue_String) {
				ssaValue *global_str = ssa_add_global_string(proc, tv->value);
				return ssa_emit_load(proc, global_str);
			}
			return ssa_make_value_constant(proc->module->allocator, tv->type, tv->value);
		}

		ssaValue *value = NULL;
		if (tv->mode == Addressing_Variable) {
			gb_printf("!Addressable!\n");
			// TODO(bill): Addressing_Variable
		} else {
			value = ssa_build_single_expression(proc, expr, tv);
		}

		return value;
	}
	return NULL;
}


ssaLvalue ssa_build_address(ssaProcedure *proc, AstNode *expr) {
	switch (expr->kind) {
	case AstNode_Identifier: {
		if (!ssa_is_blank_identifier(expr)) {
			Entity *e = entity_of_identifier(proc->module->info, expr);

			ssaLvalue val = {ssaLvalue_Address};
			val.address.expr = expr;
			ssaValue **found = map_get(&proc->module->values, hash_pointer(e));
			if (found) {
				val.address.value = *found;
			}
			return val;
		}
	} break;

	case AstNode_ParenExpression:
		return ssa_build_address(proc, unparen_expression(expr));

	case AstNode_DereferenceExpression: {
		ssaLvalue val = {ssaLvalue_Address};
		AstNode *operand = expr->dereference_expression.operand;
		val.address.value = ssa_build_expression(proc, operand);
		val.address.expr  = expr;
		return val;
	} break;

	case AstNode_SelectorExpression:
		break;

	case AstNode_IndexExpression:
		break;

	// TODO(bill): Others address
	}

	GB_PANIC("Unexpected address expression");

	ssaLvalue blank = {ssaLvalue_Blank};
	return blank;
}

void ssa_build_assign_op(ssaProcedure *proc, ssaLvalue lhs, ssaValue *value, Token op) {
	ssaValue *old_value = ssa_lvalue_load(lhs, proc);
	ssaValue *change = ssa_emit_conversion(proc, value, ssa_value_type(old_value));
	ssaValue *new_value = ssa_emit_arith(proc, op, old_value, change, ssa_lvalue_type(lhs));
	ssa_lvalue_store(lhs, proc, new_value);
}


void ssa_build_statement(ssaProcedure *proc, AstNode *s);

void ssa_build_statement_list(ssaProcedure *proc, AstNode *list) {
	for (AstNode *stmt = list ; stmt != NULL; stmt = stmt->next)
		ssa_build_statement(proc, stmt);
}

void ssa_build_statement(ssaProcedure *proc, AstNode *s) {
	switch (s->kind) {
	case AstNode_EmptyStatement:
		break;
	case AstNode_VariableDeclaration: {
		auto *vd = &s->variable_declaration;
		if (vd->kind == Declaration_Mutable) {
			if (vd->name_count == vd->value_count) { // 1:1 assigment
				gbArray(ssaLvalue)  lvals;
				gbArray(ssaValue *) inits;
				gb_array_init_reserve(lvals, gb_heap_allocator(), vd->name_count);
				gb_array_init_reserve(inits, gb_heap_allocator(), vd->name_count);
				defer (gb_array_free(lvals));
				defer (gb_array_free(inits));

				for (AstNode *name = vd->name_list; name != NULL; name = name->next) {
					ssaLvalue lval = {ssaLvalue_Blank};
					if (!ssa_is_blank_identifier(name)) {
						ssa_add_local_for_identifier(proc, name);
						lval = ssa_build_address(proc, name);
					}

					gb_array_append(lvals, lval);
				}

				for (AstNode *value = vd->value_list; value != NULL; value = value->next) {
					ssaValue *init = ssa_build_expression(proc, value);
					gb_array_append(inits, init);
				}


				gb_for_array(i, inits) {
					ssa_lvalue_store(lvals[i], proc, inits[i]);
				}

			} else if (vd->value_count == 0) { // declared and zero-initialized
				for (AstNode *name = vd->name_list; name != NULL; name = name->next) {
					if (!ssa_is_blank_identifier(name)) {
						// TODO(bill): add local
						ssa_add_local_for_identifier(proc, name);
					}
				}
			} else { // Tuple(s)
				GB_PANIC("TODO(bill): tuple assignment variable declaration");
			}
		}
	} break;

	case AstNode_IncDecStatement: {
		Token op = s->inc_dec_statement.op;
		if (op.kind == Token_Increment) {
			op.kind = Token_Add;
		} else if (op.kind == Token_Decrement) {
			op.kind = Token_Sub;
		}
		ssaLvalue lval = ssa_build_address(proc, s->inc_dec_statement.expression);
		ssaValue *one = ssa_make_value_constant(proc->module->allocator, ssa_lvalue_type(lval),
		                                        make_exact_value_integer(1));
		ssa_build_assign_op(proc, lval, one, op);

	} break;

	case AstNode_AssignStatement: {
		auto *assign = &s->assign_statement;
		switch (assign->op.kind) {
		case Token_Eq: {
			gbArray(ssaLvalue) lvals;
			gb_array_init(lvals, gb_heap_allocator());
			defer (gb_array_free(lvals));

			for (AstNode *lhs = assign->lhs_list;
			     lhs != NULL;
			     lhs = lhs->next) {
				ssaLvalue lval = {};
				if (!ssa_is_blank_identifier(lhs)) {
					lval = ssa_build_address(proc, lhs);
				}
				gb_array_append(lvals, lval);
			}

			if (assign->lhs_count == assign->rhs_count) {
				if (assign->lhs_count == 1) {
					AstNode *lhs = assign->lhs_list;
					AstNode *rhs = assign->rhs_list;
					ssaValue *init = ssa_build_expression(proc, rhs);
					ssa_lvalue_store(lvals[0], proc, init);
				} else {
					gbArray(ssaValue *) inits;
					gb_array_init_reserve(inits, gb_heap_allocator(), gb_array_count(lvals));
					defer (gb_array_free(inits));

					for (AstNode *rhs = assign->rhs_list; rhs != NULL; rhs = rhs->next) {
						ssaValue *init = ssa_build_expression(proc, rhs);
						gb_array_append(inits, init);
					}

					gb_for_array(i, inits) {
						ssa_lvalue_store(lvals[i], proc, inits[i]);
					}
				}
			} else {
				GB_PANIC("TODO(bill): tuple assignment");
			}

		} break;

		default: {
			// NOTE(bill): Only 1 += 1 is allowed, no tuples
			// +=, -=, etc
			Token op = assign->op;
			i32 kind = op.kind;
			kind += Token_Add - Token_AddEq; // Convert += to +
			op.kind = cast(TokenKind)kind;
			ssaLvalue lhs = ssa_build_address(proc, assign->lhs_list);
			ssaValue *value = ssa_build_expression(proc, assign->rhs_list);
			ssa_build_assign_op(proc, lhs, value, op);
		} break;
		}
	} break;

	case AstNode_ExpressionStatement:
		ssa_build_expression(proc, s->expression_statement.expression);
		break;

	case AstNode_BlockStatement:
		ssa_build_statement_list(proc, s->block_statement.list);
		break;

	case AstNode_IfStatement:
		GB_PANIC("AstNode_IfStatement");
		break;
	case AstNode_ReturnStatement:
		GB_PANIC("AstNode_ReturnStatement");
		break;
	case AstNode_ForStatement:
		GB_PANIC("AstNode_ForStatement");
		break;
	case AstNode_DeferStatement:
		GB_PANIC("AstNode_DeferStatement");
		break;
	case AstNode_BranchStatement:
		GB_PANIC("AstNode_BranchStatement");
		break;
	}
}



void ssa_build_procedure(ssaValue *value) {
	ssaProcedure *proc = &value->procedure;

	// gb_printf("Building %.*s: %.*s\n", LIT(entity_strings[proc->entity->kind]), LIT(proc->name));


	AstNode *proc_decl = proc->decl->proc_decl;
	switch (proc_decl->kind) {
	case AstNode_ProcedureDeclaration:
		proc->type_expr = proc_decl->procedure_declaration.type;
		proc->body = proc_decl->procedure_declaration.body;
		break;
	default:
		return;
	}

	if (proc->body == NULL) {
		// TODO(bill): External procedure
		return;
	}


	ssa_begin_procedure_body(proc);
	ssa_build_statement(proc, proc->body);
	ssa_end_procedure_body(proc);

}










