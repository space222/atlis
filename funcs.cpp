#include <vector>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <unordered_map>
#include <algorithm>
#include "types.h"
#include "funcs.h"

lptr global_T;
lptr QUOTE;
lptr lisp_out_stream;
lptr lisp_in_stream;

std::unordered_map<std::string, symbol*> symbols_by_name;
fscope first_fscope;
thread_local fscope* global_scope = &first_fscope;

lptr apply(const MultiArg& args)
{
	if( args.size() == 0 ) return lptr();

	if( args[0].type() != LTYPE_SYM )
	{
		//todo: error out
		return lptr();
	}

	lptr val = symbol_value(global_scope, args[0]);
	if( val.type() != LTYPE_FUNC )
	{
		//todo: error out
		return lptr();
	}

	func* F = val.as_func();

	if( args.size() == 1 )
	{
		// having no arguments makes things easier
		if( F->ptr )
		{
			if( F->num_args == 0 )
				return ( (zero_arg_func*)(F->ptr) ) ();
			else
				return ( (multiarg_func*)(F->ptr) )({});
		}

		// running a lisp function with no args
		return begin_new_env_c(F->body);
	}

	std::vector<lptr> applargs(args.size()-1);
	for(int i = 1; i < args.size(); ++i) applargs[i-1] = args[i];

	// if it isn't a special form, need to eval the args	
	if( ! (F->flags & LFUNC_SPECIAL) )
	{
		for_each(std::begin(applargs), std::end(applargs), [&](lptr &a) { a = eval({a, global_scope}); });
	}

	//todo: check expected arg number, eventually types as well
	// if the native pointer exists, must use that
	if( F->ptr )
	{
		if( F->num_args == 1 )
		{
			return ((one_arg_func*)(F->ptr))(applargs[0]);
		} else {
			return ((multiarg_func*)(F->ptr))(applargs);
		}
	}

	// now we're really out in the grapes implementing a fully S-expression function with arguments
	fscope* envnew = new fscope(global_scope);
	global_scope = envnew;

	//todo: set up arguments in the global_scope

	// run begin on the body
	lptr retval = begin_c(F->body);

	// restore previous global_scope
	global_scope = global_scope->parent;
	delete envnew; // for now, but might eventually implement call/cc

	return retval;
}

lptr eval(const MultiArg& args)
{
	if( args.size() == 0 || args[0].nilp() ) return lptr();
	lptr i = args[0];
	fscope* env = global_scope;
	if( args.size() > 1 ) env = args[1].env();

	if( i == global_T ) return global_T;

	if( i.type() == LTYPE_SYM )
	{
		return symbol_value(env, i);
	}
	if( i.type() != LTYPE_CONS ) 
	{
		return i;
	}

	if( args[0] == QUOTE )
	{
		if( args.size() > 1 ) return args[1];
		return lptr();
	}

	std::vector<lptr> applargs;

	do {
		applargs.push_back(i.as_cons()->a);
		i = i.as_cons()->b;
	} while( i.type() == LTYPE_CONS );

	
	fscope* temp = global_scope;
	global_scope = env;
	lptr retval = apply(applargs);
	global_scope = temp;
	return retval;
}

lptr lreturn(lptr arg)
{
	global_scope->need_return = true;
	global_scope->retval = arg;
	return arg;
}

lptr begin_new_env_c(lptr arg)
{
	fscope* env = new fscope(global_scope);
	global_scope = env;

	lptr retval = begin_c(arg);

	global_scope = global_scope->parent;
	delete env;
	return retval;
}

lptr begin_c(lptr arg)
{
	if( arg.type() != LTYPE_CONS ) return lptr();

	lptr res;
	do {
		cons* temp = arg.as_cons();
		res = eval({temp->a});
		if( temp->b.type() != LTYPE_CONS ) break;
		arg = temp->b;
	} while( !arg.nilp() && !global_scope->need_return );

	return global_scope->need_return ? global_scope->retval : res;
}

lptr begin_new_env(const MultiArg& arg)
{
	fscope* env = new fscope(global_scope);
	global_scope = env;

	lptr retval = begin(arg);

	global_scope = global_scope->parent;
	delete env;
	return retval;
}

lptr begin(const MultiArg& args)
{
	if( args.size() == 0 ) return lptr();

	lptr res;
	for(int i = 0; i < args.size() && !global_scope->need_return; ++i)
	{
		res = eval({args[i]});
	}

	return global_scope->need_return ? global_scope->retval : res;
}

lptr intern_c(const std::string& name)
{
	std::string temp;
	for_each(std::begin(name), std::end(name), [&](char c) { temp += toupper(c); });
	if( temp == "NIL" ) return lptr();
	
	auto iter = symbols_by_name.find(temp);
	if( iter != symbols_by_name.end() )
	{
		return iter->second;
	}
	
	symbol* sym = new symbol(temp);
	symbols_by_name.insert(std::make_pair(temp, sym));
	return sym;
}

lptr intern(lptr str)
{
	return lptr();
}

lptr symbol_value(fscope* env, lptr s)
{
	if( s.type() != LTYPE_SYM )
		return lptr();

	// function scope
	auto iter = std::find_if(env->symbols.rbegin(), env->symbols.rend(), [&](const auto& p) { return p.first == s.sym(); });
	if( iter != env->symbols.rend() )
	{
		return iter->second;
	}

	// global scope
	auto iter2 = std::find_if(first_fscope.symbols.rbegin(), first_fscope.symbols.rend(), [&](const auto& p) { return p.first == s.sym(); });
	if( iter2 != first_fscope.symbols.rend() )
	{
		return iter2->second;
	}

	return lptr();
}

lptr ldefine(const MultiArg& args)
{
	if( args.size() < 1 ) return lptr();

	lptr sym = args[0];
	lptr val;

	if( args.size() > 1 )
	{
		val = eval({args[1]});
	}

	auto iter2 = std::find_if(first_fscope.symbols.begin(), first_fscope.symbols.end(), [&](const auto& p) { return p.first == sym.sym(); });
	if( iter2 != first_fscope.symbols.end() )
	{
		iter2->second = val;
	} else {
		first_fscope.symbols.push_back(std::make_pair(sym.sym(), val));
	}

	return val;
}

lptr setf(const MultiArg& args)
{
	if( args.size() < 2 ) return lptr();
	lptr sym = args[0];
	if( sym.type() != LTYPE_SYM ) return lptr();
	
	// function scope
	auto iter = std::find_if(global_scope->symbols.rbegin(), global_scope->symbols.rend(), [&](const auto& p) { return p.first == sym.sym(); });
	if( iter != global_scope->symbols.rend() )
	{
		lptr val = eval({args[1]});
		iter->second = val;
		return val;
	}

	// global scope
	auto iter2 = std::find_if(first_fscope.symbols.rbegin(), first_fscope.symbols.rend(), [&](const auto& p) { return p.first == sym.sym(); });
	if( iter2 != first_fscope.symbols.rend() )
	{
		lptr val = eval({args[1]});
		iter2->second = val;
		return val;
	}

	//todo: symbol not found, error out
	return lptr();
}

lptr car(lptr v)
{
	if( v.type() != LTYPE_CONS )
	{
		//todo: error out
		return lptr();
	}

	return v.as_cons()->a;
}

lptr cdr(lptr v)
{
	if( v.type() != LTYPE_CONS )
	{
		//todo: error out
		return lptr();
	}

	return v.as_cons()->b;
}

lptr lcons(const MultiArg& args)
{
	if( args.size() < 2 )
	{
		//todo: error out
		return lptr();
	}
	return new cons(args[0], args[1]);
}

lptr lquote(lptr a)
{
	return a;
}

lptr lexit(lptr a)
{
	exit(a.as_int());
	return a; // not really
}

float to_float_c(lptr a)
{
	switch( a.type() )
	{
	case LTYPE_FLOAT: return a.as_float();
	case LTYPE_INT: return (float)(a.as_int());
	}

	return 0.0f;
}

lptr plus(const MultiArg& arg)
{
	if( arg.size() == 0 ) return (u64)0;

	if( arg.size() == 1 ) return arg[0];

	int largest_type = LTYPE_INT;
	for(int i = 0; i < arg.size(); ++i)
	{
		if( arg[i].type() > LTYPE_INT )
		{
			if( arg[i].type() >= LTYPE_OBJ ) return lptr();
			largest_type = arg[i].type();
		}
	}

	switch( largest_type )
	{
	case LTYPE_INT:
		{
			lptr retval = (u64)0;
			for(int i = 0; i < arg.size(); ++i)
			{
				retval = retval.as_int() + arg[i].as_int();
			}
			return retval;
		}
	case LTYPE_FLOAT:
		{
			lptr retval(0.0f);
			for(int i = 0; i < arg.size(); ++i)
			{
				if( arg[i].type() != LTYPE_FLOAT )
					retval = retval.as_float() + to_float_c(arg[i]);
				else
					retval = retval.as_float() + arg[i].as_float();
			}
			return retval;
		}
	default:
		return lptr();
	}

	return lptr();
}

lptr minus(const MultiArg& arg)
{
	if( arg.size() == 0 ) return (u64)0;

	int largest_type = LTYPE_INT;
	for(int i = 0; i < arg.size(); ++i)
	{
		if( arg[i].type() > LTYPE_INT )
		{
			if( arg[i].type() >= LTYPE_OBJ ) return lptr();
			largest_type = arg[i].type();
		}
	}

	if( arg.size() == 1 )
	{
		if( arg[0].type() == LTYPE_INT )
			return -arg[0].as_int();
		if( arg[0].type() == LTYPE_FLOAT )
			return -arg[0].as_float();
		else
			return lptr();
	}

	switch( largest_type )
	{
	case LTYPE_INT:
		{
			u64 retval = arg[0].as_int();
			for(int i = 1; i < arg.size(); ++i)
			{
				retval -= arg[i].as_int();
			}
			return retval;
		}
	case LTYPE_FLOAT:
		{
			float retval = to_float_c(arg[0]);
			for(int i = 1; i < arg.size(); ++i)
			{
				if( arg[i].type() != LTYPE_FLOAT )
					retval -= to_float_c(arg[i]);
				else
					retval -= arg[i].as_float();
			}
			return retval;
		}
	default:
		return lptr();
	}

	return lptr();
}

lptr mult(const MultiArg& arg)
{
	if( arg.size() == 0 ) return (u64)1;

	if( arg.size() == 1 ) return arg[0];

	int largest_type = LTYPE_INT;
	for(int i = 0; i < arg.size(); ++i)
	{
		if( arg[i].type() > LTYPE_INT )
		{
			if( arg[i].type() >= LTYPE_OBJ ) return lptr();
			largest_type = arg[i].type();
		}
	}

	switch( largest_type )
	{
	case LTYPE_INT:
		{
			s64 retval = 1;
			for(int i = 0; i < arg.size(); ++i)
			{
				retval = retval * (s64)arg[i].as_int();
			}
			return (u64)retval;
		}
	case LTYPE_FLOAT:
		{
			float retval = 1.0f;
			for(int i = 0; i < arg.size(); ++i)
			{
				if( arg[i].type() != LTYPE_FLOAT )
					retval = retval * to_float_c(arg[i]);
				else
					retval = retval * arg[i].as_float();
			}
			return retval;
		}
	default:
		return lptr();
	}

	return lptr();
}

lptr l_if(const MultiArg& args)
{
	if( args.size() < 1 ) return lptr();

	lptr res = eval({args[0]});
	if( args.size() == 1 ) return res;

	if( res.nilp() )
	{
		if( args.size() > 2 )
			return eval({args[2]});
		return lptr();
	}

	return eval({args[1]});
}

void lisp_init()
{
	global_T = intern_c("T");
	first_fscope.symbols.push_back(std::make_pair(global_T.sym(), global_T));

	QUOTE = intern_c("QUOTE");

	lisp_in_stream = new lstream(&std::cin);
	lisp_out_stream = new lstream(&std::cout);

	ldefine({intern_c("if"), new func((void*)&l_if, LFUNC_SPECIAL, -1)});
	ldefine({intern_c("*"), new func((void*)&mult, 0, -1)});
	ldefine({intern_c("+"), new func((void*)&plus, 0, -1)});
	ldefine({intern_c("-"), new func((void*)&minus,0, -1)});
	ldefine({intern_c("exit"), new func((void*)&lexit, 0, 1)});
	ldefine({intern_c("newline"), new func((void*)&newline, 0, -1)});
	ldefine({intern_c("display"), new func((void*)&ldisplay, 0, -1)});
	ldefine({intern_c("setf"), ldefine({intern_c("set!"), new func((void*)&setf, LFUNC_SPECIAL, 2)})});
	ldefine({intern_c("eval"), new func((void*)&eval, 0, -1)});
	ldefine({intern_c("apply"), new func((void*)&apply, 0, -1)});
	ldefine({intern_c("begin"), new func((void*)&begin_new_env, LFUNC_SPECIAL, -1)});
	ldefine({intern_c("return"), new func((void*)&lreturn, 0, 1)});
	ldefine({intern_c("car"), new func((void*)&car, 0, 1)});
	ldefine({intern_c("cdr"), new func((void*)&cdr, 0, 1)});
	ldefine({intern_c("cons"), new func((void*)&lcons, 0, 2)});
	ldefine({intern_c("define"), new func((void*)&ldefine, LFUNC_SPECIAL, -1)});
	ldefine({QUOTE, new func((void*)&lquote, LFUNC_SPECIAL, 1)});

	return;
}



