_Pragma("once")

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "media/video-capture/video-capture-types.h"

#pragma warning(disable:4996) 
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d11.lib")

#define VENDOR_ID_NVIDIA        0x10de
#define VENDOR_ID_AMD           0x1002
#define VENDOR_ID_MICROSOFT     0x1414
#define VENDOR_ID_INTEL         0x8086

#define SAFE_RELEASE(u) \
    do { if ((u) != NULL) (u)->lpVtbl->Release(u); (u) = NULL; } while(0)


typedef enum dxgi_status_e {
	DXGI_STATUS_RECREATE_DUP,
	DXGI_STATUS_TIMEOUT,
	DXGI_STATUS_NO_UPDATE,
	DXGI_STATUS_FAILURE,
	DXGI_STATUS_SUCCESS
}dxgi_status_t;

typedef struct dxgi_device_s {
	ID3D11Device* dev;
	ID3D11DeviceContext* ctx;
	uint32_t vid; /* vendor-id */
}dxgi_device_t;

typedef struct dxgi_dup_s {
	IDXGIOutputDuplication* dup;
	uint32_t width;
	uint32_t height;
}dxgi_dup_t;

typedef video_capture_frame_t dxgi_frame_t;

extern bool dxgi_create_device(dxgi_device_t * device);
extern bool dxgi_create_duplicator(dxgi_device_t * device, dxgi_dup_t * duplication);
extern int  dxgi_capture_frame(dxgi_dup_t * dup, dxgi_frame_t * frame);