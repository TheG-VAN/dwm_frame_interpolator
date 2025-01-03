// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <io.h>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <d3d12.h>
#include "vertex_shader.hlsl.h"
#include "main_pass.hlsl.h"
#include "curr_pass.hlsl.h"
#include "motion_pass.hlsl.h"
#include "change_pass.hlsl.h"
#include "copy_motion_pass.hlsl.h"
#include "mult_pass.hlsl.h"
#pragma comment (lib, "d3d11.lib") // Maybe un-useful
#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "dxgi.lib") // Maybe un-useful
#pragma comment (lib, "uuid.lib") // Maybe un-useful
#pragma comment (lib, "dxguid.lib")


#pragma intrinsic(_ReturnAddress)

#define RELEASE_IF_NOT_NULL(x) { if (x != NULL) { x->Release(); } }
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#define RESIZE(x, y) realloc(x, (y) * sizeof(*x));
#define LOG_FILE_PATH R"(C:\Users\kam\Downloads\dwm_lut-master\DwmLutGUI\DwmLutGUI\bin\x64\Debug\log.txt)"
#define MAX_LOG_FILE_SIZE 20 * 1024 * 1024
#define DEBUG_MODE true

#if DEBUG_MODE == true
#define __LOG_ONLY_ONCE(x, y) if (static bool first_log_##y = true) { log_to_file(x); first_log_##y = false; }
#define _LOG_ONLY_ONCE(x, y) __LOG_ONLY_ONCE(x, y)
#define LOG_ONLY_ONCE(x) _LOG_ONLY_ONCE(x, __COUNTER__)
#define MESSAGE_BOX_DBG(x, y) MessageBoxA(NULL, x, "DEBUG HOOK DWM", y);

#define EXECUTE_WITH_LOG(winapi_func_hr) \
	do { \
		HRESULT hr = (winapi_func_hr); \
		if (FAILED(hr)) \
		{ \
			std::stringstream ss; \
			ss << "ERROR AT LINE: " << __LINE__ << " HR: " << hr << " - DETAILS: "; \
			LPSTR error_message = nullptr; \
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
				NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error_message, 0, NULL); \
			ss << error_message; \
			log_to_file(ss.str().c_str()); \
			LocalFree(error_message); \
			throw std::exception(ss.str().c_str()); \
		} \
	} while (false);

#define EXECUTE_D3DCOMPILE_WITH_LOG(winapi_func_hr, error_interface) \
	do { \
		HRESULT hr = (winapi_func_hr); \
		if (FAILED(hr)) \
		{ \
			std::stringstream ss; \
			ss << "ERROR AT LINE: " << __LINE__ << " HR: " << hr << " - DETAILS: "; \
			LPSTR error_message = nullptr; \
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
				NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error_message, 0, NULL); \
			ss << error_message << " - DX COMPILE ERROR: " << (char*)error_interface->GetBufferPointer(); \
			error_interface->Release(); \
			log_to_file(ss.str().c_str()); \
			LocalFree(error_message); \
			throw std::exception(ss.str().c_str()); \
		} \
	} while (false);

#define LOG_ADDRESS(prefix_message, address) \
	{ \
		std::stringstream ss; \
		ss << prefix_message << " 0x" << std::setw(sizeof(address) * 2) << std::setfill('0') << std::hex << (UINT_PTR)address; \
		log_to_file(ss.str().c_str()); \
	}

#else
#define LOG_ONLY_ONCE(x) // NOP, not in debug mode
#define MESSAGE_BOX_DBG(x, y) // NOP, not in debug mode
#define EXECUTE_WITH_LOG(winapi_func_hr) winapi_func_hr;
#define EXECUTE_D3DCOMPILE_WITH_LOG(winapi_func_hr, error_interface) winapi_func_hr;
#define LOG_ADDRESS(prefix_message, address) // NOP, not in debug mode
#endif


#if DEBUG_MODE == true
void print_error(const char* prefix_message)
{
	DWORD errorCode = GetLastError();
	LPSTR errorMessage = nullptr;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errorMessage, 0, nullptr);

	char message_buf[100];
	sprintf(message_buf, "%s: %s - error code: %u", prefix_message, errorMessage, errorCode);
	MESSAGE_BOX_DBG(message_buf, MB_OK | MB_ICONWARNING)
		return;
}

void log_to_file(const char* log_buf)
{
	FILE* pFile = fopen(LOG_FILE_PATH, "a");
	if (pFile == NULL)
	{
		// print_error("Error during logging"); // Comment out to prevent UI freeze when used inside hooked functions
		return;
	}
	fseek(pFile, 0, SEEK_END);
	long size = ftell(pFile);
	if (size > MAX_LOG_FILE_SIZE)
	{
		if (_chsize(_fileno(pFile), 0) == -1)
		{
			fclose(pFile);
			return;
		}
	}
	fseek(pFile, 0, SEEK_END);
	fprintf(pFile, "%s\n", log_buf);
	fclose(pFile);
}
#endif

// Find relative address utility function
void* get_relative_address(void* instruction_address, int offset, int instruction_size)
{
	int relative_offset = *(int*)((unsigned char*)instruction_address + offset);

	return (unsigned char*)instruction_address + instruction_size + relative_offset;
}

const unsigned char COverlayContext_Present_bytes[] = {
	0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xec, 0x40, 0x48, 0x8b, 0xb1, 0x20,
	0x2c, 0x00, 0x00, 0x45, 0x8b, 0xd0, 0x48, 0x8b, 0xfa, 0x48, 0x8b, 0xd9, 0x48, 0x85, 0xf6, 0x0f, 0x85
};
const int IOverlaySwapChain_IDXGISwapChain_offset = -0x118;

const unsigned char COverlayContext_IsCandidateDirectFlipCompatbile_bytes[] = {
	0x48, 0x89, 0x7c, 0x24, 0x20, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8b, 0xec, 0x48, 0x83,
	0xec, 0x40
};
const unsigned char COverlayContext_OverlaysEnabled_bytes[] = {
	0x75, 0x04, 0x32, 0xc0, 0xc3, 0xcc, 0x83, 0x79, 0x30, 0x01, 0x0f, 0x97, 0xc0, 0xc3
};

const int COverlayContext_DeviceClipBox_offset = -0x120;

const int IOverlaySwapChain_HardwareProtected_offset = -0xbc;

/*
 * AOB for function: COverlayContext_Present_bytes_w11
 *
 * 40 53 55 56 57 41 56 41 57 48 81 EC 88 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 78 48
 *
 */
const unsigned char COverlayContext_Present_bytes_w11[] = {
	0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x05,
	'?', '?', '?', '?', 0x48, 0x33, 0xC4, 0x48, 0x89, 0x44, 0x24, 0x78, 0x48
};
const int IOverlaySwapChain_IDXGISwapChain_offset_w11 = 0xE0;

/*
 * AOB for function: COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11
 *
 * 40 55 53 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 68 48
 */
const unsigned char COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11[] = {
	0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8B, 0xEC, 0x48, 0x83, 0xEC,
	0x68, 0x48,
};

/*
 * AOB for function: COverlayContext_OverlaysEnabled_bytes_w11
 *
 * 83 3D ?? ?? ?? ?? ?? 75 04
 */
const unsigned char COverlayContext_OverlaysEnabled_bytes_w11[] = {
	0x83, 0x3D, '?', '?', '?', '?', '?', 0x75, 0x04
};

int COverlayContext_DeviceClipBox_offset_w11 = 0x466C;

const int IOverlaySwapChain_HardwareProtected_offset_w11 = -0x144;

/**
 * AOB for function COverlayContext_Present_bytes_w11_24h2
 *
 * 4C 8B DC 56 41 56
 */
const unsigned char COverlayContext_Present_bytes_w11_24h2[] = {
	0x4C, 0x8B, 0xDC, 0x56, 0x41, 0x56
};

const int IOverlaySwapChain_IDXGISwapChain_offset_w11_24h2 = 0x108; // wrt OverlaySwapChain

const unsigned char COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11_24h2[] = {
	0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, '?', 0x48, 0x89, 0x68, '?', 0x48, 0x89, 0x70, '?', 0x48, 0x89, 0x78, '?', 0x41, 0x56, 0x48, 0x83, 0xEC, 0x20, 0x33, 0xDB
};

const unsigned char COverlayContext_OverlaysEnabled_bytes_relative_w11_24h2[] = {
	0xE8, '?', '?', '?', '?', 0x84, 0xC0, 0xB8, 0x04, 0x00, 0x00, 0x00
};

int COverlayContext_DeviceClipBox_offset_w11_24h2 = 0x53E8;

const int IOverlaySwapChain_HardwareProtected_offset_w11_24h2 = 0x64;

bool isWindows11 = false;
bool isWindows11_24h2 = false;

bool aob_match_inverse(const void* buf1, const void* mask, const int buf_len)
{
	for (int i = 0; i < buf_len; ++i)
	{
		if (((unsigned char*)buf1)[i] != ((unsigned char*)mask)[i] && ((unsigned char*)mask)[i] != '?')
		{
			return true;
		}
	}
	return false;
}

ID3D11Device* device;
ID3D11DeviceContext* deviceContext;
ID3D11VertexShader* vertexShader;
ID3D11PixelShader* mainPass;
ID3D11InputLayout* inputLayout;

ID3D11Buffer* vertexBuffer;
UINT numVerts;
UINT stride;
UINT offset;

D3D11_TEXTURE2D_DESC backBufferDesc;
D3D11_TEXTURE2D_DESC textureDesc[2];

ID3D11SamplerState* pointSamplerState;
ID3D11Texture2D* texture[2];
ID3D11ShaderResourceView* textureView[2];

ID3D11PixelShader* currPass;
ID3D11SamplerState* lodSamplerState;
ID3D11ShaderResourceView* currTextureView;
ID3D11RenderTargetView* currRenderTarget;

ID3D11ShaderResourceView* prevTextureView;
ID3D11RenderTargetView* prevRenderTarget;

ID3D11PixelShader* motionPass;
ID3D11ShaderResourceView* motion0TextureView;
ID3D11RenderTargetView* motion0RenderTarget;
ID3D11ShaderResourceView* motion1TextureView;
ID3D11RenderTargetView* motion1RenderTarget;
ID3D11ShaderResourceView* motion2TextureView;
ID3D11RenderTargetView* motion2RenderTarget;
ID3D11ShaderResourceView* motion3TextureView;
ID3D11RenderTargetView* motion3RenderTarget;
ID3D11ShaderResourceView* motion4TextureView;
ID3D11RenderTargetView* motion4RenderTarget;
ID3D11ShaderResourceView* motion5TextureView;
ID3D11RenderTargetView* motion5RenderTarget;
ID3D11ShaderResourceView* motion6TextureView;
ID3D11RenderTargetView* motion6RenderTarget;
ID3D11ShaderResourceView* motion7TextureView;
ID3D11RenderTargetView* motion7RenderTarget;
ID3D11ShaderResourceView* motion8TextureView;
ID3D11RenderTargetView* motion8RenderTarget;
ID3D11ShaderResourceView* motion9TextureView;
ID3D11RenderTargetView* motion9RenderTarget;
ID3D11ShaderResourceView** views[] = { &motion0TextureView, &motion1TextureView, &motion2TextureView, &motion3TextureView, &motion4TextureView, &motion5TextureView, &motion6TextureView, &motion7TextureView, &motion8TextureView, &motion9TextureView, &motion0TextureView };
ID3D11RenderTargetView** targets[] = { &motion0RenderTarget, &motion1RenderTarget, &motion2RenderTarget, &motion3RenderTarget, &motion4RenderTarget, &motion5RenderTarget, &motion6RenderTarget, &motion7RenderTarget, &motion8RenderTarget, &motion9RenderTarget };

ID3D11ComputeShader* changePass;
ID3D11ShaderResourceView* changeTextureView;
ID3D11UnorderedAccessView* changeUAV;

ID3D11PixelShader* copyMotionPass;
ID3D11ShaderResourceView* motionCopyTextureView;
ID3D11RenderTargetView* motionCopyRenderTarget;

ID3D11PixelShader* multPass;
ID3D11ShaderResourceView* multTextureView;
ID3D11RenderTargetView* multRenderTarget;
ID3D11ShaderResourceView* multCopyTextureView;
ID3D11RenderTargetView* multCopyRenderTarget;


ID3D11Buffer* constantBuffer;

int fps_multiplier = 2;
int resolution_multiplier = 1;
int monitorLeft = 0;
int monitorTop = 0;

int ctr = 0;

std::chrono::high_resolution_clock::time_point time_at = std::chrono::high_resolution_clock::now();
int fps = 0;
long long frametime = 0;

int max_mip_level = 6;

void SetVertexBuffer(struct tagRECT* rect, float texWidth, float texHeight) {
	float width = backBufferDesc.Width;
	float height = backBufferDesc.Height;

	float screenLeft = rect->left / width;
	float screenTop = rect->top / height;
	float screenRight = rect->right / width;
	float screenBottom = rect->bottom / height;

	float left = screenLeft * 2 - 1;
	float top = screenTop * -2 + 1;
	float right = screenRight * 2 - 1;
	float bottom = screenBottom * -2 + 1;

	float texLeft = rect->left / texWidth;
	float texTop = rect->top / texHeight;
	float texRight = rect->right / texWidth;
	float texBottom = rect->bottom / texHeight;

	float vertexData[] = {
		left, bottom, texLeft, texBottom,
		left, top, texLeft, texTop,
		right, bottom, texRight, texBottom,
		right, top, texRight, texTop
	};

	D3D11_MAPPED_SUBRESOURCE resource;
	EXECUTE_WITH_LOG(deviceContext->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource))
		memcpy(resource.pData, vertexData, stride * numVerts);
	deviceContext->Unmap(vertexBuffer, 0);

	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
}

void SetConstantBuffer(int constantData[], int length) {
	D3D11_MAPPED_SUBRESOURCE resource;
	EXECUTE_WITH_LOG(deviceContext->Map((ID3D11Resource*)constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
		&resource))
	memcpy(resource.pData, constantData, sizeof(int) * length);
	deviceContext->Unmap((ID3D11Resource*)constantBuffer, 0);
	deviceContext->PSSetConstantBuffers(0, 1, &constantBuffer);
}

void DrawRectangle(struct tagRECT* rect, int index)
{
	std::chrono::high_resolution_clock::time_point time_now = std::chrono::high_resolution_clock::now();
	frametime = 0.9 * frametime + 0.1 * (time_now - time_at).count();
	time_at = time_now;

	ID3D11RenderTargetView* renderTargetView;
	deviceContext->OMGetRenderTargets(1, &renderTargetView, NULL);

	// curr pass
	SetVertexBuffer(rect, min(resolution_multiplier, 2) * backBufferDesc.Width >> 1, min(resolution_multiplier, 2) * backBufferDesc.Height >> 1);
	deviceContext->PSSetShader(currPass, NULL, 0);
	deviceContext->PSSetShaderResources(0, 1, &textureView[index]);
	deviceContext->PSSetSamplers(0, 1, &lodSamplerState);
	deviceContext->OMSetRenderTargets(1, &currRenderTarget, NULL);

	int currConstantData[1] = { true };
	SetConstantBuffer(currConstantData, 1);

	deviceContext->Draw(numVerts, 0);
	deviceContext->GenerateMips(currTextureView);
	ID3D11RenderTargetView* NullRen = nullptr;
	deviceContext->OMSetRenderTargets(1, &NullRen, NULL);

	// change pass
	if (fps_multiplier != 2) {
		deviceContext->CSSetShader(changePass, NULL, 0);
		deviceContext->CSSetUnorderedAccessViews(0, 1, &changeUAV, NULL);
		deviceContext->CSSetShaderResources(0, 1, &currTextureView);
		deviceContext->CSSetShaderResources(1, 1, &prevTextureView);
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		deviceContext->ClearUnorderedAccessViewFloat(changeUAV, clearColor);
		UINT width = backBufferDesc.Width / 2;
		UINT height = backBufferDesc.Height / 2;
		const UINT threadGroupSize = 16;
		UINT numGroupsX = (width + threadGroupSize - 1) / threadGroupSize;
		UINT numGroupsY = (height + threadGroupSize - 1) / threadGroupSize;
		deviceContext->Dispatch(numGroupsX, numGroupsY, 1);

		ID3D11UnorderedAccessView* NullUav = nullptr;
		deviceContext->CSSetUnorderedAccessViews(0, 1, &NullUav, nullptr);
	}

	//mult pass
	if (fps_multiplier == 1) {
		SetVertexBuffer(rect, 1, 1);
		deviceContext->PSSetShader(multPass, NULL, 0);
		deviceContext->OMSetRenderTargets(1, &multRenderTarget, NULL);
		deviceContext->PSSetShaderResources(0, 1, &multCopyTextureView);
		deviceContext->PSSetShaderResources(1, 1, &changeTextureView);
		deviceContext->PSSetSamplers(0, 1, &lodSamplerState);
		deviceContext->Draw(numVerts, 0);
		ID3D11RenderTargetView* NullRen = nullptr;
		deviceContext->OMSetRenderTargets(1, &NullRen, NULL);

		// copy mult
		deviceContext->PSSetShader(copyMotionPass, NULL, 0);
		deviceContext->OMSetRenderTargets(1, &multCopyRenderTarget, NULL);
		deviceContext->PSSetShaderResources(0, 1, &multTextureView);
		deviceContext->PSSetSamplers(0, 1, &lodSamplerState);
		deviceContext->Draw(numVerts, 0);
	}

	// motion passes
	deviceContext->PSSetShader(motionPass, NULL, 0);
	static int frame_count = 0;
	frame_count++;
	for (int mip_level = max_mip_level; mip_level >= 0; mip_level--) {
		deviceContext->OMSetRenderTargets(1, targets[mip_level], NULL);
		deviceContext->PSSetShaderResources(0, 1, views[mip_level + 1]);
		deviceContext->PSSetSamplers(0, 1, &lodSamplerState);
		deviceContext->PSSetShaderResources(1, 1, &currTextureView);
		deviceContext->PSSetShaderResources(2, 1, &prevTextureView);
		deviceContext->PSSetShaderResources(3, 1, &changeTextureView);
		deviceContext->PSSetShaderResources(4, 1, &motionCopyTextureView);
		deviceContext->PSSetShaderResources(5, 1, &multTextureView);

		SetVertexBuffer(rect, resolution_multiplier * backBufferDesc.Width >> (3 + mip_level), resolution_multiplier * backBufferDesc.Height >> (3 + mip_level));

		int constantData[3] = { mip_level, frame_count, fps_multiplier };
		SetConstantBuffer(constantData, 3);

		deviceContext->Draw(numVerts, 0);
	}

	// copy motion pass
	if (fps_multiplier != 2) {
		deviceContext->PSSetShader(copyMotionPass, NULL, 0);
		deviceContext->OMSetRenderTargets(1, &motionCopyRenderTarget, NULL);
		deviceContext->PSSetShaderResources(0, 1, views[0]);
		deviceContext->PSSetSamplers(0, 1, &lodSamplerState);

		SetVertexBuffer(rect, resolution_multiplier * backBufferDesc.Width >> 3, resolution_multiplier * backBufferDesc.Height >> 3);
		deviceContext->Draw(numVerts, 0);
	}

	// main pass
	SetVertexBuffer(rect, backBufferDesc.Width, backBufferDesc.Height);

	deviceContext->OMSetRenderTargets(1, &renderTargetView, NULL);
	renderTargetView->Release();

	deviceContext->PSSetShaderResources(0, 1, &textureView[index]);
	deviceContext->PSSetSamplers(0, 1, &lodSamplerState);
	deviceContext->PSSetSamplers(1, 1, &pointSamplerState);
	deviceContext->PSSetShaderResources(1, 1, &prevTextureView);
	deviceContext->PSSetShaderResources(2, 1, views[0]);
	deviceContext->PSSetShaderResources(3, 1, &multTextureView);
	deviceContext->PSSetShader(mainPass, NULL, 0);

	// ctrl+shift+alt for debug mode
	bool debugMode = GetKeyState(VK_CONTROL) & 0x8000 && GetKeyState(VK_SHIFT) & 0x8000 && GetKeyState(VK_MENU) & 0x8000;
	int constantData[3] = { frametime, debugMode, fps_multiplier };
	SetConstantBuffer(constantData, 3);

	deviceContext->Draw(numVerts, 0);

	// prev pass (just curr pass but reading from curr texture instead of backbuffer)
	SetVertexBuffer(rect, min(resolution_multiplier, 2) * backBufferDesc.Width >> 1, min(resolution_multiplier, 2) * backBufferDesc.Height >> 1);
	deviceContext->PSSetShader(currPass, NULL, 0);
	deviceContext->PSSetShaderResources(0, 1, &currTextureView);
	deviceContext->PSSetSamplers(0, 1, &lodSamplerState);
	deviceContext->OMSetRenderTargets(1, &prevRenderTarget, NULL);

	int prevConstantData[1] = { false };
	SetConstantBuffer(prevConstantData, 1);

	deviceContext->Draw(numVerts, 0);
	deviceContext->GenerateMips(prevTextureView);

}

void InitializeStuff(IDXGISwapChain* swapChain)
{
	try
	{
		EXECUTE_WITH_LOG(swapChain->GetDevice(IID_ID3D11Device, (void**)&device))
			LOG_ADDRESS("Current swapchain address is: ", swapChain)
			LOG_ONLY_ONCE("Device successfully gathered")
			LOG_ADDRESS("The device address is: ", device)

			device->GetImmediateContext(&deviceContext);
		LOG_ONLY_ONCE("Got context after device")
			LOG_ADDRESS("The Device context is located at address: ", deviceContext)
		{
			ID3DBlob* vsBlob;
			ID3DBlob* compile_error_interface;
			LOG_ONLY_ONCE(("Trying to compile vshader with this code:\n" + std::string(vertex_shader)).c_str())
				EXECUTE_D3DCOMPILE_WITH_LOG(
					D3DCompile(vertex_shader, sizeof vertex_shader, NULL, NULL, NULL, "VS", "vs_5_0", 0, 0, &vsBlob, &
						compile_error_interface), compile_error_interface)


				LOG_ONLY_ONCE("Vertex shader compiled successfully")
				EXECUTE_WITH_LOG(device->CreateVertexShader(vsBlob->GetBufferPointer(),
					vsBlob->GetBufferSize(), NULL, &vertexShader))


				LOG_ONLY_ONCE("Vertex shader created successfully")
				D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{
					"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
					D3D11_INPUT_PER_VERTEX_DATA, 0
				}
			};
			EXECUTE_WITH_LOG(device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc),
				vsBlob->GetBufferPointer(),
				vsBlob->GetBufferSize(), &inputLayout))

				vsBlob->Release();
		}
		{
			ID3DBlob* psBlob;
			ID3DBlob* compile_error_interface;
			EXECUTE_D3DCOMPILE_WITH_LOG(
				D3DCompile(main_pass, sizeof main_pass, NULL, NULL, NULL, "PS", "ps_5_0", 0, 0, &psBlob, &
					compile_error_interface), compile_error_interface)

				LOG_ONLY_ONCE("Pixel shader compiled successfully")
				device->CreatePixelShader(psBlob->GetBufferPointer(),
					psBlob->GetBufferSize(), NULL, &mainPass);
			psBlob->Release();
		}
		{
			stride = 4 * sizeof(float);
			numVerts = 4;
			offset = 0;

			D3D11_BUFFER_DESC vertexBufferDesc = {};
			vertexBufferDesc.ByteWidth = stride * numVerts;
			vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
			vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			EXECUTE_WITH_LOG(device->CreateBuffer(&vertexBufferDesc, NULL, &vertexBuffer))
		}
		{
			D3D11_SAMPLER_DESC samplerDesc = {};
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
			samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

			EXECUTE_WITH_LOG(device->CreateSamplerState(&samplerDesc, &pointSamplerState))
		}
		{
			ID3DBlob* psBlob;
			ID3DBlob* compile_error_interface;
			EXECUTE_D3DCOMPILE_WITH_LOG(
				D3DCompile(curr_pass, sizeof curr_pass, NULL, NULL, NULL, "PS", "ps_5_0", 0, 0, &psBlob, &
					compile_error_interface), compile_error_interface)

				LOG_ONLY_ONCE("Pixel shader compiled successfully")
				device->CreatePixelShader(psBlob->GetBufferPointer(),
					psBlob->GetBufferSize(), NULL, &currPass);
			psBlob->Release();
		}
		{
			ID3DBlob* psBlob;
			ID3DBlob* compile_error_interface;
			EXECUTE_D3DCOMPILE_WITH_LOG(
				D3DCompile(motion_pass, sizeof motion_pass, NULL, NULL, NULL, "PS", "ps_5_0", 0, 0, &psBlob, &
					compile_error_interface), compile_error_interface)

				LOG_ONLY_ONCE("Pixel shader compiled successfully")
				device->CreatePixelShader(psBlob->GetBufferPointer(),
					psBlob->GetBufferSize(), NULL, &motionPass);
			psBlob->Release();
		}
		{
			ID3DBlob* csBlob;
			ID3DBlob* compile_error_interface;
			EXECUTE_D3DCOMPILE_WITH_LOG(
				D3DCompile(change_pass, sizeof change_pass, NULL, NULL, NULL, "CS", "cs_5_0", 0, 0, &csBlob, &
					compile_error_interface), compile_error_interface)

				LOG_ONLY_ONCE("Compute shader compiled successfully")
				device->CreateComputeShader(csBlob->GetBufferPointer(),
					csBlob->GetBufferSize(), NULL, &changePass);
			csBlob->Release();
		}
		{
			ID3DBlob* psBlob;
			ID3DBlob* compile_error_interface;
			EXECUTE_D3DCOMPILE_WITH_LOG(
				D3DCompile(copy_motion_pass, sizeof copy_motion_pass, NULL, NULL, NULL, "PS", "ps_5_0", 0, 0, &psBlob, &
					compile_error_interface), compile_error_interface)

				LOG_ONLY_ONCE("Pixel shader compiled successfully")
				device->CreatePixelShader(psBlob->GetBufferPointer(),
					psBlob->GetBufferSize(), NULL, &copyMotionPass);
			psBlob->Release();
		}
		{
			ID3DBlob* psBlob;
			ID3DBlob* compile_error_interface;
			EXECUTE_D3DCOMPILE_WITH_LOG(
				D3DCompile(mult_pass, sizeof mult_pass, NULL, NULL, NULL, "PS", "ps_5_0", 0, 0, &psBlob, &
					compile_error_interface), compile_error_interface)

				LOG_ONLY_ONCE("Pixel shader compiled successfully")
				device->CreatePixelShader(psBlob->GetBufferPointer(),
					psBlob->GetBufferSize(), NULL, &multPass);
			psBlob->Release();
		}
		{
			D3D11_SAMPLER_DESC samplerDesc = {};
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
			samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

			EXECUTE_WITH_LOG(device->CreateSamplerState(&samplerDesc, &lodSamplerState))
		}
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = min(resolution_multiplier, 2) * backBufferDesc.Width >> 1;
			desc.Height = min(resolution_multiplier, 2) * backBufferDesc.Height >> 1;
			desc.MipLevels = 0;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

			ID3D11Texture2D* tex;
			EXECUTE_WITH_LOG(device->CreateTexture2D(&desc, NULL, &tex))
				EXECUTE_WITH_LOG(device->CreateShaderResourceView((ID3D11Resource*)tex, NULL, &currTextureView))
				EXECUTE_WITH_LOG(device->CreateRenderTargetView((ID3D11Resource*)tex, NULL, &currRenderTarget))
				tex->Release();
		}
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = min(resolution_multiplier, 2) * backBufferDesc.Width >> 1;
			desc.Height = min(resolution_multiplier, 2) * backBufferDesc.Height >> 1;
			desc.MipLevels = 0;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

			ID3D11Texture2D* tex;
			EXECUTE_WITH_LOG(device->CreateTexture2D(&desc, NULL, &tex))
				EXECUTE_WITH_LOG(device->CreateShaderResourceView((ID3D11Resource*)tex, NULL, &prevTextureView))
				EXECUTE_WITH_LOG(device->CreateRenderTargetView((ID3D11Resource*)tex, NULL, &prevRenderTarget))
				tex->Release();
		}
		{
			for (int i = 0; i <= max_mip_level; i++) {
				D3D11_TEXTURE2D_DESC desc = {};
				desc.Width = resolution_multiplier * backBufferDesc.Width >> (3 + i);
				desc.Height = resolution_multiplier * backBufferDesc.Height >> (3 + i);
				desc.MipLevels = 0;
				desc.ArraySize = 1;
				desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				desc.SampleDesc.Count = 1;
				desc.Usage = D3D11_USAGE_DEFAULT;
				desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;
				desc.MiscFlags = 0;

				ID3D11Texture2D* tex;
				EXECUTE_WITH_LOG(device->CreateTexture2D(&desc, NULL, &tex))
					EXECUTE_WITH_LOG(device->CreateShaderResourceView((ID3D11Resource*)tex, NULL, views[i]))
					EXECUTE_WITH_LOG(device->CreateRenderTargetView((ID3D11Resource*)tex, NULL, targets[i]))
					tex->Release();
			}
		}
		{
			D3D11_BUFFER_DESC constantBufferDesc = {};
			constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			constantBufferDesc.ByteWidth = 16;  // Constant buffer size must be multiple of 16
			constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
			constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			EXECUTE_WITH_LOG(device->CreateBuffer(&constantBufferDesc, NULL, &constantBuffer))
		}
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = 1;
			desc.Height = 1;
			desc.MipLevels = 0;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			ID3D11Texture2D* tex;
			EXECUTE_WITH_LOG(device->CreateTexture2D(&desc, NULL, &tex))
				EXECUTE_WITH_LOG(device->CreateShaderResourceView((ID3D11Resource*)tex, NULL, &changeTextureView))
				EXECUTE_WITH_LOG(device->CreateUnorderedAccessView((ID3D11Resource*)tex, NULL, &changeUAV))
				tex->Release();
		}
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = resolution_multiplier * backBufferDesc.Width >> 3;
			desc.Height = resolution_multiplier * backBufferDesc.Height >> 3;
			desc.MipLevels = 0;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			ID3D11Texture2D* tex;
			EXECUTE_WITH_LOG(device->CreateTexture2D(&desc, NULL, &tex))
				EXECUTE_WITH_LOG(device->CreateShaderResourceView((ID3D11Resource*)tex, NULL, &motionCopyTextureView))
				EXECUTE_WITH_LOG(device->CreateRenderTargetView((ID3D11Resource*)tex, NULL, &motionCopyRenderTarget))
				tex->Release();
		}
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = 1;
			desc.Height = 1;
			desc.MipLevels = 0;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R32G32_FLOAT;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			ID3D11Texture2D* tex;
			EXECUTE_WITH_LOG(device->CreateTexture2D(&desc, NULL, &tex))
				EXECUTE_WITH_LOG(device->CreateShaderResourceView((ID3D11Resource*)tex, NULL, &multTextureView))
				EXECUTE_WITH_LOG(device->CreateRenderTargetView((ID3D11Resource*)tex, NULL, &multRenderTarget))
				tex->Release();
			EXECUTE_WITH_LOG(device->CreateTexture2D(&desc, NULL, &tex))
				EXECUTE_WITH_LOG(device->CreateShaderResourceView((ID3D11Resource*)tex, NULL, &multCopyTextureView))
				EXECUTE_WITH_LOG(device->CreateRenderTargetView((ID3D11Resource*)tex, NULL, &multCopyRenderTarget))
				tex->Release();
		}
	}
	catch (std::exception& ex)
	{
		std::stringstream ex_message;
		ex_message << "Exception caught at line " << __LINE__ << ": " << ex.what() << std::endl;
		LOG_ONLY_ONCE(ex_message.str().c_str())
			throw;
	}
	catch (...)
	{
		std::stringstream ex_message;
		ex_message << "Exception caught at line " << __LINE__ << ": " << std::endl;
		LOG_ONLY_ONCE(ex_message.str().c_str())
			throw;
	}
}

void UninitializeStuff()
{
	RELEASE_IF_NOT_NULL(device)
		RELEASE_IF_NOT_NULL(deviceContext)
		RELEASE_IF_NOT_NULL(vertexShader)
		RELEASE_IF_NOT_NULL(mainPass)
		RELEASE_IF_NOT_NULL(inputLayout)
		RELEASE_IF_NOT_NULL(vertexBuffer)
		RELEASE_IF_NOT_NULL(pointSamplerState)
		for (int i = 0; i < 2; i++)
		{
			RELEASE_IF_NOT_NULL(texture[i])
				RELEASE_IF_NOT_NULL(textureView[i])
		}
	RELEASE_IF_NOT_NULL(currPass)
		RELEASE_IF_NOT_NULL(lodSamplerState)
		RELEASE_IF_NOT_NULL(currTextureView)
		RELEASE_IF_NOT_NULL(currRenderTarget)
		RELEASE_IF_NOT_NULL(prevTextureView)
		RELEASE_IF_NOT_NULL(prevRenderTarget)
		RELEASE_IF_NOT_NULL(constantBuffer)
		for (int i = 0; i <= 9; i++) {
			RELEASE_IF_NOT_NULL((*views[i]))
				RELEASE_IF_NOT_NULL((*targets[i]))
		}
}

bool ApplyLUT(void* cOverlayContext, IDXGISwapChain* swapChain, struct tagRECT* rects, int numRects)
{
	try
	{
		// Only change config on re-apply
		if (!device) {
			HKEY hKey;
			LPCSTR subKey = "Software\\DwmFrameInterpolator";
			LPCSTR fpsValueName = "FpsMultiplier";
			DWORD fpsValue = 0;
			DWORD fpsValueSize = sizeof(fpsValue);
			LPCSTR resValueName = "ResolutionMultiplier";
			DWORD resValue = 0;
			DWORD resValueSize = sizeof(resValue);
			LPCSTR leftValueName = "MonitorLeft";
			DWORD leftValue = 0;
			DWORD leftValueSize = sizeof(leftValue);
			LPCSTR topValueName = "MonitorTop";
			DWORD topValue = 0;
			DWORD topValueSize = sizeof(topValue);
			if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
			{
				// Retrieve the value using RegGetValueA
				if (RegGetValueA(hKey, nullptr, fpsValueName, RRF_RT_REG_DWORD, NULL, &fpsValue, &fpsValueSize) == ERROR_SUCCESS)
				{
					fps_multiplier = fpsValue;
				}
				if (RegGetValueA(hKey, nullptr, resValueName, RRF_RT_REG_DWORD, NULL, &resValue, &resValueSize) == ERROR_SUCCESS)
				{
					resolution_multiplier = resValue;
				}
				if (RegGetValueA(hKey, nullptr, leftValueName, RRF_RT_REG_DWORD, NULL, &leftValue, &leftValueSize) == ERROR_SUCCESS) {
					monitorLeft = leftValue;
				}
				if (RegGetValueA(hKey, nullptr, topValueName, RRF_RT_REG_DWORD, NULL, &topValue, &topValueSize) == ERROR_SUCCESS) {
					monitorTop = topValue;
				}
				RegCloseKey(hKey);
			}
		}

		int left, top;
		if (isWindows11_24h2)
		{
			float* rect = (float*)((unsigned char*)*(void**)cOverlayContext + COverlayContext_DeviceClipBox_offset_w11_24h2);
			left = (int)rect[2];
			top = (int)rect[3];
		}
		else if (isWindows11)
		{
			float* rect = (float*)((unsigned char*)*(void**)cOverlayContext + COverlayContext_DeviceClipBox_offset_w11);
			left = (int)rect[0];
			top = (int)rect[1];
		}
		else
		{
			int* rect = (int*)((unsigned char*)cOverlayContext + COverlayContext_DeviceClipBox_offset);
			left = rect[0];
			top = rect[1];
		}

		if (left != monitorLeft || top != monitorTop) {
			return true;
		}

		ID3D11Texture2D* backBuffer;
		ID3D11RenderTargetView* renderTargetView;


		EXECUTE_WITH_LOG(swapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backBuffer))

			backBuffer->GetDesc(&backBufferDesc);

		if (!device)
		{
			max_mip_level = min(9, floor(log2(backBufferDesc.Height * resolution_multiplier)) - 3);
			LOG_ONLY_ONCE("Initializing stuff in ApplyLUT")
				InitializeStuff(swapChain);
		}
		LOG_ONLY_ONCE("Init done, continuing with LUT application")

			int index = -1;
		if (backBufferDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM)
		{
			index = 0;
		}
		else if (backBufferDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
		{
			index = 1;
		}

		D3D11_TEXTURE2D_DESC oldTextureDesc = textureDesc[index];
		if (backBufferDesc.Width != oldTextureDesc.Width || backBufferDesc.Height != oldTextureDesc.Height)
		{
			if (texture[index] != NULL)
			{
				texture[index]->Release();
				textureView[index]->Release();
			}

			UINT newWidth = backBufferDesc.Width;
			UINT newHeight = backBufferDesc.Height;

			D3D11_TEXTURE2D_DESC newTextureDesc;

			newTextureDesc = backBufferDesc;
			newTextureDesc.Width = newWidth;
			newTextureDesc.Height = newHeight;
			newTextureDesc.Usage = D3D11_USAGE_DEFAULT;
			newTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			newTextureDesc.CPUAccessFlags = 0;
			newTextureDesc.MiscFlags = 0;

			textureDesc[index] = newTextureDesc;

			EXECUTE_WITH_LOG(device->CreateTexture2D(&textureDesc[index], NULL, &texture[index]))
				EXECUTE_WITH_LOG(
					device->CreateShaderResourceView((ID3D11Resource*)texture[index], NULL, &textureView[index]))
		}

		EXECUTE_WITH_LOG(device->CreateRenderTargetView((ID3D11Resource*)backBuffer, NULL, &renderTargetView))
			const D3D11_VIEWPORT d3d11_viewport(0, 0, backBufferDesc.Width, backBufferDesc.Height, 0.0f, 1.0f);
		deviceContext->RSSetViewports(1, &d3d11_viewport);

		deviceContext->OMSetRenderTargets(1, &renderTargetView, NULL);
		renderTargetView->Release();

		deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		deviceContext->IASetInputLayout(inputLayout);

		deviceContext->VSSetShader(vertexShader, NULL, 0);

		for (int i = 0; i < numRects; i++)
		{
			D3D11_BOX sourceRegion;
			sourceRegion.left = rects[i].left;
			sourceRegion.right = rects[i].right;
			sourceRegion.top = rects[i].top;
			sourceRegion.bottom = rects[i].bottom;
			sourceRegion.front = 0;
			sourceRegion.back = 1;

			deviceContext->CopySubresourceRegion((ID3D11Resource*)texture[index], 0, rects[i].left,
				rects[i].top, 0, (ID3D11Resource*)backBuffer, 0, &sourceRegion);
			DrawRectangle(&rects[i], index);
		}

		backBuffer->Release();
		return true;
	}
	catch (std::exception& ex)
	{
		std::stringstream ex_message;
		ex_message << "Exception caught at line " << __LINE__ << ": " << ex.what() << std::endl;
		LOG_ONLY_ONCE(ex_message.str().c_str())
			return false;
	}
	catch (...)
	{
		std::stringstream ex_message;
		ex_message << "Exception caught at line " << __LINE__ << std::endl;
		LOG_ONLY_ONCE(ex_message.str().c_str())
			return false;
	}
}

typedef struct rectVec
{
	struct tagRECT* start;
	struct tagRECT* end;
	struct tagRECT* cap;
} rectVec;

typedef long (COverlayContext_Present_t)(void*, void*, unsigned int, rectVec*, unsigned int, bool);
typedef long long (COverlayContext_Present_24h2_t)(void*, void*, unsigned int, rectVec*, int, void*, bool);

COverlayContext_Present_t* COverlayContext_Present_orig = NULL;
COverlayContext_Present_t* COverlayContext_Present_real_orig = NULL;

COverlayContext_Present_24h2_t* COverlayContext_Present_orig_24h2 = NULL;
COverlayContext_Present_24h2_t* COverlayContext_Present_real_orig_24h2 = NULL;

long long COverlayContext_Present_hook_24h2(void* self, void* overlaySwapChain, unsigned int a3, rectVec* rectVec,
	int a5, void* a6, bool a7)
{
	if (_ReturnAddress() < (void*)COverlayContext_Present_real_orig_24h2 || isWindows11_24h2)
	{
		LOG_ONLY_ONCE("I am inside COverlayContext::Present hook inside the main if condition")
			std::stringstream overlay_swapchain_message;
		overlay_swapchain_message << "OverlaySwapChain address: 0x" << std::hex << overlaySwapChain << " -- windows 11 24h2: " << isWindows11_24h2
			<< " -- " << "windows 11: " << isWindows11;
		LOG_ONLY_ONCE(overlay_swapchain_message.str().c_str())
			if ((isWindows11_24h2 && *((bool*)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11_24h2)) ||
				(isWindows11 && *((bool*)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11)) ||
				(!(isWindows11 || isWindows11_24h2) && *((bool*)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset)))
			{
				std::stringstream hw_protection_message;
				hw_protection_message << "I'm inside the Hardware protection condition - 0x" << std::hex << (bool*)
					overlaySwapChain + (!isWindows11_24h2 ? (!isWindows11 ? IOverlaySwapChain_HardwareProtected_offset : IOverlaySwapChain_HardwareProtected_offset_w11) : IOverlaySwapChain_HardwareProtected_offset_w11_24h2) << " - value: 0x" << *((bool*)
						overlaySwapChain + (!isWindows11_24h2 ? (!isWindows11 ? IOverlaySwapChain_HardwareProtected_offset : IOverlaySwapChain_HardwareProtected_offset_w11) : IOverlaySwapChain_HardwareProtected_offset_w11_24h2));
				LOG_ONLY_ONCE(hw_protection_message.str().c_str())
			}
			else
			{
				std::stringstream hw_protection_message;
				hw_protection_message << "I'm outside the Hardware protection condition - 0x" << std::hex << (bool*)
					overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11 << " - value: 0x" << *((bool*)
						overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11);
				LOG_ONLY_ONCE(hw_protection_message.str().c_str())

					IDXGISwapChain* swapChain;

				if (isWindows11_24h2)
				{
					LOG_ONLY_ONCE("Gathering IDXGISwapChain pointer")

						swapChain = *(IDXGISwapChain**)((unsigned char*)overlaySwapChain +
							IOverlaySwapChain_IDXGISwapChain_offset_w11_24h2);

				}
				else if (isWindows11)
				{
					LOG_ONLY_ONCE("Gathering IDXGISwapChain pointer")
						int sub_from_legacy_swapchain = *(int*)((unsigned char*)overlaySwapChain - 4);
					void* real_overlay_swap_chain = (unsigned char*)overlaySwapChain - sub_from_legacy_swapchain -
						0x1b0;
					swapChain = *(IDXGISwapChain**)((unsigned char*)real_overlay_swap_chain +
						IOverlaySwapChain_IDXGISwapChain_offset_w11);
				}
				else
				{
					swapChain = *(IDXGISwapChain**)((unsigned char*)overlaySwapChain +
						IOverlaySwapChain_IDXGISwapChain_offset);
				}

				if (ApplyLUT(self, swapChain, rectVec->start, rectVec->end - rectVec->start))
				{
					LOG_ONLY_ONCE("Setting LUTactive")
				}
				else
				{
					LOG_ONLY_ONCE("Un-setting LUTactive")
				}
			}
	}
	return COverlayContext_Present_orig_24h2(self, overlaySwapChain, a3, rectVec, a5, a6, a7);
}


long COverlayContext_Present_hook(void* self, void* overlaySwapChain, unsigned int a3, rectVec* rectVec,
	unsigned int a5, bool a6)
{
	if (_ReturnAddress() < (void*)COverlayContext_Present_real_orig)
	{
		LOG_ONLY_ONCE("I am inside COverlayContext::Present hook inside the main if condition")

			if ((isWindows11_24h2 && *((bool*)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11_24h2)) ||
				(isWindows11 && *((bool*)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11)) ||
				(!(isWindows11 || isWindows11_24h2) && *((bool*)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset)))
			{
				std::stringstream hw_protection_message;
				hw_protection_message << "I'm inside the Hardware protection condition - 0x" << std::hex << (bool*)
					overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11 << " - value: 0x" << *((bool*)
						overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11);
				LOG_ONLY_ONCE(hw_protection_message.str().c_str())
			}
			else
			{
				std::stringstream hw_protection_message;
				hw_protection_message << "I'm outside the Hardware protection condition - 0x" << std::hex << (bool*)
					overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11 << " - value: 0x" << *((bool*)
						overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11);
				LOG_ONLY_ONCE(hw_protection_message.str().c_str())

					IDXGISwapChain* swapChain;
				if (isWindows11_24h2)
				{
					LOG_ONLY_ONCE("Gathering IDXGISwapChain pointer")

						swapChain = *(IDXGISwapChain**)((unsigned char*)overlaySwapChain +
							IOverlaySwapChain_IDXGISwapChain_offset_w11_24h2);

				}
				else if (isWindows11)
				{
					LOG_ONLY_ONCE("Gathering IDXGISwapChain pointer")
						int sub_from_legacy_swapchain = *(int*)((unsigned char*)overlaySwapChain - 4);
					void* real_overlay_swap_chain = (unsigned char*)overlaySwapChain - sub_from_legacy_swapchain -
						0x1b0;
					swapChain = *(IDXGISwapChain**)((unsigned char*)real_overlay_swap_chain +
						IOverlaySwapChain_IDXGISwapChain_offset_w11);
				}
				else
				{
					swapChain = *(IDXGISwapChain**)((unsigned char*)overlaySwapChain +
						IOverlaySwapChain_IDXGISwapChain_offset);
				}

				if (ApplyLUT(self, swapChain, rectVec->start, rectVec->end - rectVec->start))
				{
					LOG_ONLY_ONCE("Setting LUTactive")
				}
				else
				{
					LOG_ONLY_ONCE("Un-setting LUTactive")
				}
			}
	}

	return COverlayContext_Present_orig(self, overlaySwapChain, a3, rectVec, a5, a6);
}

typedef bool (COverlayContext_IsCandidateDirectFlipCompatbile_t)(void*, void*, void*, void*, int, unsigned int, bool,
	bool);
typedef bool (COverlayContext_IsCandidateDirectFlipCompatbile_24h2_t)(void*, void*, void*, void*, unsigned int, bool);

COverlayContext_IsCandidateDirectFlipCompatbile_t* COverlayContext_IsCandidateDirectFlipCompatbile_orig;
COverlayContext_IsCandidateDirectFlipCompatbile_24h2_t* COverlayContext_IsCandidateDirectFlipCompatbile_orig_24h2;

bool COverlayContext_IsCandidateDirectFlipCompatbile_hook_24h2(void* self, void* a2, void* a3, void* a4, unsigned int a5,
	bool a6)
{
	return COverlayContext_IsCandidateDirectFlipCompatbile_orig_24h2(self, a2, a3, a4, a5, a6);
}

bool COverlayContext_IsCandidateDirectFlipCompatbile_hook(void* self, void* a2, void* a3, void* a4, int a5,
	unsigned int a6, bool a7, bool a8)
{
	return COverlayContext_IsCandidateDirectFlipCompatbile_orig(self, a2, a3, a4, a5, a6, a7, a8);
}

typedef bool (COverlayContext_OverlaysEnabled_t)(void*);

COverlayContext_OverlaysEnabled_t* COverlayContext_OverlaysEnabled_orig = NULL;

bool COverlayContext_OverlaysEnabled_hook(void* self)
{
	return COverlayContext_OverlaysEnabled_orig(self);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
	{
		HMODULE dwmcore = GetModuleHandle(L"dwmcore.dll");
		MODULEINFO moduleInfo;
		GetModuleInformation(GetCurrentProcess(), dwmcore, &moduleInfo, sizeof moduleInfo);

		OSVERSIONINFOEX versionInfo;
		ZeroMemory(&versionInfo, sizeof OSVERSIONINFOEX);
		versionInfo.dwOSVersionInfoSize = sizeof OSVERSIONINFOEX;
		versionInfo.dwBuildNumber = 22000;

		// Version info for windows 11 24h2
		OSVERSIONINFOEX versionInfo24h2;
		ZeroMemory(&versionInfo24h2, sizeof OSVERSIONINFOEX);
		versionInfo24h2.dwOSVersionInfoSize = sizeof OSVERSIONINFOEX;
		versionInfo24h2.dwBuildNumber = 26100;

		ULONGLONG dwlConditionMask = 0;
		VER_SET_CONDITION(dwlConditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

		if (VerifyVersionInfo(&versionInfo24h2, VER_BUILDNUMBER, dwlConditionMask))
		{
			isWindows11_24h2 = true;
		}
		else if (VerifyVersionInfo(&versionInfo, VER_BUILDNUMBER, dwlConditionMask))
		{
			isWindows11 = true;
		}
		else
		{
			isWindows11 = false;
		}

		// TODO: Remove this debug instruction
		//MESSAGE_BOX_DBG("DWM LUT ATTACH", MB_OK)

		if (isWindows11_24h2)
		{

				for (size_t i = 0; i <= moduleInfo.SizeOfImage - sizeof COverlayContext_OverlaysEnabled_bytes_relative_w11_24h2; i++)
				{
					unsigned char* address = (unsigned char*)dwmcore + i;
					if (!COverlayContext_Present_orig && sizeof COverlayContext_Present_bytes_w11_24h2 <= moduleInfo.
						SizeOfImage - i && !aob_match_inverse(address, COverlayContext_Present_bytes_w11_24h2,
							sizeof COverlayContext_Present_bytes_w11_24h2))
					{
							COverlayContext_Present_orig_24h2 = (COverlayContext_Present_24h2_t*)address;
						COverlayContext_Present_real_orig_24h2 = COverlayContext_Present_orig_24h2;
					}
					else if (!COverlayContext_IsCandidateDirectFlipCompatbile_orig && sizeof
						COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11_24h2 <= moduleInfo.SizeOfImage - i && !
						aob_match_inverse(
							address, COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11_24h2,
							sizeof COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11_24h2))
					{
						COverlayContext_IsCandidateDirectFlipCompatbile_orig_24h2 = (
							COverlayContext_IsCandidateDirectFlipCompatbile_24h2_t*)address;
					}
					else if (!COverlayContext_OverlaysEnabled_orig && sizeof COverlayContext_OverlaysEnabled_bytes_relative_w11_24h2
						<= moduleInfo.SizeOfImage - i && !aob_match_inverse(
							address, COverlayContext_OverlaysEnabled_bytes_relative_w11_24h2,
							sizeof COverlayContext_OverlaysEnabled_bytes_relative_w11_24h2))
					{


						COverlayContext_OverlaysEnabled_orig = (COverlayContext_OverlaysEnabled_t*)get_relative_address(address, 1, 5);
					}
					if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
						COverlayContext_OverlaysEnabled_orig)
					{

							break;
					}
				}
		}
		else if (isWindows11)
		{
			// TODO: Remove this debug instruction
			//MESSAGE_BOX_DBG("DETECTED WINDOWS 11 OS", MB_OK)

			for (size_t i = 0; i <= moduleInfo.SizeOfImage - sizeof COverlayContext_OverlaysEnabled_bytes_w11; i++)
			{
				unsigned char* address = (unsigned char*)dwmcore + i;
				if (!COverlayContext_Present_orig && sizeof COverlayContext_Present_bytes_w11 <= moduleInfo.
					SizeOfImage - i && !aob_match_inverse(address, COverlayContext_Present_bytes_w11,
						sizeof COverlayContext_Present_bytes_w11))
				{
					// TODO: Remove this debug instruction
					//MESSAGE_BOX_DBG("DETECTED COverlayContextPresent address", MB_OK)

					COverlayContext_Present_orig = (COverlayContext_Present_t*)address;
					COverlayContext_Present_real_orig = COverlayContext_Present_orig;
				}
				else if (!COverlayContext_IsCandidateDirectFlipCompatbile_orig && sizeof
					COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11 <= moduleInfo.SizeOfImage - i && !
					aob_match_inverse(
						address, COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11,
						sizeof COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11))
				{
					COverlayContext_IsCandidateDirectFlipCompatbile_orig = (
						COverlayContext_IsCandidateDirectFlipCompatbile_t*)address;
				}
				else if (!COverlayContext_OverlaysEnabled_orig && sizeof COverlayContext_OverlaysEnabled_bytes_w11
					<= moduleInfo.SizeOfImage - i && !aob_match_inverse(
						address, COverlayContext_OverlaysEnabled_bytes_w11,
						sizeof COverlayContext_OverlaysEnabled_bytes_w11))
				{
					COverlayContext_OverlaysEnabled_orig = (COverlayContext_OverlaysEnabled_t*)address;
				}
				if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
					COverlayContext_OverlaysEnabled_orig)
				{
					//MESSAGE_BOX_DBG("All addresses successfully retrieved", MB_OK)

					break;
				}
			}

			DWORD rev;
			DWORD revSize = sizeof(rev);
			RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "UBR", RRF_RT_DWORD,
				NULL, &rev, &revSize);

			if (rev >= 706)
			{
				//MESSAGE_BOX_DBG("Detected recent Windows OS", MB_OK)

				// COverlayContext_DeviceClipBox_offset_w11 += 8;
			}
		}
		else
		{
			for (size_t i = 0; i <= moduleInfo.SizeOfImage - sizeof(COverlayContext_Present_bytes); i++)
			{
				unsigned char* address = (unsigned char*)dwmcore + i;
				if (!COverlayContext_Present_orig && !memcmp(address, COverlayContext_Present_bytes,
					sizeof(COverlayContext_Present_bytes)))
				{
					COverlayContext_Present_orig = (COverlayContext_Present_t*)address;
					COverlayContext_Present_real_orig = COverlayContext_Present_orig;
				}
				else if (!COverlayContext_IsCandidateDirectFlipCompatbile_orig && !memcmp(
					address, COverlayContext_IsCandidateDirectFlipCompatbile_bytes,
					sizeof(COverlayContext_IsCandidateDirectFlipCompatbile_bytes)))
				{
					static int found = 0;
					found++;
					if (found == 2)
					{
						COverlayContext_IsCandidateDirectFlipCompatbile_orig = (
							COverlayContext_IsCandidateDirectFlipCompatbile_t*)(address - 0xa);
					}
				}
				else if (!COverlayContext_OverlaysEnabled_orig && !memcmp(
					address, COverlayContext_OverlaysEnabled_bytes, sizeof(COverlayContext_OverlaysEnabled_bytes)))
				{
					COverlayContext_OverlaysEnabled_orig = (COverlayContext_OverlaysEnabled_t*)(address - 0x7);
				}
				if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
					COverlayContext_OverlaysEnabled_orig)
				{
					break;
				}
			}
		}

		char lutFolderPath[MAX_PATH];
		char variable_message_states[300];
		if (!isWindows11_24h2) {
			sprintf(variable_message_states, "Current variable states: COverlayContext::Present - %p\t"
				"COverlayContext::IsCandidateDirectFlipCompatible - %p\tCOverlayContext::OverlaysEnabled - %p",
				COverlayContext_Present_orig,
				COverlayContext_IsCandidateDirectFlipCompatbile_orig, COverlayContext_OverlaysEnabled_orig);
		}
		else {
			sprintf(variable_message_states, "Current variable states: COverlayContext::Present - %p\t"
				"COverlayContext::IsCandidateDirectFlipCompatible - %p\tCOverlayContext::OverlaysEnabled - %p",
				COverlayContext_Present_orig_24h2,
				COverlayContext_IsCandidateDirectFlipCompatbile_orig_24h2, COverlayContext_OverlaysEnabled_orig);
		}

		//MESSAGE_BOX_DBG(variable_message_states, MB_OK)

		if ((COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
			COverlayContext_OverlaysEnabled_orig) ||
			(COverlayContext_Present_orig_24h2 && COverlayContext_IsCandidateDirectFlipCompatbile_orig_24h2 && COverlayContext_OverlaysEnabled_orig))
		{
			MH_Initialize();
			if (!isWindows11_24h2)
				MH_CreateHook((PVOID)COverlayContext_Present_orig, (PVOID)COverlayContext_Present_hook,
					(PVOID*)&COverlayContext_Present_orig);
			else
				MH_CreateHook((PVOID)COverlayContext_Present_orig_24h2, (PVOID)COverlayContext_Present_hook_24h2,
					(PVOID*)&COverlayContext_Present_orig_24h2);

			if (!isWindows11_24h2)
				MH_CreateHook((PVOID)COverlayContext_IsCandidateDirectFlipCompatbile_orig,
					(PVOID)COverlayContext_IsCandidateDirectFlipCompatbile_hook,
					(PVOID*)&COverlayContext_IsCandidateDirectFlipCompatbile_orig);
			else
				MH_CreateHook((PVOID)COverlayContext_IsCandidateDirectFlipCompatbile_orig_24h2,
					(PVOID)COverlayContext_IsCandidateDirectFlipCompatbile_hook_24h2,
					(PVOID*)&COverlayContext_IsCandidateDirectFlipCompatbile_orig_24h2);
			MH_CreateHook((PVOID)COverlayContext_OverlaysEnabled_orig, (PVOID)COverlayContext_OverlaysEnabled_hook,
				(PVOID*)&COverlayContext_OverlaysEnabled_orig);
			MH_EnableHook(MH_ALL_HOOKS);
			LOG_ONLY_ONCE("DWM HOOK DLL INITIALIZATION. START LOGGING")
				//MESSAGE_BOX_DBG("DWM HOOK INITIALIZATION", MB_OK)

				break;
		}
		return FALSE;
	}
	case DLL_PROCESS_DETACH:
		MH_Uninitialize();
		Sleep(100);
		UninitializeStuff();
		break;
	default:
		break;
	}
	return TRUE;
}