#include <unordered_map>
#include <vector>
#include <istream>
#include "types.h"
#include "funcs.h"

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











