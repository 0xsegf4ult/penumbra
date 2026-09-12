#include <penumbra/cvar.hpp>
#include <penumbra/types.hpp>
#include <string_view>
#include <charconv>

namespace penumbra
{

static cvar_t* cvarlist = nullptr;

void cvar_register(cvar_t* cvar)
{
	cvar->next = nullptr;

	switch(cvar->type)
	{
	case CVAR_TYPE_INT:
		cvar->int_v = cvar->int_defv;
		break;
	case CVAR_TYPE_FLOAT:
		cvar->float_v = cvar->float_defv;
		break;
	};

	if(cvar_get(cvar->name))
		return;

	if(!cvarlist || cvar->name < cvarlist->name)
	{
		cvar->next = cvarlist;
		cvarlist = cvar;
	}
	else
	{
		cvar_t* prev = cvarlist;
		cvar_t* cur = cvarlist->next;
		while(cur && cvar->name > cur->name)
		{
			prev = cur;
			cur = cur->next;
		}

		cvar->next = prev->next;
		prev->next = cvar;
	}
}

cvar_t* cvar_get(std::string_view name)
{
	cvar_t* cur = cvarlist;
	while(cur)
	{
		if(cur->name == name)
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
		break;
	}

	if(cvar->callback)
		cvar->callback(cvar);
}

void cvar_set_string(cvar_t* cvar, std::string_view str)
{
	switch(cvar->type)
	{
	case CVAR_TYPE_INT:
	{
		int value = 0;
		auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
		if(ec == std::errc())
			cvar->int_v = value;
		else if(str == "true" || str == "TRUE" || str == "True")
			cvar->int_v = 1;
		else if(str == "false" || str == "FALSE" || str == "False")
			cvar->int_v = 0;

		break;
	}
	case CVAR_TYPE_FLOAT:
	{
		float value = 0.0f;
		auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
		if(ec == std::errc())
			cvar->float_v = value;

		break;
	}
	case CVAR_TYPE_STRING:
		break;
	}

	if(cvar->callback)
		cvar->callback(cvar);
}

}
