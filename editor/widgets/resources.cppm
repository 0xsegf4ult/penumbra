export module penumbra.editor:resource_window;

import :widget;

import penumbra.core;
import penumbra.math;
import penumbra.resource;
import penumbra.ui;
import std;
using std::uint32_t, std::size_t;

namespace penumbra
{
	
enum class ResourceView
{
	ResourceTypes,
	Geometry,
	Texture,
	Material,
	Animation,
	Skeleton
};

constexpr const char* resource_view_names[] =
{
	"geometry",
	"textures",
	"materials",
	"animations",
	"skeletons"
};

struct ItemData
{
	const char* name;
	ResourceID resource;
};

export class ResourceWindow : public Widget
{
public:
	ResourceWindow() : Widget("Resources") {}

	void on_draw() override
	{
		ImGui::SetNextWindowContentSize(ImVec2(0.0f, layout_outer_padding + layout_line_count * (layout_item_size.y + layout_item_spacing)));
		if(ImGui::BeginChild("Resources##panel", ImVec2(0.0f, -ImGui::GetTextLineHeightWithSpacing()), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoMove))
		{
			ImDrawList* draw_list = ImGui::GetWindowDrawList();

			const float avail_width = ImGui::GetContentRegionAvail().x;
			update_layout(avail_width);

			ImVec2 start_pos = ImGui::GetCursorScreenPos();
			start_pos = ImVec2(start_pos.x + layout_outer_padding, start_pos.y + layout_outer_padding);
			ImGui::SetCursorScreenPos(start_pos);

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(layout_selectable_spacing, layout_selectable_spacing));

			ImGuiListClipper clipper;
			clipper.Begin(layout_line_count, layout_item_step.y);
			while(clipper.Step())
			{
				for(int line_idx = clipper.DisplayStart; line_idx < clipper.DisplayEnd; line_idx++)
				{
					const int item_min_idx_curline = line_idx * layout_column_count;
					const int item_max_idx_curline = std::min((line_idx + 1) * layout_column_count, get_element_count());
					for(int item_idx = item_min_idx_curline; item_idx < item_max_idx_curline; ++item_idx)
					{
						ImGui::PushID(item_idx);

						ImVec2 pos = ImVec2(start_pos.x + (item_idx % layout_column_count) * layout_item_step.x, start_pos.y + line_idx * layout_item_step.y);
						ImGui::SetCursorScreenPos(pos);

						bool item_visible = ImGui::IsRectVisible(layout_item_size);
						ImGui::Selectable("", false, ImGuiSelectableFlags_None, layout_item_size);
					
						auto item_data = get_element_data(item_idx);
						
						if(res_view == ResourceView::ResourceTypes)
						{
							if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
							{
								res_view = static_cast<ResourceView>(item_idx + 1);
								ImGui::PopID();
								break;
							}
						}
						else
						{
							if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && item_idx == 0)
							{
								res_view = ResourceView::ResourceTypes;
								ImGui::PopID();
								break;
							}

							if(item_idx > 0 && ImGui::BeginDragDropSource())
							{
								ImGui::SetDragDropPayload("resource_payload", &item_data.resource, sizeof(ResourceID));
							       	ImGui::Text("%s", item_data.name ? item_data.name : "resource");
								ImGui::EndDragDropSource();
							}
						}

						if(item_visible)
						{
							ImVec2 box_min(pos.x - 1, pos.y - 1);
							ImVec2 box_max(box_min.x + layout_item_size.x + 2, box_min.y + layout_item_size.y + 2);
							draw_list->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(0.13f, 0.13f, 0.13f, 0.86f)));
							if(item_data.name)
							{
								ImVec2 text_size = ImGui::CalcTextSize(item_data.name);
								ImGui::RenderTextEllipsis(draw_list, ImVec2(box_min.x, box_max.y - ImGui::GetFontSize()), ImVec2(box_max.x - 4, box_max.y), box_max.x - 4, item_data.name, nullptr, &text_size);
								if(ImGui::IsItemHovered() && text_size.x > (box_max.x - 4 - box_min.x))
									ImGui::SetTooltip("%s", item_data.name);
							}
						}

						ImGui::PopID();
					}
				}
			}
			clipper.End();
			ImGui::PopStyleVar();

			if(ImGui::IsWindowAppearing())
				zoom_accum = 0.0f;

			ImGuiIO& io = ImGui::GetIO();
			if(ImGui::IsWindowHovered() && io.MouseWheel != 0.0f && ImGui::IsKeyDown(ImGuiMod_Ctrl))
			{
				zoom_accum += io.MouseWheel;
				if(std::fabsf(zoom_accum) >= 1.0f)
				{
					item_size *= std::powf(1.1f, static_cast<float>(static_cast<int>(zoom_accum)));
					item_size = std::clamp(item_size, 48.0f, 128.0f);
					zoom_accum -= zoom_accum;
					update_layout(avail_width);
				}
			}
		}
		ImGui::EndChild();
	}
private:
	int get_element_count()
	{
		switch(res_view)
		{
		case ResourceView::ResourceTypes:
			return 5;
		case ResourceView::Geometry:
			return resource_manager_get_geometry_storage().size() + 1;
		case ResourceView::Texture:
			return resource_manager_get_texture_storage().size() + 1;
		case ResourceView::Material:
			return resource_manager_get_material_storage().size() + 1;
		case ResourceView::Animation:
			return resource_manager_get_animation_storage().size() + 1;
		case ResourceView::Skeleton:
			return resource_manager_get_skeleton_storage().size() + 1;
		default:
			return 1;
		}
	}

	ItemData get_element_data(int item_idx)
	{
		if(res_view == ResourceView::ResourceTypes)
		{
			return {resource_view_names[item_idx], {}};
		}

		if(item_idx == 0)
			return {"..", {}};

		switch(res_view)
		{
		case ResourceView::Geometry:
			return {resource_manager_get_geometry_storage()[item_idx - 1].name.c_str(), ResourceID{ResourceType::Geometry, uint32_t(item_idx)}};
		case ResourceView::Texture:
			return {resource_manager_get_texture_storage()[item_idx - 1].name.c_str(), ResourceID{ResourceType::Texture, uint32_t(item_idx)}};
		case ResourceView::Material:
			return {resource_manager_get_material_storage()[item_idx - 1].name.c_str(), ResourceID{ResourceType::Material, uint32_t(item_idx)}};
		case ResourceView::Animation:
			return {resource_manager_get_animation_storage()[item_idx - 1].name.c_str(), ResourceID{ResourceType::Animation, uint32_t(item_idx)}};
		case ResourceView::Skeleton:
			return {resource_manager_get_animation_storage()[item_idx - 1].name.c_str(), ResourceID{ResourceType::Skeleton, uint32_t(item_idx)}};
		default:
			return {"invalid", {}};
		}
	}

	void update_layout(float avail_width)
	{
		layout_item_spacing = static_cast<float>(item_spacing);
		layout_item_size = ImVec2(std::floorf(item_size), std::floorf(item_size));
		layout_column_count = std::max(static_cast<int>(avail_width / (layout_item_size.x + layout_item_spacing)), 1);
		layout_line_count = (get_element_count() + layout_column_count - 1) / layout_column_count;

		if(layout_column_count > 1)
			layout_item_spacing = std::floorf(avail_width - layout_item_size.x * layout_column_count) / layout_column_count;

		layout_item_step = ImVec2(layout_item_size.x + layout_item_spacing, layout_item_size.y + layout_item_spacing);
		layout_selectable_spacing = std::max(std::floorf(layout_item_spacing) - item_hit_spacing, 0.0f);
		layout_outer_padding = std::floorf(layout_item_spacing * 0.5f);
	}


	ImVec2 layout_item_size;
	ImVec2 layout_item_step;
	float layout_item_spacing = 0.0f;
	float layout_selectable_spacing = 0.0f;
	float layout_outer_padding = 0.0f;
	int layout_column_count = 0;
	int layout_line_count = 0;

	float item_size = 96.0f;
	int item_spacing = 10;
	int item_hit_spacing = 4;
	float zoom_accum = 0.0f;

	ResourceView res_view{ResourceView::ResourceTypes};
};

}
