#pragma once
#include <unordered_map>
#include <vector>
#include <istream>
#include <fstream>
#include <sstream>
#include <string>
#include <variant>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t  s8;

const int LTYPE_INT = 0;
const int LTYPE_FLOAT = 1;
const int LTYPE_SYM = 2;
const int LTYPE_FUNC = 3;
const int LTYPE_CONS = 4;
const int LTYPE_OBJ = 5;
const int LTYPE_ENV = 6;
const int LTYPE_STR = 7;
const int LTYPE_STREAM = 8;

const int LGC_MARK = (1<<31);
const int LGC_NO_FREE = (1<<30);
const int LGC_TYPE_MASK = (LGC_MARK|LGC_NO_FREE);

struct lobj
{
	u32 type;
};

#define LTYPE(a) ((a)->type & ~(1<<31))

struct cons;
struct symbol;
struct fscope;
struct func;
struct lstr;
struct lstream;

class lptr
{
public:
	lptr() : val(LTYPE_OBJ) {}

	lptr(u64 v)
	{
		val = (v<<3);
		return;
	}

	lptr(float v)
	{
		val = *(u64*)&v;
		val <<= 3;
		val |= LTYPE_FLOAT;
	}

	lptr(cons* c)
	{
		val =(u64) c;
		if( c )
			val |= LTYPE_CONS;
		else
			val = LTYPE_OBJ;
		return;
	}

	lptr(symbol* s)
	{
		val =(u64) s;
		if( s )
			val |= LTYPE_SYM;
		else
			val = LTYPE_OBJ;
		return;
	}

	lptr(fscope* e)
	{
		val =(u64) e;
		val |= LTYPE_OBJ;
		return;
	}

	lptr(lstr* s)
	{
		val =(u64) s;
		val |= LTYPE_OBJ;
		return;
	}

	lptr(lstream* s)
	{
		val =(u64) s;
		val |= LTYPE_OBJ;
		return;
	}

	lptr(func* f)
	{
		val =(u64) f;
		if( f )
			val |= LTYPE_FUNC;
		else
			val |= LTYPE_OBJ;
	}

	lptr(lobj* p)
	{
		if( !p )
		{
			val = LTYPE_OBJ;
			return;
		}

		val =(u64) p;
		if( (p->type&~LGC_TYPE_MASK) < LTYPE_OBJ )
			val |= (p->type&~LGC_TYPE_MASK);
		else
			val |= 4;
		return;
	}

	lstream* stream() const { return (lstream*)(val&~7); }
	func* as_func() const { return (func*)(val&~7); }
	cons* as_cons() const { return (cons*)(val&~7); }
	symbol* sym() const { return (symbol*)(val&~7); }
	fscope* env() const { return (fscope*)(val&~7); }
	lstr* string() const { return (lstr*)(val&~7); }
	u64 as_int() const { return val>>3; }
	float as_float() const { u64 v = val>>3; return *(float*)&v; }

	int type() const
	{
		int t = val&7;
		if( t == LTYPE_OBJ )
		{
			lobj* L =(lobj*) (val&~7);
			if( L ) t = L->type & ~LGC_TYPE_MASK;
		}
		return t;
	}

	bool nilp() const
	{
		return (val&7)==4 && (val&~7)==0;
	}

	u64 val;
};

struct cons
{
	cons() : type(LTYPE_CONS) {}
	cons(lptr a1, lptr b1) : type(LTYPE_CONS), a(a1), b(b1) {}

	u32 type;
	lptr a, b;
};

struct symbol
{
	symbol() : type(LTYPE_SYM) {}
	symbol(const std::string& n) : type(LTYPE_SYM), name(n) { }

	u32 type;
	std::string name;
};

struct fscope
{
	fscope(fscope* par = nullptr) : type(LTYPE_ENV), parent(par), need_return(false) {}

	u32 type;
	func* F;
	u32 pc;
	fscope* parent;
	bool need_return;
	lptr retval;

	std::vector<std::pair<symbol*, lptr>> symbols;
	std::vector<int> scope; //nested scopes (LET, etc). number of symbols to pop
	std::vector<lptr> position;
};

const int LFUNC_SPECIAL = 1;  // function is special form
const int LFUNC_BYTECODE = 2; // func::ptr is bytecode not native

using zero_arg_func = lptr(void);
using one_arg_func = lptr(lptr);
using multiarg_func = lptr(const std::vector<lptr>&);

struct func
{
	func() : type(LTYPE_FUNC), flags(0), num_args(0), closure(nullptr),
			ptr(nullptr) {}

	func(void* p, u32 f, u32 numargs) : type(LTYPE_FUNC), ptr(p), flags(f), num_args(numargs), closure(nullptr) {}

	u32 type;
	u32 flags;
	u32 num_args;
	fscope* closure;
	void* ptr;
	lptr body;
	lptr pos;
};

struct lstr
{
	lstr() : type(LTYPE_STR) {}
	lstr(const std::string& s) : type(LTYPE_STR), txt(s) {}

	u32 type;
	std::string txt;
};

struct lstream
{
	lstream() : type(LTYPE_STREAM) {}
	lstream(std::fstream* f) : type(LTYPE_STREAM), strm(f) {}
	~lstream() 
	{ 
		if( std::holds_alternative<std::fstream*>(strm) )
		{
			delete std::get<std::fstream*>(strm);
		} else if( std::holds_alternative<std::stringstream*>(strm) ) {
			delete std::get<std::stringstream*>(strm);
		} else if( std::holds_alternative<int>(strm) ) {
			int a = std::get<int>(strm);
		}
		return;
	}

	u32 type;
	std::variant<std::fstream*, std::stringstream*, int, std::monostate> strm;
};














