#pragma once
#include <vector>
#include <string>
#include "types.h"

lptr apply(const MultiArg& args);
lptr eval(const MultiArg& args);
lptr evlis(lptr);
lptr begin(const MultiArg&);
lptr begin_new_env(const MultiArg&);
lptr begin_c(lptr);
lptr begin_new_env_c(lptr);
lptr intern_c(const std::string&);
lptr intern(lptr);
lptr symbol_value(fscope*, lptr);

void lisp_init();


// IO
lptr newline(const MultiArg& args);
lptr ldisplay(const MultiArg& args);
lptr peek_char(const MultiArg& args);
lptr read_char(const MultiArg& args);
lptr lread(const MultiArg& args);
lptr lwrite(const MultiArg& args);




