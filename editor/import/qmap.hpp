#include <penumbra/vfs.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

struct WorldState;

struct qmap_import_context
{
	WorldState* world;
};

bool import_qmap(qmap_import_context& ctx, const vfs_path& path);

}
