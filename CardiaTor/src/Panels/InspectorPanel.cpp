﻿#include "Panels/InspectorPanel.hpp"

#include <filesystem>
#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#include "Cardia/ECS/Components.hpp"
#include "Cardia/ECS/Entity.hpp"


namespace Cardia::Panel
{
	void InspectorPanel::SetDataToField(ScriptInstance* instance, std::pair<std::string, ScriptField>& item, const std::function<bool(py::object&)>& imGuiCallback) {
		auto fieldName = item.first;
		if (instance) {
			auto field = instance->GetAttrOrMethod(fieldName.c_str());
			auto res = imGuiCallback(field);
			item.second.instance = field;
			if (res) {
				instance->SetAttr(fieldName.c_str(), field);
			}
		} else {
			auto field = py::object(item.second.instance);
			imGuiCallback(field);
			item.second.instance = field;
		}
	}

        void InspectorPanel::OnImGuiRender()
        {
                ImGui::Begin("Inspector");

		if(!m_CurrentScene)
		{
			ImGui::End();
			return;
		}

		auto selectedEntity = m_CurrentScene->GetCurrentEntity();
		if(!selectedEntity)
		{
			ImGui::End();
			return;
		}
		// Name Component
		auto& name = selectedEntity.getComponent<Component::Name>();
		auto& uuid = selectedEntity.getComponent<Component::ID>();

		char buffer[128] {0};
		constexpr size_t bufferSize = sizeof(buffer)/sizeof(char);
		name.name.copy(buffer, bufferSize);

		if(ImGui::InputText("Name", buffer, bufferSize))
		{
			name.name = std::string(buffer);
		}

		// Transform Component

		DrawInspectorComponent<Component::Transform>("Transform", selectedEntity, [](Component::Transform& transform) {
			DrawVec3DragFloat("Position", transform.position);

			auto rotation = glm::degrees(transform.rotation);
			DrawVec3DragFloat("Rotation", rotation);
			transform.rotation = glm::radians(rotation);

			DrawVec3DragFloat("Scale", transform.scale, 1);
		});

		// SpriteRenderer Component

		DrawInspectorComponent<Component::SpriteRenderer>("Sprite Renderer", selectedEntity, [](Component::SpriteRenderer& sprite) {
			ImGui::ColorEdit4("Color", glm::value_ptr(sprite.color));
			ImGui::DragFloat("Tiling Factor", &sprite.tillingFactor, 0.05f, 0);
			uint32_t whiteColor = 0xffffffff;

			const auto white = Texture2D::create(1, 1, &whiteColor);
			const auto texID = sprite.texture ? sprite.texture->getRendererID() : white->getRendererID();

			ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<size_t>(texID)), {15, 15}, {0, 1}, {1, 0});
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PATH"))
				{
					const auto* path = static_cast<const char*>(payload->Data);
					auto tex = Texture2D::create(path);
					if (tex->isLoaded())
					{
						sprite.texture = std::move(tex);
					}
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::SameLine();
			ImGui::Text("Texture");
			ImGui::DragInt("zIndex", &sprite.zIndex, 0.05f);
		});

		// Camera Component

		DrawInspectorComponent<Component::Camera>("Camera", selectedEntity, [](Component::Camera& camera) {
			SceneCamera& cam = camera.camera;

			int type = static_cast<int>(cam.getProjectionType());
			const char *cameraTypes[] = {"Perspective", "Orthographic"};
			if (ImGui::Combo("Camera Type", &type, cameraTypes, sizeof(cameraTypes) / sizeof(char *)))
				cam.setProjectionType(static_cast<SceneCamera::ProjectionType>(type));

			auto& isPrimary = camera.primary;

			ImGui::Checkbox("Primary", &isPrimary);

			if (cam.getProjectionType() == SceneCamera::ProjectionType::Perspective) {
				float pFov = glm::degrees(cam.getPerspectiveFov());
				if (ImGui::DragFloat("Fov", &pFov, 0.05f))
					cam.setPerspectiveFov(glm::radians(pFov));
				float pFar = cam.getPerspectiveFar();
				if (ImGui::DragFloat("Far", &pFar, 0.05f))
					cam.setPerspectiveFar(pFar);
				float pNear = cam.getPerspectiveNear();
				if (ImGui::DragFloat("Near", &pNear, 0.05f))
					cam.setPerspectiveNear(pNear);
			}
			else if (cam.getProjectionType() == SceneCamera::ProjectionType::Orthographic) {
				float oSize = cam.getOrthographicSize();
				if (ImGui::DragFloat("Size", &oSize, 0.05f))
					cam.setOrthographicSize(oSize);
				float oFar = cam.getOrthographicFar();
				if (ImGui::DragFloat("Far", &oFar, 0.05f))
					cam.setOrthographicFar(oFar);
				float oNear = cam.getOrthographicNear();
				if (ImGui::DragFloat("Near", &oNear, 0.05f))
					cam.setOrthographicNear(oNear);
			}
		});

		// Light Component

		DrawInspectorComponent<Component::Light>("Light", selectedEntity, [](Component::Light& light) {
			int item_current = light.lightType;
			const char* items[] = { "Directional Light", "Point Light", "Spot Light" };
			ImGui::Combo("Light Type", &item_current, items, IM_ARRAYSIZE(items));

			light.lightType = item_current;

			ImGui::ColorEdit3("Color", glm::value_ptr(light.color));
			if (item_current == 0) return;

			ImGui::DragFloat("Range", &light.range, 0.01f, 0.0f);

			if (item_current == 1) return;

			ImGui::DragFloat("Angle", &light.angle, 0.5f, 0.0f);

			ImGui::DragFloat("Smoothness", &light.smoothness, 0.5f);
		});

		DrawInspectorComponent<Component::Script>("Script", selectedEntity, [&](Component::Script& scriptComponent) {
			std::filesystem::path filepath = scriptComponent.getPath();
			auto path = filepath.filename().string();

			char* buffer = &path[0];
			ImGui::InputText("label", buffer, path.size(), ImGuiInputTextFlags_ReadOnly);

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PATH"))
				{
					const std::filesystem::path behaviorPath = static_cast<const char*>(payload->Data);
					if (behaviorPath.extension() == ".py")
					{
						scriptComponent.setPath(behaviorPath.string());
					}
					else
					{
						Log::warn("Could not load file {0}", behaviorPath.string());
					}
				}
				ImGui::EndDragDropTarget();
			}

			for (auto& item: scriptComponent.scriptClass.Attributes())
			{
				auto fieldName = item.first;
				auto instance = ScriptEngine::Instance().GetInstance(uuid.uuid);
				auto type = item.second.type;
				switch (type) {
					case ScriptFieldType::Int:
						SetDataToField(instance, item, [&](py::object& pyField) {
							auto field = pyField.cast<int>();
							ImGui::DragInt(fieldName.c_str(), &field, 0.1f);
							pyField = py::cast(field);
							return true;
						});
						break;
					case ScriptFieldType::Float:
					{
						SetDataToField(instance, item, [&](py::object& pyField) {
							auto field = pyField.cast<float>();
							ImGui::DragFloat(fieldName.c_str(), &field, 0.1f);
							pyField = py::cast(field);
							return true;
						});
						break;
					}
					case ScriptFieldType::String:
					{
						SetDataToField(instance, item, [&](py::object& pyField) {
							auto field = pyField.cast<std::string>();
							char buff[128] {0};
							constexpr size_t bufferSize = sizeof(buff)/sizeof(char);
							field.copy(buff, bufferSize);
							ImGui::InputText(fieldName.c_str(), buff, IM_ARRAYSIZE(buff));
							pyField = py::cast(std::string(buff));
							return true;
						});
						break;
					}
					case ScriptFieldType::List:
					{
						auto list = py::list(instance->GetAttrOrMethod(fieldName.c_str()));
						break;
					}
					case ScriptFieldType::Dict: {
						auto dict = py::dict(instance->GetAttrOrMethod(fieldName.c_str()));
						break;
					}
					case ScriptFieldType::PyBehavior:
					{
						char buff[128]{0};
						constexpr size_t bufferSize = sizeof(buff) / sizeof(char);
						auto id = py::handle(item.second.instance).cast<std::string>();
						try {
							auto entity = m_CurrentScene->GetEntityByUUID(UUID::fromString(id));

							if (entity) {
								auto instanceName = entity.getComponent<Component::Name>().name;
								instanceName.copy(buff, bufferSize);
							}
						} catch (std::exception& e) {

						}
						ImGui::InputText(fieldName.c_str(), buff, bufferSize, ImGuiInputTextFlags_ReadOnly);
						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_UUID"))
							{
								const char* str = static_cast<const char*>(payload->Data);
								item.second.instance = py::cast(str);
								auto script = ScriptEngine::Instance().GetInstance(UUID::fromString(str));
								if (script && instance)
								{
									instance->SetAttr(fieldName.c_str(), *script);
								}
							}
							ImGui::EndDragDropTarget();
						}
						break;
					}
					case ScriptFieldType::Vector2:
						SetDataToField(instance, item, [&](py::object& pyField) {
							auto field = pyField.cast<glm::vec2>();
							ImGui::DragFloat2(fieldName.c_str(), glm::value_ptr(field), 0.1f);
							py::setattr(pyField, "x", py::cast(field.x));
							py::setattr(pyField, "y", py::cast(field.y));
							return false;
						});
						break;
					case ScriptFieldType::Vector3:
						SetDataToField(instance, item, [&](py::object& pyField) {
							auto field = pyField.cast<glm::vec3>();
							DrawVec3DragFloat(fieldName, field);
							py::setattr(pyField, "x", py::cast(field.x));
							py::setattr(pyField, "y", py::cast(field.y));
							py::setattr(pyField, "z", py::cast(field.z));
							return false;
						});
						break;
					case ScriptFieldType::Vector4:
						SetDataToField(instance, item, [&](py::object& pyField) {
							auto field = pyField.cast<glm::vec4>();
							ImGui::DragFloat4(fieldName.c_str(), glm::value_ptr(field), 0.1f);
							py::setattr(pyField, "x", py::cast(field.x));
							py::setattr(pyField, "y", py::cast(field.y));
							py::setattr(pyField, "z", py::cast(field.z));
							py::setattr(pyField, "w", py::cast(field.w));
							return false;
						});
						break;
					case ScriptFieldType::Unserializable:break;
				}

			}
		});

		ImGui::Separator();

		const auto windowWidth = ImGui::GetWindowSize().x;
		const auto textWidth   = ImGui::CalcTextSize("Add Components...").x;

		ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
		if (ImGui::Button("Add Component...")) {
			ImGui::OpenPopup("Add Component");
		}


		// Add component button
		if (ImGui::BeginPopup("Add Component"))
		{
			if (!selectedEntity.hasComponent<Component::Camera>() && ImGui::MenuItem("Camera"))
			{
				selectedEntity.addComponent<Component::Camera>();
				ImGui::EndPopup();
			}

			if (!selectedEntity.hasComponent<Component::SpriteRenderer>() && ImGui::MenuItem("Sprite Renderer"))
			{
				selectedEntity.addComponent<Component::SpriteRenderer>();
				ImGui::EndPopup();
			}

			if (!selectedEntity.hasComponent<Component::Script>() && ImGui::MenuItem("Entity Behavior"))
			{
				selectedEntity.addComponent<Component::Script>();
				ImGui::EndPopup();
			}

			if (!selectedEntity.hasComponent<Component::Light>() && ImGui::MenuItem("Light"))
			{
				selectedEntity.addComponent<Component::Light>();
				ImGui::EndPopup();
			}
		}
		ImGui::End();
        }


        template<typename T>
	void InspectorPanel::DrawInspectorComponent(const char* name, Entity entity, std::function<void(T&)> func)
        {
        	constexpr auto componentFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;

        	if (entity.hasComponent<T>()) {
        		auto& component = entity.getComponent<T>();
        		if(ImGui::TreeNodeEx(static_cast<void*>(&component), componentFlags, "%s", name))
        		{
        			if (ImGui::BeginPopupContextItem())
        			{
        				if (ImGui::MenuItem("Reset Component"))
        				{
        					component.reset();
        					ImGui::EndPopup();
        				}

        				if (!std::is_same_v<T, Component::Transform> && ImGui::MenuItem("Remove Component"))
        				{
						entity.removeComponent<T>();
        					ImGui::EndPopup();
        				}
        				ImGui::EndPopup();
        			}
        			func(component);
        			ImGui::TreePop();
        		}
        	}
        }

	void InspectorPanel::OnSceneLoad(Cardia::Scene *scene)
	{
		m_CurrentScene = scene;
	}

	void InspectorPanel::DrawVec3DragFloat(const std::string &label, glm::vec3 &vector, float reset,
					       float columnWidth)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
			vector.x = reset;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &vector.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
			vector.y = reset;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &vector.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
			vector.z = reset;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &vector.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}
}
