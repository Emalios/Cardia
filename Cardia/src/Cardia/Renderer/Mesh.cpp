﻿#include "cdpch.hpp"
#include "Cardia/Renderer/Mesh.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Cardia/Core/Log.hpp>


namespace Cardia
{
	std::vector<std::shared_ptr<Mesh>> Mesh::ReadMeshFromFile(const std::string &path)
	{
		std::vector<std::shared_ptr<Mesh>> meshes;
		Assimp::Importer importer;
		const aiScene *scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

		if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			Log::coreError("Failed to load model : {0}", importer.GetErrorString());
			return {};
		}


		for (int ind = 0; ind < scene->mNumMeshes; ind++) {
			Mesh mesh;
			aiMesh* ai_mesh = scene->mMeshes[ind];
			// walk through each of the mesh's vertices
			for(unsigned i = 0; i < ai_mesh->mNumVertices; i++) {
				Vertex vertex{};
				vertex.color = {1.0f, 0.5f, 0.2f, 1.0f};
				glm::vec3 vector;
				vector.x = ai_mesh->mVertices[i].x;
				vector.y = ai_mesh->mVertices[i].y;
				vector.z = ai_mesh->mVertices[i].z;

				vertex.position = vector;

				if (ai_mesh->HasNormals())
				{
					vector.x = ai_mesh->mNormals[i].x;
					vector.y = ai_mesh->mNormals[i].y;
					vector.z = ai_mesh->mNormals[i].z;
					vertex.normal = vector;
				}

				if(ai_mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
				{
					glm::vec2 vec;
					// a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't
					// use models where a vertex can have multiple texture coordinates so we always take the first set (0).
					vec.x = ai_mesh->mTextureCoords[0][i].x;
					vec.y = ai_mesh->mTextureCoords[0][i].y;
					vertex.textureCoord = vec;
				}
				else
					vertex.textureCoord = glm::vec2(0.0f, 0.0f);

				for (unsigned int faceIndex = 0; faceIndex < ai_mesh->mNumFaces; faceIndex++)
				{
					aiFace face = ai_mesh->mFaces[faceIndex];
					// retrieve all indices of the face and store them in the indices vector
					for(unsigned int j = 0; j < face.mNumIndices; j++)
						mesh.indices.push_back(face.mIndices[j]);
				}
				mesh.vertices.emplace_back(vertex);
			}
			meshes.emplace_back(std::make_shared<Mesh>(mesh));
		}

		return meshes;
	}
}
