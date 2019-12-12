#pragma once
#include <vector>
#include <string>
#include "types.h"

lptr apply(const std::vector<lptr>& args);
lptr eval(const std::vector<lptr>& args);
lptr evlis(lptr);
lptr begin(lptr);
lptr begin_new_env(lptr arg);
lptr intern_c(const std::string&);
lptr intern(lptr);
lptr environ_lookup(environ*, lptr);

void lisp_init();

