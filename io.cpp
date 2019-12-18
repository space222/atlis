#include <unordered_map>
#include <vector>
#include <istream>
#include "types.h"
#include "funcs.h"

extern lptr lisp_out_stream;
extern lptr lisp_in_stream;


lptr write_char(const std::vector<lptr>& args)
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

lptr newline(const std::vector<lptr>& args)
{
	lptr ostr = lisp_out_stream;
	if( args.size() > 0 && args[0].type() == LTYPE_STREAM )
	{
		ostr = args[0];
	}

	write_char({(u64)'\n', ostr});

	return lptr();
}

lptr lwrite(const std::vector<lptr>& args)
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

	switch( args[0].type() )
	{
	case LTYPE_INT: lstream_write_string(ostr, std::to_string(args[0].as_int())); return ostr;
	case LTYPE_FLOAT: lstream_write_string(ostr, std::to_string(args[0].as_float())); return ostr;
	case LTYPE_STR: lstream_write_string(ostr, '"' + args[0].string()->txt + '"'); return ostr;
	}

	if( args[0].type() == LTYPE_CONS )
	{
		cons* cc = args[0].as_cons();
		//todo: the special reader things like quote, splice, etc
		write_char({(u64)'(', ostr});
		do {
			lwrite({cc->a, ostr});
			if( cc->b.type() != LTYPE_CONS )
				lstream_write_string(ostr, " . ");
			else
				lstream_write_string(ostr, ", ");
		} while( !cc->b.nilp() && cc->b.type() == LTYPE_CONS && (cc = cc->b.as_cons(), true) );
		lptr last = cc;
		if( ! last.nilp() )
		{
			lwrite({cc->b, ostr});
		}
		write_char({(u64)')', ostr});

		return ostr;
	}

	return lptr();
}

lptr ldisplay(const std::vector<lptr>& args)
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

	switch( args[0].type() )
	{
	case LTYPE_INT: lstream_write_string(ostr, std::to_string(ostr.as_int())); break;
	case LTYPE_FLOAT: lstream_write_string(ostr, std::to_string(ostr.as_float())); break;
	case LTYPE_STR: lstream_write_string(ostr, ostr.string()->txt); break;
	case LTYPE_CONS: lwrite(args); break;
	}

	return args[0];
}

lptr open_output_file(const std::vector<lptr>& args)
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

lptr open_input_file(const std::vector<lptr>& args)
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











