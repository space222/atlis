#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include "types.h"
#include "funcs.h"

std::unordered_map<std::string, symbol*> symbols_by_name;
environ first_environ;
thread_local environ* environment = &first_environ;

lptr apply(const std::vector<lptr>& args)
{
	if( args.size() == 0 ) return lptr();

	if( args[0].type() != LTYPE_SYM )
	{
		//todo: error out
		return lptr();
	}

	lptr val = environ_lookup(environment, args[0]);
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
			return ( (zero_arg_func*)(F->ptr) ) ();
		}

		// running a lisp function with no args
		return begin_new_env(F->body);
	}

	std::vector<lptr> applargs;
	applargs.insert(std::begin(applargs), std::begin(args)+1, std::end(args));

	// if it isn't a special form, need to eval the args	
	if( ! (F->flags & LFUNC_SPECIAL) )
	{
		for_each(std::begin(applargs), std::end(applargs), [&](lptr &a) { a = eval({a, environment}); });
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
	environ* envnew = new environ(environment);
	environment = envnew;

	//todo: set up arguments in the environment

	// run begin on the body
	lptr retval = begin(F->body);

	// restore previous environment
	environment = environment->parent;
	delete envnew; // for now, but might eventually implement call/cc

	return retval;
}

lptr eval(const std::vector<lptr>& args)
{
	if( args.size() == 0 ) return lptr();
	lptr i = args[0];
	environ* env = environment;
	if( args.size() > 1 ) env = args[1].env();

	if( i.type() == LTYPE_SYM )
	{
		return environ_lookup(env, i);
	}
	if( i.type() != LTYPE_CONS ) 
	{
		return i;
	}

	std::vector<lptr> applargs;

	do {
		applargs.push_back(i.as_cons()->a);
		i = i.as_cons()->b;
	} while( i.type() == LTYPE_CONS );
	
	environ* temp = environment;
	environment = env;
	lptr retval = apply(applargs);
	environment = temp;
	return retval;
}

lptr lreturn(lptr arg)
{
	environment->need_return = true;
	environment->retval = arg;
	return arg;
}

lptr begin_new_env(lptr arg)
{
	environ* env = new environ(environment);
	environment = env;

	lptr retval = begin(arg);

	environment = environment->parent;
	delete env;
	return retval;
}

lptr begin(lptr arg)
{
	if( arg.type() != LTYPE_CONS ) return lptr();

	lptr res;
	do {
		cons* temp = arg.as_cons();
		res = eval({temp->a});
		if( temp->b.type() != LTYPE_CONS ) break;
		arg = temp->b;
	} while( !arg.nilp() && !environment->need_return );

	environment->position = arg;

	return environment->need_return ? environment->retval : res;
}

lptr intern_c(const std::string& name)
{
	std::string temp;
	for_each(std::begin(name), std::end(name), [&](char c) { temp += toupper(c); });
	
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

lptr environ_lookup(environ* env, lptr s)
{
	if( s.type() != LTYPE_SYM )
		return lptr();

	while( env )
	{
		auto iter = env->symbols.find(s.sym());
		if( iter != env->symbols.end() ) return iter->second;
		env = env->parent;
	}

	return lptr();
}

lptr setf(const std::vector<lptr>& args)
{
	if( args.size() < 2 ) return lptr();
	lptr sym = args[0];
	if( sym.type() != LTYPE_SYM ) return lptr();

	std::unordered_map<symbol*, lptr>::iterator iter;
	environ* env = environment;
	while( env )
	{
		iter = env->symbols.find(sym.sym());
		if( iter != env->symbols.end() ) break;
		env = env->parent;
	}

	lptr val;
	if( env )
	{
		val = eval({args[1]});
		iter->second = args[1];
	} else {
		//todo: error out somehow
		return lptr();
	}

	return val;
}

lptr display(const std::vector<lptr>& args)
{
	//todo: 1 optional arg is stream to output
	lptr s = args[0];
	if( s.type() != LTYPE_STR ) return lptr();
	std::cout << s.string()->txt;
	return s;
}

lptr newline(const std::vector<lptr>& args)
{
	//todo: 1 optional arg is stream to output
	std::cout << '\n'; //std::endl; //todo: is newline supposed to flush?
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

lptr lcons(const std::vector<lptr>& args)
{
	if( args.size() < 2 )
	{
		//todo: error out
		return lptr();
	}
	return new cons(args[0], args[1]);
}

void lisp_init()
{
	intern_c("T");

	setf({intern_c("newline"), new func((void*)&newline, 0, 0)});
	setf({intern_c("display"), new func((void*)&display, 0, 1)});
	setf({intern_c("setf"), setf({intern_c("set!"), new func((void*)&setf, LFUNC_SPECIAL, 2)})});
	setf({intern_c("eval"), new func((void*)&eval, 0, -1)});
	setf({intern_c("apply"), new func((void*)&apply, 0, -1)});
	setf({intern_c("begin"), new func((void*)&begin_new_env, LFUNC_SPECIAL, -1)});
	setf({intern_c("return"), new func((void*)&lreturn, LFUNC_SPECIAL, 1)});
	setf({intern_c("car"), new func((void*)&car, 0, 1)});
	setf({intern_c("cdr"), new func((void*)&cdr, 0, 1)});
	setf({intern_c("cons"), new func((void*)&lcons, 0, 2)});

	return;
}



