
#include "pch.h"
#include "d3d11.h"
#include "d3d11_1.h"
#include "dxgi.h"
#include "d3d9on12.h"
#include "dxva.h"
#include "dxva2api.h"
#include <iostream>
#include < vector >
#include < tuple >
#include < comdef.h >
#include <string>
#include <thread>

struct GetMonitorRect_data {
	int current;
	int required;
	RECT requiredRect;
};
BOOL CALLBACK GetMonitorRect_MonitorEnumProc(HMONITOR /*hMonitor*/,
	HDC /*hdcMonitor*/,
	LPRECT lprcMonitor,
	LPARAM dwData)
{
	GetMonitorRect_data* data = reinterpret_cast<GetMonitorRect_data*>(dwData);
	RECT r = { 0 };
	if (NULL == lprcMonitor)
		lprcMonitor = &r;

	if (data->current == data->required)
		data->requiredRect = *lprcMonitor;
	data->current++;

	return TRUE;
}

class DeviceHandle
{
public:
	DeviceHandle(IDirect3DDeviceManager9* manager)
		: m_manager(manager)
		, m_handle(0)
	{
		if (manager)
		{
			HRESULT hr = manager->OpenDeviceHandle(&m_handle);
			if (FAILED(hr))
				m_manager = 0;
		}
	}

	~DeviceHandle()
	{
		if (m_manager && m_handle)
			m_manager->CloseDeviceHandle(m_handle);
	}

	HANDLE Detach()
	{
		HANDLE tmp = m_handle;
		m_manager = 0;
		m_handle = 0;
		return tmp;
	}

	operator HANDLE()
	{
		return m_handle;
	}

	bool operator !() const
	{
		return m_handle == 0;
	}

protected:
	CComPtr<IDirect3DDeviceManager9> m_manager;
	HANDLE m_handle;
};

CComPtr<IDirect3D9Ex> d3d9;
CComPtr<IDirect3DDeviceManager9> deviceManager;
CComPtr<IDirectXVideoProcessorService> service;
CComPtr<IDirect3DDevice9Ex> d3d9Device;

std::vector<IDirect3DSurface9*> d3d9Surfaces(10);

int init(bool used3d9on12) {

	HRESULT hr = 0;
	if (used3d9on12)
	{
		printf("d3d9on12 path\n");
		D3D9ON12_ARGS args = {};
		args.Enable9On12 = true;

		hr = Direct3DCreate9On12Ex(D3D_SDK_VERSION, &args, 1, &d3d9);
		if (FAILED(hr)) {
			std::cout << "Direct3DCreate9On12 failed" << hr << std::endl;
			return 1;
		}
	}
	else
	{
		printf("d3d9 path\n");
		hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9);
	}

	GetMonitorRect_data monitor = { 0 };
	monitor.required = 0;
	EnumDisplayMonitors(NULL, NULL, &GetMonitorRect_MonitorEnumProc, (LPARAM)&monitor);

	POINT point = { monitor.requiredRect.left, monitor.requiredRect.top };
	HWND  hWindow = WindowFromPoint(point);

	D3DPRESENT_PARAMETERS d3dPar = {};
	d3dPar.BackBufferFormat = (D3DFORMAT)22;
	d3dPar.BackBufferCount = 24;
	d3dPar.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dPar.hDeviceWindow = (HWND)hWindow;
	d3dPar.Windowed = 1;
	d3dPar.Flags = D3DPRESENTFLAG_VIDEO;
	d3dPar.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT | D3DPRESENT_DONOTWAIT;

	hr = d3d9->CreateDeviceEx(0,
		D3DDEVTYPE_HAL,
		(HWND)hWindow,
		D3DCREATE_HARDWARE_VERTEXPROCESSING |
		D3DCREATE_FPU_PRESERVE |
		D3DCREATE_MULTITHREADED,
		&d3dPar,
		nullptr,
		&d3d9Device);
	if (FAILED(hr)) {
		std::cout << "CreateDeviceEx failed " << hr << std::endl;
		return 1;
	}

	UINT resetToken = 0;
	hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &deviceManager);
	if (FAILED(hr)) {
		std::cout << "DXVA2CreateDirect3DDeviceManager9 failed " << hr << std::endl;
		return 1;
	}

	hr = deviceManager->ResetDevice(d3d9Device, resetToken);
	if (FAILED(hr)) {
		std::cout << "deviceManager->ResetDevice failed " << hr << std::endl;
		return 1;
	}

	DeviceHandle device = DeviceHandle(deviceManager);
	hr = deviceManager->GetVideoService(device, IID_PPV_ARGS(&service));
	if (FAILED(hr)) {
		std::cout << "deviceManager->GetVideoService failed " << hr << std::endl;
		return 1;
	}

	hr = service->CreateSurface(
		4096,
		2048,
		10 - 1,
		(D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'),
		//D3DFMT_A8R8G8B8,
		D3DPOOL_DEFAULT,
		0,
		DXVA2_VideoDecoderRenderTarget,
		d3d9Surfaces.data(),
		NULL);
	if (FAILED(hr)) {
		std::cout << "CreateSurface " << hr << std::endl;
		return 1;
	}

	return 0;
}

int main(int argc, char *argv[])
{

	bool used3d9on12 = !!(argc > 1);

	if (init(used3d9on12)) {
		std::cout << "Init failed " << std::endl;
		return -1;
	}

	for (size_t i = 0; i < 2000;++i)
	{
		D3DLOCKED_RECT rect;
		auto hr = d3d9Surfaces[0]->LockRect(&rect, NULL, D3DLOCK_NOSYSLOCK);
		if (FAILED(hr)) {
			printf("LockRect FAILED\n");
			abort();
		}
		//char* data = (char*)rect.pBits;
		//data = 0x0;
		hr = d3d9Surfaces[0]->UnlockRect();
		if (FAILED(hr)) {
			printf("UnlockRect FAILED\n");
			abort();
		}
	}

}
