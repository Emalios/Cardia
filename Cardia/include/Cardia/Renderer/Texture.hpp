#pragma once

#include <memory>
#include <string>


namespace Cardia
{
	class Texture {
	public:
		virtual ~Texture() = default;
		virtual uint32_t getHeight() const = 0;
		virtual uint32_t getWidth() const = 0;

		virtual void bind(int slot = 0) const = 0;
		virtual bool operator==(const Texture& other) const = 0;
		virtual uint32_t getRendererID() = 0;
		virtual bool isLoaded() { return m_Loaded; }
		virtual std::string getPath() const { return m_Path; }
		virtual bool isTransparent() const { return m_IsTransparent; }
	protected:
		bool m_Loaded = false;
		bool m_IsTransparent = false;
		std::string m_Path {};
	};

	class Texture2D : public Texture
	{
	public:
		static std::unique_ptr<Texture2D> create(const std::string& path);
		static std::unique_ptr<Texture2D> create(int width, int height, void* data);
	};
}