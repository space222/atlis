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

	cons* c = args[0].as_cons();
	if( c->a.type() != LTYPE_SYM )
	{
		//todo: error out
		return lptr();
	}

	environ* env = environment;
	if( args.size() > 1 ) env = args[1].env();

	lptr val = environ_lookup(env, c->a);
	if( val.nilp() || val.type() != LTYPE_FUNC )
	{
		//todo: error out
		return lptr();
	}

	func* F = val.as_func();

	if( c->b.nilp() )
	{
		// having no arguments makes things easier
		if( F->ptr )
		{
			return ( (zero_arg_func*)(F->ptr) ) ();
		}

		// running a lisp function with no args
		environ* envnew = new environ(env);
		environment = envnew;
		return begin(F->body);
	}

	std::vector<lptr> applargs;
	lptr temp = c->b;
	if( temp.type() != LTYPE_CONS )
	{
		//todo: error out
		return lptr();
	}

	// gather args
	do {
		applargs.push_back(temp.as_cons()->a);
		temp = temp.as_cons()->b;
	} while( !temp.nilp() && temp.type() == LTYPE_CONS );

	// if it isn't a special form, need to eval the args	
	if( ! (F->flags & LFUNC_SPECIAL) )
	{
		for_each(std::begin(applargs), std::end(applargs), [&](lptr &a) { a = eval({a, env}); });
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
	environ* envnew = new environ(env);
	environment = envnew;

	//todo: set up arguments in the environment

	// run begin on the body
	lptr retval = begin(F->body);

	// restore previous environment
	environment = envnew->parent;
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

	return apply({i, env});
}

lptr lreturn(lptr arg)
{
	environment->need_return = true;
	environment->retval = arg;
	return arg;
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

	return res;
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

lptr display(lptr s)
{
	if( s.type() != LTYPE_STR ) return lptr();
	std::cout << s.string()->txt;
	return s;
}

lptr newline()
{
	std::cout << '\n'; //std::endl; //todo: is newline supposed to flush?
	return lptr();
}


void lisp_init()
{
	intern_c("T");

	setf({intern_c("newline"), new func((void*)&newline, 0, 0)});
	setf({intern_c("display"), new func((void*)&display, 0, 1)});
	setf({intern_c("setf"), setf({intern_c("set!"), new func((void*)&setf, LFUNC_SPECIAL, 2)})});
	setf({intern_c("eval"), new func((void*)&eval, 0, -1)});
	setf({intern_c("apply"), new func((void*)&apply, 0, -1)});
	setf({intern_c("begin"), new func((void*)&begin, LFUNC_SPECIAL, -1)});
	setf({intern_c("return"), new func((void*)&lreturn, LFUNC_SPECIAL, 1)});

	return;
}



