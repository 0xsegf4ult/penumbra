#pragma once

#include <penumbra/types.hpp>

namespace penumbra
{

enum cvar_type
{
	CVAR_TYPE_INT,
	CVAR_TYPE_FLOAT,
	CVAR_TYPE_STRING
};

struct cvar_t
{
	const char* name;
	cvar_type type;

	union
	{
		int int_defv;
		float float_defv;
		const char* string_defv;
	};

	union
	{
		int int_v;
		float float_v;
		char* string_v;
	};

	void (*callback)(cvar_t* cvar){nullptr};

	cvar_t* next{nullptr};
};


void cvar_register(cvar_t* cvar);
cvar_t* cvar_get(const char* name);
void cvar_set(cvar_t* cvar, u64 value);

}
