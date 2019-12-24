#include <unordered_map>
#include <vector>
#include <istream>
#include <stdio.h>
#include "types.h"
#include "funcs.h"

extern lptr lisp_out_stream;
extern lptr lisp_in_stream;
extern lptr QUOTE;

lptr write_char(const MultiArg& args)
{
	if( args.size() == 0 ) return lptr();

	if( args[0].type() != LTYPE_INT )
	{
		return lptr();
	}

	lptr S;
	if( args.size() > 1 && args[1].type() == LTYPE_STREAM )
	{
		S = args[1];
	} else {
		S = lisp_out_stream;
	}

	if( S.stream()->flags & LSTREAM_OUT )
	{
		char c =(char) args[0].as_int();
		std::get<std::ostream*>(lisp_out_stream.stream()->strm)->put(c);
		return args[0];
	}

	return lptr();
}

void lstream_write_string(lptr stream, const std::string_view SV)
{
	lstream* SM = stream.stream();

	if( std::holds_alternative<std::fstream*>(SM->strm) )
	{
		*std::get<std::fstream*>(SM->strm) << SV;
	} else if( std::holds_alternative<std::ostream*>(SM->strm) ) {
		*std::get<std::ostream*>(SM->strm) << SV;
	} else if( std::holds_alternative<std::stringstream*>(SM->strm) ) {
		*std::get<std::stringstream*>(SM->strm) << SV;
	}

	return;
}

lptr newline(const MultiArg& args)
{
	lptr ostr = lisp_out_stream;
	if( args.size() > 0 && args[0].type() == LTYPE_STREAM )
	{
		ostr = args[0];
	}

	write_char({(u64)'\n', ostr});

	return lptr();
}

lptr lwrite(const MultiArg& args)
{
	if( args.size() == 0 ) return lptr();

	lptr ostr = lisp_out_stream;
	if( args.size() > 1 && args[1].type() == LTYPE_STREAM )
	{
		ostr = args[1];
	}

	if( ! ( ostr.stream()->flags & LSTREAM_OUT ) )
	{
		return lptr();
	}

	if( args[0].nilp() )
	{
		lstream_write_string(ostr, "Nil");
		return ostr;
	}

	//printf("arg type = %i\n", args[0].type());

	switch( args[0].type() )
	{
	case LTYPE_INT: lstream_write_string(ostr, std::to_string(args[0].as_int())); return ostr;
	case LTYPE_FLOAT: lstream_write_string(ostr, std::to_string(args[0].as_float())); return ostr;
	case LTYPE_STR: lstream_write_string(ostr, '"' + args[0].string()->txt + '"'); return ostr;
	case LTYPE_SYM: lstream_write_string(ostr, args[0].sym()->name); break;
	case LTYPE_FUNC: lstream_write_string(ostr, "<#function @" + std::to_string((u64)args[0].as_func()) + ">"); break;
	default: break;
	}

	if( args[0].type() == LTYPE_CONS )
	{
		cons* cc = args[0].as_cons();
		//todo: the special reader things like quote, splice, etc
		write_char({(u64)'(', ostr});
		lwrite({cc->a, ostr});
		while( cc->b.type() == LTYPE_CONS && (cc = cc->b.as_cons(), true) )
		{
			write_char({(u64)' ', ostr});
			lwrite({cc->a, ostr});
		}
		lptr last = cc->b;
		if( ! last.nilp() )
		{
			lstream_write_string(ostr, " . ");
			lwrite({cc->b, ostr});
		}
		write_char({(u64)')', ostr});

		return ostr;
	}

	return lptr();
}

lptr ldisplay(const MultiArg& args)
{
	if( args.size() == 0 ) return lptr();

	lptr ostr = lisp_out_stream;
	if( args.size() > 1 && args[1].type() == LTYPE_STREAM )
	{
		ostr = args[1];
	}

	if( ! ( ostr.stream()->flags & LSTREAM_OUT ) )
	{
		return lptr();
	}

	if( args[0].nilp() )
	{
		lstream_write_string(ostr, "Nil");
		return ostr;
	}

	switch( args[0].type() )
	{
	case LTYPE_INT: lstream_write_string(ostr, std::to_string(args[0].as_int())); break;
	case LTYPE_FLOAT: lstream_write_string(ostr, std::to_string(args[0].as_float())); break;
	case LTYPE_STR: lstream_write_string(ostr, args[0].string()->txt); break;
	case LTYPE_CONS: lwrite(args); break;
	case LTYPE_SYM: lstream_write_string(ostr, args[0].sym()->name); break;
	case LTYPE_FUNC: lwrite(args); break;
	}

	return args[0];
}

lptr open_output_file(const MultiArg& args)
{
	if( args.size() == 0 ) return lptr();

	if( args[0].type() != LTYPE_STR )
	{
		return lptr();
	}

	std::fstream* out1 = new std::fstream(args[0].string()->txt, std::ios_base::binary|std::ios_base::out);

	if( !*out1 )
	{
		delete out1;
		return lptr();
	}

	return new lstream(out1, LSTREAM_FILE|LSTREAM_IN);
}

lptr open_input_file(const MultiArg& args)
{
	if( args.size() == 0 ) return lptr();

	if( args[0].type() != LTYPE_STR )
	{
		return lptr();
	}

	std::fstream* in1 = new std::fstream(args[0].string()->txt, std::ios_base::binary|std::ios_base::in);

	if( !*in1 )
	{
		delete in1;
		return lptr();
	}

	return new lstream(in1);
}

lptr lclose(lptr port)
{
	if( port.type() != LTYPE_STREAM )
	{
		return lptr();
	}

	lstream* S = port.stream();

	if( std::holds_alternative<std::fstream*>(S->strm) )
	{
		std::get<std::fstream*>(S->strm)->close();
	} else if( std::holds_alternative<std::stringstream*>(S->strm) ) {
		// can't close a stringstream, but nothing happens.
	} else if( std::holds_alternative<int>(S->strm) ) {
		//todo: socket close
	} 

	return lptr();
}

lptr peek_char(const MultiArg& args)
{
	lptr port = lisp_in_stream;
	if( args.size() > 0 && args[0].type() == LTYPE_STREAM )
		port = args[0];

	if( ! (port.stream()->flags & LSTREAM_IN) ) return lptr();

	lstream* S = port.stream();
	if( std::holds_alternative<std::fstream*>(S->strm) )
	{
		return (u64)(u8) std::get<std::fstream*>(S->strm)->peek();
	} else if( std::holds_alternative<std::istream*>(S->strm) ) {
		return (u64)(u8) std::get<std::istream*>(S->strm)->peek();
	}
	
	return (u64)(u8) std::get<std::stringstream*>(S->strm)->peek();
}

lptr read_char(const MultiArg& args)
{
	lptr port = lisp_in_stream;
	if( args.size() > 0 && args[0].type() == LTYPE_STREAM )
		port = args[0];

	if( ! (port.stream()->flags & LSTREAM_IN) ) return lptr();

	lstream* S = port.stream();
	if( std::holds_alternative<std::fstream*>(S->strm) )
	{
		return (u64)(u8) std::get<std::fstream*>(S->strm)->get();
	} else if( std::holds_alternative<std::istream*>(S->strm) ) {
		return (u64)(u8) std::get<std::istream*>(S->strm)->get();
	}
	
	return (u64)(u8) std::get<std::stringstream*>(S->strm)->get();
}

void consume_ws(lptr port)
{
	int c =(int) peek_char({port}).as_int();
	while( isspace(c) && c != -1 )
	{
		read_char({port});
		c =(int) peek_char({port}).as_int();
		while( c == ';' )
		{
			while( (c != '\n' && c != -1) || isspace(c) )
			{
				read_char({port});
				c =(int) peek_char({port}).as_int();
			}		
		}
	}
	return;
}

lptr lread(const MultiArg& args)
{
	lptr port = lisp_in_stream;
	if( args.size() > 0 && args[0].type() == LTYPE_STREAM )
		port = args[0];

	if( ! (port.stream()->flags & LSTREAM_IN) ) return lptr();

	consume_ws(port);

	int c =(int) peek_char({port}).as_int();
	if( c == -1 ) return lptr();

	if( c == '(' )
	{
		//printf("about to list\n");
		read_char({port});
		consume_ws(port);
		c = (int) peek_char({port}).as_int();
		if( c == ')' )
		{  // empty list
			read_char({port});
			return lptr();
		}

		lptr a = lread({port});
		cons* fin = new cons(a, lptr());
		cons* temp = fin;
		consume_ws(port);
		c =(int) peek_char({port}).as_int();
		while( c != ')' )
		{
			if( c == '.' )
			{
				read_char({port});
				consume_ws(port);
				temp->b = lread({port});
				c =(int) read_char({port}).as_int();
				if( c != ')' )
				{
					//todo: malformed list
					while( c != ')' && c != -1 ) c =(int) read_char({port}).as_int();
				}
				return fin;
			}
			cons* n = new cons(lread({port}), lptr());
			temp->b = n;
			temp = n;
			consume_ws(port);
			c =(int) peek_char({port}).as_int();
		}
		read_char({port});
		return fin;
	}

	//printf("c == <%c>\n", (char)c);
	
	if( c == '\'' )
	{
		read_char({port});
		lptr b = lread({port});
		return new cons(QUOTE, new cons(b, lptr()));
	}

	if( c == ',' )
	{
		read_char({port});
		c = peek_char({port}).as_int();
		if( c == '@' )
		{
			read_char({port});
			lptr b = lread({port});
			return new cons(intern_c("unquote-splice"), b);
		}
		lptr b = lread({port});
		return new cons(intern_c("unquote"), b);
	}

	if( c == '\"' )
	{
		read_char({port});
		c =(int) peek_char({port}).as_int();
		if( c == '\"' || c == -1 )
		{
			return new lstr();
		}
		std::string str;
		do {
			str += c;
			read_char({port});
			c =(int) peek_char({port}).as_int();
		} while( c != '\"' && c != -1 );
		read_char({port});
		return new lstr(str);
	}

	std::string atom;
	do {
		atom += c;
		read_char({port});
		c =(int) peek_char({port}).as_int();
	} while( !isspace(c) && c != '(' && c != ')' && c != ';' && c != ',' && c != '`' && c != '\'' );

	size_t pos;
	try {
		u64 res = std::stoll(atom, &pos, 0);
		if( pos == atom.size() )
		{
			return res;
		}
	} catch(...) {}

	try {
		float r2 = std::stof(atom, &pos);
		if( pos == atom.size() )
		{
			return r2;
		}
	} catch(...) {}

	lptr a = intern_c(atom);
	//printf("got = %i\n", a.type());
	return a;
}










