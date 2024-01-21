#include <windows.h>
#include <D3dkmthk.h>
#include <chrono>
#include "dwmapi.h"
using namespace std::chrono;

#pragma comment(lib,"dwmapi.lib")

const LPCWSTR g_szClassName = L"dummyApp";
bool quitProgram = false;

// Window event handling
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CLOSE:
		quitProgram = true;
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		quitProgram = true;
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	WNDCLASSEX wc;
	HWND hwnd;
	MSG Msg;

	// Registering the Window Class
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = g_szClassName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, L"Window Registration Failed!", L"Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	// Window ignores mouseclicks thanks to this advice:
	// https://stackoverflow.com/questions/31313624/click-through-transparent-window-no-dragging-allowed-c

	// Creating the Window
	hwnd = CreateWindowEx(
		WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
		g_szClassName,
		L"DummyApp",
		WS_POPUP,
		0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
		NULL, NULL, hInstance, NULL);

	if (hwnd == NULL)
	{
		MessageBox(NULL, L"Window Creation Failed!", L"Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	bool flipper = false;

	while (!quitProgram)
	{
		flipper = !flipper;
		// Set the color key to RGB(1, 1, 1) or RGB(2, 2, 2) each frame.
		// This does nothing since the window is completely transparent anyway but it tricks dwm into updating.
		// Do 1 + flipper because RGB(0, 0, 0) as color key makes the mini-window seen when holding alt-tab or hovering over it on the taskbar flicker.
		SetLayeredWindowAttributes(hwnd, 1 + flipper, 0, LWA_COLORKEY|LWA_ALPHA);
		RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
		DwmFlush();
		while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE) > 0) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
	}

	return Msg.wParam;
}