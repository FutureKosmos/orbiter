_Pragma("once")

#include "cdk.h"

#define COBJMACROS
#include <dxgi1_5.h>
#include <d3d11.h>

//#pragma warning(disable:4996)
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d11.lib")

#define SAFE_RELEASE(u) \
    do { if ((u) != NULL) (u)->lpVtbl->Release(u); (u) = NULL; } while(0)

typedef struct d3d11_device_s {
	ID3D11Device* p_device;
	ID3D11DeviceContext* p_context;
	int vendor;
}d3d11_device_t;

d3d11_device_t d3d11_;


extern void d3d11_device_create(void);
extern void d3d11_device_destroy(void);