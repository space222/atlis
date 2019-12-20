#include <iostream>
#include "types.h"
#include "funcs.h"


int main()
{
try {
	lisp_init();
	while(1)
	{
		lwrite({ eval({ lread({}) }) });
	}
} catch(const char* e) {
	std::cout << e << std::endl;
}
	return 0;
}

