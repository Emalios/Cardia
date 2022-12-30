﻿#include "cdpch.hpp"
#include "Cardia/SerializationUtils/SceneSerializer.hpp"

#include "Cardia/ECS/Components.hpp"
#include <json/json.h>

namespace Json {
	template <> glm::vec2 Value::as<glm::vec2>() const {
		glm::vec3 out;
		out.x = this->operator[]("x").asFloat();
		out.y = this->operator[]("y").asFloat();
		return out;
	}

	template <> glm::vec3 Value::as<glm::vec3>() const {
		glm::vec3 out;
		out.x = this->operator[]("x").asFloat();
		out.y = this->operator[]("y").asFloat();
		out.z = this->operator[]("z").asFloat();
		return out;
	}

	template <> glm::vec4 Value::as<glm::vec4>() const {
		glm::vec4 out;
		out.x = this->operator[]("x").asFloat();
		out.y = this->operator[]("y").asFloat();
		out.z = this->operator[]("z").asFloat();
		out.w = this->operator[]("w").asFloat();
		return out;
	}
}


namespace Cardia::SerializerUtils
{
	void SerializeVec2(Json::Value& node, const glm::vec2& vec);
	void SerializeVec3(Json::Value& node, const glm::vec3& vec);
	void SerializeVec4(Json::Value& node, const glm::vec4& vec);
	void SerializeColorRgb(Json::Value& node, const glm::vec3& vec);
	void SerializeColorRgba(Json::Value& node, const glm::vec4& vec);

        std::string SerializeScene(Scene* scene, const std::string& workspace)
        {
                Json::Value root;
                const auto view = scene->GetRegistry().view<Component::Name>();

                for (const auto entity_id : view)
                {
                        Entity entity(entity_id, scene);
			const auto uuid = entity.getComponent<Component::ID>();

			const auto name = entity.getComponent<Component::Name>();

			root[uuid.uuid]["name"] = name.name;

			Json::Value node;
                        // Transform
                        const auto& transform = entity.getComponent<Component::Transform>();
			SerializeVec3(node["position"], transform.position);
			SerializeVec3(node["rotation"], transform.rotation);
			SerializeVec3(node["scale"], transform.scale);

                        root[uuid.uuid]["transform"] = node;
                        node.clear();

                        // SpriteRenderer
                        if (entity.hasComponent<Component::SpriteRenderer>())
                        {
                                const auto& spriteRenderer = entity.getComponent<Component::SpriteRenderer>();
				SerializeColorRgba(node["color"], spriteRenderer.color);
                                const auto path = spriteRenderer.texture ? spriteRenderer.texture->getPath() : "";
                                
                                node["texture"] = std::filesystem::relative(path, workspace).string();
                                node["tillingFactor"] = spriteRenderer.tillingFactor;
                                node["zIndex"] = spriteRenderer.zIndex;
                        
                                root[uuid.uuid]["spriteRenderer"] = node;
                                node.clear();
                        }
                        
                        // Camera
                        if (entity.hasComponent<Component::Camera>())
                        {
                                const auto& camera = entity.getComponent<Component::Camera>();
                                node["type"] = static_cast<int>(camera.camera.getProjectionType());
                                node["perspectiveFov"] = camera.camera.getPerspectiveFov();
                                node["perspectiveFar"] = camera.camera.getPerspectiveFar();
                                node["perspectiveNear"] = camera.camera.getPerspectiveNear();
                                
                                node["orthoSize"] =camera.camera.getOrthographicSize();
                                node["orthoNear"] = camera.camera.getOrthographicNear();
                                node["orthoFar"] = camera.camera.getOrthographicFar();
                                
                                root[uuid.uuid]["camera"] = node;
                                node.clear();
                        }

			// PointLight
			if (entity.hasComponent<Component::Light>())
			{
				const auto& light = entity.getComponent<Component::Light>();
				node["type"] = light.lightType;
				SerializeColorRgb(node["color"], light.color);

				node["range"] = light.range;
				node["angle"] = light.angle;
				node["smoothness"] = light.smoothness;

				root[uuid.uuid]["light"] = node;
				node.clear();
			}

                        if (entity.hasComponent<Component::Script>())
                        {
                                const auto& behavior = entity.getComponent<Component::Script>();
                                node["path"] = behavior.getPath();

				auto& attrsNode = node["attributes"];
				auto attrs = behavior.scriptClass.Attributes();
				for (auto& item : attrs) {
					auto& field = attrsNode[item.fieldName.c_str()];
					field["type"] = static_cast<int>(item.type);
					switch (item.type) {
						case ScriptFieldType::Int:
							field["value"] = py::handle(item.instance).cast<int>();
							break;
						case ScriptFieldType::Float:
							field["value"] = py::handle(item.instance).cast<float>();
							break;
						case ScriptFieldType::String:
							field["value"] = py::handle(item.instance).cast<std::string>();
							break;
						case ScriptFieldType::List:
						{
//							auto arr = Json::Value();
//
//							for (size_t i; i < py::len(item.instance.object()); i++) {
//								arr.append(item.instance.object().cast<int>());
//							}
							break;
						}
						case ScriptFieldType::Dict:
							field["value"] = "dict";
							break;
						case ScriptFieldType::PyBehavior:
							field["value"] = py::handle(item.instance).cast<std::string>();
							break;
						case ScriptFieldType::Vector2:
							SerializeVec2(field["value"], py::handle(item.instance).cast<glm::vec2>());
							break;
						case ScriptFieldType::Vector3:
							SerializeVec3(field["value"], item.instance.object().cast<glm::vec3>());
							break;
						case ScriptFieldType::Vector4:
							SerializeVec4(field["value"], py::handle(item.instance).cast<glm::vec4>());
							break;
					}
				}

                                root[uuid.uuid]["behavior"] = node;
                                node.clear();
                        }
                }

                std::stringstream output;

                output << root;
                
                return output.str();
        }

        bool DeserializeScene(const std::string& serializedScene, Scene& scene, const std::string& workspace)
        {
                Json::Value root;

                std::string err;
                const Json::CharReaderBuilder builder;
                const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
                if (!reader->parse(serializedScene.c_str(), serializedScene.c_str() + serializedScene.length(), &root, &err))
                {
                        Log::coreError("Could not load scene {0} : {1}", scene.GetName(), err);
                        return false;
                }

                scene.clear();

                for (const auto& uuid: root.getMemberNames())
                {
			UUID id;
			Entity entity;
			try {
				id = UUID::fromString(uuid);
				entity = scene.CreateEntityFromId(id);
			}
			catch (const std::invalid_argument& e) {
				Log::warn("Entity with invalid UUID found");
				entity = scene.CreateEntity();
			}

                        auto& node = root[uuid];
			auto& name = entity.getComponent<Component::Name>();
			name.name = node["name"].asString();

                        auto& component = entity.getComponent<Component::Transform>();
			component.position = node["transform"]["position"].as<glm::vec3>();
			component.rotation = node["transform"]["rotation"].as<glm::vec3>();
			component.scale = node["transform"]["scale"].as<glm::vec3>();

                        if (node.isMember("spriteRenderer"))
                        {
                                auto& spriteRenderer = entity.addComponent<Component::SpriteRenderer>();
				spriteRenderer.color = node["spriteRenderer"]["color"].as<glm::vec4>();

                                const auto path = std::filesystem::path(workspace);
                                auto texture = Texture2D::create((path / node["spriteRenderer"]["texture"].asString()).string());
                                if (texture->isLoaded())
                                {
                                        spriteRenderer.texture = std::move(texture);
                                }
                                
                                spriteRenderer.tillingFactor = node["spriteRenderer"]["tillingFactor"].asFloat();
                                spriteRenderer.zIndex = node["spriteRenderer"]["zIndex"].asInt();
                        }

                        if (node.isMember("camera"))
                        {
                                auto& camera = entity.addComponent<Component::Camera>();
                                camera.camera.setProjectionType(static_cast<SceneCamera::ProjectionType>(node["camera"]["type"].asInt()));
                                camera.camera.setPerspectiveFov(node["camera"]["perspectiveFov"].asFloat());
                                camera.camera.setPerspectiveFar(node["camera"]["perspectiveNear"].asFloat());
                                camera.camera.setPerspectiveFar(node["camera"]["perspectiveFar"].asFloat());

                                camera.camera.setOrthographicSize(node["camera"]["orthoSize"].asFloat());
                                camera.camera.setOrthographicNear(node["camera"]["orthoNear"].asFloat());
                                camera.camera.setOrthographicFar(node["camera"]["orthoFar"].asFloat());
                        }

			// PointLight
			if (node.isMember("light"))
			{
				auto& light = entity.addComponent<Component::Light>();
				light.lightType = node["light"]["type"].asInt();
				light.color = node["light"]["color"].as<glm::vec3>();

				light.range = node["light"]["range"].asFloat();
				light.angle = node["light"]["angle"].asFloat();
				light.smoothness = node["light"]["smoothness"].asFloat();

			}
                        
                        if (node.isMember("behavior"))
                        {
                                auto& behavior = entity.addComponent<Component::Script>();
                                const auto path = std::filesystem::path(workspace);
                                behavior.setPath(node["behavior"]["path"].asString());

				auto& attrsNode = node["behavior"]["attributes"];
				auto& attrs = behavior.scriptClass.Attributes();
				for (const auto& attrName: attrsNode.getMemberNames()) {
					ScriptField field;
					field.fieldName = attrName;
					field.type = static_cast<ScriptFieldType>(attrsNode[attrName]["type"].asInt());
					switch (field.type) {
						case ScriptFieldType::Int:
							field.instance = py::int_(attrsNode[attrName]["value"].asInt());
							break;
						case ScriptFieldType::Float:
							field.instance = py::float_(attrsNode[attrName]["value"].asFloat());
							break;
						case ScriptFieldType::String:
							field.instance = py::str(attrsNode[attrName]["value"].asString());
							break;
						case ScriptFieldType::List:
							field.instance = py::list();
							break;
						case ScriptFieldType::Dict:
							field.instance = py::dict();
							break;
						case ScriptFieldType::PyBehavior:
							field.instance = py::str(attrsNode[attrName]["value"].asString());
							break;
						case ScriptFieldType::Vector2:
						{
							auto vec = attrsNode[attrName]["value"].as<glm::vec2>();
							field.instance = py::cast(vec);
							break;
						}
						case ScriptFieldType::Vector3:
						{
							auto vec = attrsNode[attrName]["value"].as<glm::vec3>();
							field.instance = py::cast(vec);
							break;
						}
						case ScriptFieldType::Vector4:
						{
							auto vec = attrsNode[attrName]["value"].as<glm::vec4>();
							field.instance = py::cast(vec);
							break;
						}
						case ScriptFieldType::Unserializable:break;
					}
					auto attrPair = std::find_if(attrs.begin(), attrs.end(), [&](auto& attr) {
						return attr.fieldName == attrName;
					});
					if (attrPair != attrs.end()) {
						*attrPair = field;
					} else {
						attrs.push_back(field);
					}
				}

			}
                }
                
                return true;
        }

	void SerializeVec2(Json::Value& node, const glm::vec2& vec) {
		node["x"] = vec.x;
		node["y"] = vec.y;
	}

	void SerializeVec3(Json::Value& node, const glm::vec3& vec) {
		node["x"] = vec.x;
		node["y"] = vec.y;
		node["z"] = vec.z;
	}

	void SerializeVec4(Json::Value& node, const glm::vec4& vec) {
		node["x"] = vec.x;
		node["y"] = vec.y;
		node["z"] = vec.z;
		node["w"] = vec.z;
	}

	void SerializeColorRgb(Json::Value& node, const glm::vec3& vec) {
		node["x"] = vec.r;
		node["y"] = vec.g;
		node["z"] = vec.b;
	}

	void SerializeColorRgba(Json::Value& node, const glm::vec4& vec) {
		node["x"] = vec.r;
		node["y"] = vec.g;
		node["z"] = vec.b;
		node["w"] = vec.a;
	}

}
