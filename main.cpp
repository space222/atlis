#include <iostream>
#include "types.h"
#include "funcs.h"


int main()
{
	lisp_init();
	while(1)
	{
		lwrite({ lread({}) });
	}

	return 0;
}

