#pragma once
struct GLFWwindow;
namespace Core { struct Config; }
namespace Platform {
	class WindowManager {
	public:
		WindowManager(); ~WindowManager();
		bool create(const Core::Config& cfg);
		void poll();
		void swap();
		bool shouldClose() const;
		void requestClose();
		GLFWwindow* handle() const { return m_window; }
		int width() const { return m_width; }
		int height() const { return m_height; }
	private:
		GLFWwindow* m_window = nullptr;
		int m_width = 0, m_height = 0;
	};
}
