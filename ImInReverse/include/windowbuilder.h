#pragma once

#include "pch.h"

// Forward declarations
class Window;
class WBPlugin;

// A simple configuration struct for window properties.
struct WindowConfig {
	const char* title = "Window";
	const char* className = "WindowClass";
	std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	int width = 800;
	int height = 600;
	bool useImmersiveTitlebar = true;
	bool vsync = false; // Pd9ba
	std::function<void(Window&)> onResize = nullptr;
	std::function<void(Window&)> onClose = nullptr;
	std::function<void(Window&)> onRender = nullptr;
	std::vector<std::unique_ptr<WBPlugin>> plugins;
};

/// <summary>
/// Base plugin class that can be used to extend the window functionality.
/// </summary>
class WBPlugin {
public:
	/// <summary>
	/// Destructor.
	/// </summary>
	virtual ~WBPlugin() = default;

	/// <summary>
	/// Called when the plugin is loaded.
	/// </summary>
	/// <param name="window">The window instance.</param>
	virtual void OnLoad(Window&) {}

	/// <summary>
	/// Called when the plugin is unloaded.
	/// </summary>
	/// <param name="window">The window instance.</param>
	virtual void OnUnload(Window&) {}

	/// <summary>
	/// Called before the window is rendered.
	/// </summary>
	/// <param name="window">The window instance.</param>
	virtual void PreRender(Window&) {}

	/// <summary>
	/// Called after the window is rendered but before the swap chain is presented.
	/// </summary>
	/// <param name="window">The window instance.</param>
	virtual void PostRender(Window&) {}

	/// <summary>
	/// Called when the window receives a message but previous handlers have not processed it.
	/// </summary>
	/// <param name="window">The window instance.</param>
	/// <param name="msg">The message ID.</param>
	/// <param name="wParam">The WPARAM parameter.</param>
	/// <param name="lParam">The LPARAM parameter.</param>
	virtual void HandleMessage(Window&, UINT, WPARAM, LPARAM) {}
};

/// <summary>
/// A fully built an presentable window.
/// </summary>
class Window {
public:
	// Delete copy operations
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	// Allow move semantics if needed
	Window(Window&&) = default;
	Window& operator=(Window&&) = default;

	~Window() {
		if (renderTargetView) renderTargetView->Release();
		if (swapChain) swapChain->Release();
		if (context) context->Release();
		if (device) device->Release();
	}

	/// <summary>
	/// Shows the window and enters the message loop.
	/// </summary>
	void Show() {
		MSG msg = {};
		constexpr DWORD idleWaitMs = 100; // 10 FPS
		constexpr DWORD eventsMask = QS_ALLINPUT;

		while (msg.message != WM_QUIT) {
			// Wait for any event or timeout (for 10 FPS idle rendering)
			DWORD result = MsgWaitForMultipleObjects(0, nullptr, FALSE, idleWaitMs, eventsMask);

			// Messages are available
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			// Always render once per loop, regardless of message presence
			context->ClearRenderTargetView(renderTargetView, clearColor.data());

			for (auto& plugin : plugins)
				plugin->PreRender(*this);

			if (onRender)
				onRender(*this);

			for (auto& plugin : plugins)
				plugin->PostRender(*this);

			swapChain->Present(vsync ? 1 : 0, 0);
		}

		for (auto& plugin : plugins)
			plugin->OnUnload(*this);
	}


	explicit Window(WindowConfig config)
		: width(config.width),
		height(config.height),
		title(config.title),
		className(config.className),
		clearColor(config.clearColor),
		onResize(config.onResize ? config.onResize : defaultOnResize),
		onClose(config.onClose ? config.onClose : defaultOnClose),
		onRender(config.onRender ? config.onRender : defaultOnRender),
		plugins(std::move(config.plugins)),
		useImmersiveTitlebar(config.useImmersiveTitlebar),
		vsync(config.vsync)
	{
		// Register window class
		WNDCLASS wc = {};
		wc.lpfnWndProc = WndProc;
		wc.hInstance = GetModuleHandle(NULL);
#ifdef UNICODE
		wchar_t wClassName[256];
		swprintf(wClassName, 256, L"%hs", className);
		wc.lpszClassName = wClassName;
#else
		wc.lpszClassName = className;
#endif
		RegisterClass(&wc);

		// Create window
		HWND hWnd = nullptr;
#ifdef UNICODE
		wchar_t wTitle[256];
		swprintf(wTitle, 256, L"%hs", title);
		hWnd = CreateWindow(wClassName, wTitle, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, width, height,
			NULL, NULL, GetModuleHandle(NULL), NULL);
#else
		hWnd = CreateWindow(className, title, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, width, height,
			NULL, NULL, GetModuleHandle(NULL), NULL);
#endif

		// Associate this Window instance with the HWND
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		this->hWnd = hWnd;
		this->hInstance = GetModuleHandle(NULL);

		// Optionally enable an immersive (e.g., dark mode) titlebar
		if (useImmersiveTitlebar) {
			HKEY hKey = nullptr;
			if (RegOpenKeyEx(HKEY_CURRENT_USER,
#ifdef UNICODE
				L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
#else
				"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
#endif
				0, KEY_READ, &hKey) == ERROR_SUCCESS) {
				DWORD value = 0;
				DWORD size = sizeof(DWORD);
				if (RegQueryValueEx(hKey,
#ifdef UNICODE
					L"AppsUseLightTheme",
#else
					"AppsUseLightTheme",
#endif
					NULL, NULL,
					reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS) {
					BOOL dark = value == 0;
					DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
						&dark, sizeof(BOOL));
				}
				RegCloseKey(hKey);
			}
		}

		// Create DX11 device and swap chain
		DXGI_SWAP_CHAIN_DESC scd = {};
		scd.BufferCount = 1;
		scd.BufferDesc.Width = width;
		scd.BufferDesc.Height = height;
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd.BufferDesc.RefreshRate.Numerator = 60;
		scd.BufferDesc.RefreshRate.Denominator = 1;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.OutputWindow = hWnd;
		scd.SampleDesc.Count = 1;
		scd.SampleDesc.Quality = 0;
		scd.Windowed = TRUE;

		HRESULT res = D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
			nullptr, 0, D3D11_SDK_VERSION, &scd,
			&swapChain, &device, nullptr, &context);
		if (res != S_OK) {
			std::cerr << "Failed to create device and swap chain" << std::endl;
			LPSTR errorMsg = nullptr;
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, res, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				reinterpret_cast<LPSTR>(&errorMsg), 0, nullptr);
			std::cerr << errorMsg << std::endl;
			LocalFree(errorMsg);
			return;
		}

		// Create render target view
		ID3D11Texture2D* backBuffer = nullptr;
		swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(&backBuffer));
		assert(backBuffer != nullptr);
		device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
		backBuffer->Release();

		ShowWindow(hWnd, SW_SHOW);
		UpdateWindow(hWnd);

		context->OMSetRenderTargets(1, &renderTargetView, nullptr);

		// Notify plugins that the window has loaded
		for (auto& plugin : plugins)
			plugin->OnLoad(*this);
	}

	// DX11/Win32 objects
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;
	IDXGISwapChain* swapChain = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;
	HINSTANCE hInstance = nullptr;
	HWND hWnd = nullptr;

	// Window properties
	int width = 800;
	int height = 600;
	const char* title = "Window";
	const char* className = "WindowClass";
	std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	std::function<void(Window&)> onResize = defaultOnResize;
	std::function<void(Window&)> onClose = defaultOnClose;
	std::function<void(Window&)> onRender = defaultOnRender;
	std::vector<std::unique_ptr<WBPlugin>> plugins = {};
	bool useImmersiveTitlebar = false;
	bool vsync = false;

	// Window procedure
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
		if (window) {
			switch (message) {
			case WM_SIZE:
				window->width = LOWORD(lParam);
				window->height = HIWORD(lParam);
				if (window->onResize)
					window->onResize(*window);
				break;
			case WM_CLOSE:
				if (window->onClose)
					window->onClose(*window);
				break;
			}

			for (auto& plugin : window->plugins)
				plugin->HandleMessage(*window, message, wParam, lParam);
		}

		return DefWindowProc(hWnd, message, wParam, lParam);
	}

private:
	// Default callback implementations
	static void defaultOnResize(Window& window) {
		if (window.renderTargetView) window.renderTargetView->Release();
		window.swapChain->ResizeBuffers(0, window.width, window.height, DXGI_FORMAT_UNKNOWN, 0);
		ID3D11Texture2D* backBuffer = nullptr;
		window.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(&backBuffer));
		window.device->CreateRenderTargetView(backBuffer, nullptr, &window.renderTargetView);
		backBuffer->Release();
		window.context->OMSetRenderTargets(1, &window.renderTargetView, nullptr);
	}
	static void defaultOnClose(Window&) {
		PostQuitMessage(0);
	}
	static void defaultOnRender(Window&) {}
};

// A builder class for creating a Window.
class WindowBuilder {
public:
	WindowBuilder() {}

	WindowBuilder& Name(const char* title, const char* className = "WindowClass") {
		config.title = title;
		config.className = className;
		return *this;
	}
	WindowBuilder& Size(int width, int height) {
		config.width = width;
		config.height = height;
		return *this;
	}
	WindowBuilder& ClearColor(float r, float g, float b, float a) {
		config.clearColor = { r, g, b, a };
		return *this;
	}
	WindowBuilder& OnResize(std::function<void(Window&)> onResize) {
		config.onResize = onResize;
		return *this;
	}
	WindowBuilder& OnClose(std::function<void(Window&)> onClose) {
		config.onClose = onClose;
		return *this;
	}
	WindowBuilder& OnRender(std::function<void(Window&)> onRender) {
		config.onRender = onRender;
		return *this;
	}
	WindowBuilder& ImmersiveTitlebar(bool useImmersiveTitlebar = true) {
		config.useImmersiveTitlebar = useImmersiveTitlebar;
		return *this;
	}
	WindowBuilder& VSync(bool vsync = true) { // P8b76
		config.vsync = vsync;
		return *this;
	}

	template<typename T>
	WindowBuilder& Plugin() {
		config.plugins.emplace_back(std::make_unique<T>());
		return *this;
	}

	/// <summary>
	/// Creates a new Window instance with the current configuration.
	/// </summary>
	/// <returns>The new Window instance, heap-allocated with a unique pointer.</returns>
	std::unique_ptr<Window> Build() {
		return std::make_unique<Window>(std::move(config));
	}

private:
	WindowConfig config;
};
