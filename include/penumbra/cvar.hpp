#pragma once

#include <penumbra/types.hpp>
#include <string_view>

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
	std::string_view name;
	cvar_type type;

	union
	{
		int int_defv;
		float float_defv;
		std::string_view string_defv;
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
cvar_t* cvar_get(std::string_view name);
void cvar_set(cvar_t* cvar, u64 value);
void cvar_set_string(cvar_t* cvar, std::string_view str);

}
