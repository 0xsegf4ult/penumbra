#include <import/qmap.hpp>
#include <world/state.hpp>
#include <penumbra/log.hpp>
#include <penumbra/vfs.hpp>
#include <penumbra/types.hpp>

#include <cstring>

using std::strcmp;

namespace penumbra
{

struct qmap_entity
{
};

struct parser_context
{
	const char* data;
	const char* script_p;
	char token[128];
	u32 scriptline;

	std::vector<qmap_entity> entities;
};

static bool parser_get_token(parser_context& ctx, bool crossline)
{
	char* token_p;

skipspace:
	while(*ctx.script_p <= 32)
	{
		if(!*ctx.script_p)
		{
			if(!crossline)
				log::error("qmap_parse: line {} incomplete", ctx.scriptline);
			
			return false;
		}
		if(*ctx.script_p++ == '\n')
		{
			if(!crossline)
				log::error("qmap_parse: line {} incomplete", ctx.scriptline);
			
			ctx.scriptline++;
		}
	}

	if(ctx.script_p[0] == '/' && ctx.script_p[1] == '/')
	{
		if(!crossline)
			log::error("qmap_parse: line {} incomplete", ctx.scriptline);

		while(*ctx.script_p++ != '\n')
		{
			if(!*ctx.script_p)
			{
				if(!crossline)
					log::error("qmap_parse: line {} incomplete", ctx.scriptline);
				return false;
			}
		}
			
		goto skipspace;
	}

	token_p = ctx.token;

	if(*ctx.script_p == '"')
	{
		ctx.script_p++;
		while(*ctx.script_p != '"')
		{
			if(!*ctx.script_p)
				log::error("qmap_parse: EOF inside quoted token");

			*token_p++ = *ctx.script_p++;
			if(token_p > &ctx.token[127])
				log::error("qmap_parse: token on line {} too large", ctx.scriptline);
		}
		ctx.script_p++;
	}
	else
	{
		while(*ctx.script_p > 32)
		{
			*token_p++ = *ctx.script_p++;
			if(token_p > &ctx.token[127])
				log::error("qmap_parse: token on line {} too large", ctx.scriptline);
		}
	}

	*token_p = '\0';

	return true;
}

static void parse_epair(parser_context& ctx)
{
	std::string key{ctx.token};
	parser_get_token(ctx, false);
	std::string value{ctx.token};

	log::info("qmap_parse: epair {} : {}", key, value);
}

static void parse_brush(parser_context& ctx)
{
	log::info("qmap_parse: brush");
	vec3 planepts[3];

	do
	{
		if(!parser_get_token(ctx, true))
			break;

		if(!strcmp(ctx.token, "}"))
			break;

		for(int i = 0; i < 3; i++)
		{
			if(i != 0)
				parser_get_token(ctx, true);

			if(strcmp(ctx.token, "("))
				log::error("qmap_parse: invalid brush definition");

			for(int j = 0; j < 3; j++)
			{
				parser_get_token(ctx, false);
				planepts[i][j] = std::atoi(ctx.token);
			}

			parser_get_token(ctx, false);
			if(strcmp(ctx.token, ")"))
				log::error("qmap_parse: invalid brush definition");
		}

		parser_get_token(ctx, false);
		parser_get_token(ctx, false);
		parser_get_token(ctx, false);
		parser_get_token(ctx, false);
		parser_get_token(ctx, false);
		parser_get_token(ctx, false);

		log::info("qmap_parse: plane {} {} {}", planepts[0], planepts[1], planepts[2]);
	} while(1);
}

static bool qmap_parse(parser_context& ctx)
{
	if(!parser_get_token(ctx, true))
		return false;

	if(strcmp(ctx.token, "{"))
		log::error("qmap_parse: { not found");

	ctx.entities.push_back({});
	auto& mapent = ctx.entities.back();

	do
	{
		if(!parser_get_token(ctx, true))
			log::error("qmap_parse: EOF without closing brace");

		if(!strcmp(ctx.token, "}"))
			break;

		if(!strcmp(ctx.token, "{"))
			parse_brush(ctx);
		else
			parse_epair(ctx);
	} while(1);

	return true;
}

bool import_qmap(qmap_import_context& ctx, const vfs_path& path)
{
	auto map_file = vfs_open(path, VFS_ACCESS_READ);
	if(map_file < 0)
	{
		log::warn("import_qmap[{}] failed: could not open file", path.string());
		return false;
	}

	parser_context pctx;
	pctx.data = reinterpret_cast<const char*>(vfs_map(map_file));
	pctx.script_p = pctx.data;
	pctx.scriptline = 1;

	while(qmap_parse(pctx))
	{
	}

	vfs_close(map_file);
	return true;
}

}
