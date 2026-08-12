#include <penumbra/cvar.hpp>
#include <penumbra/types.hpp>
#include <string>

namespace penumbra
{

static cvar_t* cvarlist = nullptr;

void cvar_register(cvar_t* cvar)
{
	cvar->next = nullptr;

	if(cvar_get(cvar->name))
		return;

	if(!cvarlist || std::strcmp(cvar->name, cvarlist->name) < 0)
	{
		cvar->next = cvarlist;
		cvarlist = cvar;
	}
	else
	{
		cvar_t* prev = cvarlist;
		cvar_t* cur = cvarlist->next;
		while(cur && (std::strcmp(cvar->name, cur->name) > 0))
		{
			prev = cur;
			cur = cur->next;
		}

		cvar->next = prev->next;
		prev->next = cvar;
	}
}

cvar_t* cvar_get(const char* name)
{
	cvar_t* cur = cvarlist;
	while(cur)
	{
		if(!std::strcmp(cur->name, name))
			return cur;

		cur = cur->next;
	}

	return nullptr;
}

void cvar_set(cvar_t* cvar, u64 value)
{
	switch(cvar->type)
	{
	case CVAR_TYPE_INT:
		cvar->int_v = static_cast<int>(value);
		break;
	case CVAR_TYPE_FLOAT:
		std::memcpy(&cvar->float_v, &value, sizeof(float));
		break;
	case CVAR_TYPE_STRING:
		//FIXME: duplicate?
		cvar->string_v = reinterpret_cast<char*>(value);
		break;
	}

	if(cvar->callback)
		cvar->callback(cvar);
}

}
