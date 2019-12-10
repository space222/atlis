#include <vector>
#include <string>
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
	if( i.type() != LTYPE_CONS ) return i;

	environ* env = environment;
	if( args.size() > 1 ) env = args[1].env();

	return apply({i, env});
}

lptr evlis(lptr list)
{
	return lptr();
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



