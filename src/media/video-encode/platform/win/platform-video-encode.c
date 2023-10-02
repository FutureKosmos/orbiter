#define COBJMACROS
#include "media/video-encode/video-encode-types.h"
#include "media/video-capture/video-capture-types.h"

#define SAFE_RELEASE(u) \
    do { if ((u) != NULL) (u)->lpVtbl->Release(u); (u) = NULL; } while(0)

typedef struct d3d11_device_s {
	ID3D11Device* device;
	ID3D11DeviceContext* context;
}d3d11_device_t;

typedef struct d3d11_video_device_s {
	ID3D11VideoDevice* device;
	ID3D11VideoContext* context;
}d3d11_video_device_t;

typedef struct d3d11_video_processor_s {
	ID3D11VideoProcessor* processor;
	ID3D11VideoProcessorEnumerator* enumerator;
}d3d11_video_processor_t;

static d3d11_device_t d3d11_device;
static d3d11_video_device_t d3d11_video_device;
static d3d11_video_processor_t d3d11_video_processor;
static int last_frame_width;
static int last_frame_height;

static bool _d3d11_create_device(d3d11_device_t* device) {
	HRESULT hr;
	IDXGIFactory5* p_dxgi_factory5; /* using five version to control tearing */
	IDXGIAdapter* p_dxgi_adapter;

	p_dxgi_factory5 = NULL;
	p_dxgi_adapter = NULL;

	SAFE_RELEASE(device->device);
	SAFE_RELEASE(device->context);

	/**
	 * using CreateDXGIFactory1 not CreateDXGIFactory for IDXGIOutput1_DuplicateOutput success.
	 * https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgioutput1-duplicateoutput
	 */
	if (FAILED(hr = CreateDXGIFactory1(&IID_IDXGIFactory5, &p_dxgi_factory5)))
	{
		printf("create dxgi factory failed: 0x%x.\n", hr);
		return false;
	}
	IDXGIFactory5_EnumAdapters(p_dxgi_factory5, 0, &p_dxgi_adapter);

	/**
	 * when adapter not null, driver type must be D3D_DRIVER_TYPE_UNKNOWN.
	 * https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice
	 */
	hr = D3D11CreateDevice(p_dxgi_adapter,
		D3D_DRIVER_TYPE_UNKNOWN,
		NULL,
		0,
		NULL,
		0,
		D3D11_SDK_VERSION,
		&device->device,
		NULL,
		&device->context);

	if (FAILED(hr)) {
		printf("d3d11 create device and context failed: 0x%x.\n", hr);

		SAFE_RELEASE(p_dxgi_factory5);
		SAFE_RELEASE(p_dxgi_adapter);
		return false;
	}

	SAFE_RELEASE(p_dxgi_factory5);
	SAFE_RELEASE(p_dxgi_adapter);
	return true;
}

static void _d3d11_create_video_device(d3d11_device_t* device, d3d11_video_device_t* video_device) {
	HRESULT hr;
	if (FAILED(hr = ID3D11Device_QueryInterface(device->device, &IID_ID3D11VideoDevice, &video_device->device)))
	{
		printf("query video device failed: 0x%x.\n", hr);
		return;
	}
	if (FAILED(hr = ID3D11DeviceContext_QueryInterface(device->context, &IID_ID3D11VideoContext, &video_device->context)))
	{
		printf("query video device context failed: 0x%x.\n", hr);
		return;
	}
}

static void _d3d11_bgra_to_nv12(d3d11_video_device_t* device, d3d11_video_processor_t* processor, ID3D11Texture2D* bgra, ID3D11Texture2D* nv12){
	ID3D11VideoProcessorInputView* in_view;
	ID3D11VideoProcessorOutputView* out_view;
	D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC in_view_desc;
	D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC out_view_desc;
	D3D11_VIDEO_PROCESSOR_STREAM stream;

	memset(&in_view_desc, 0, sizeof(D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC));
	memset(&out_view_desc, 0, sizeof(D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC));
	memset(&stream, 0, sizeof(D3D11_VIDEO_PROCESSOR_STREAM));

	in_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
	out_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;

	if (FAILED(ID3D11VideoDevice_CreateVideoProcessorInputView(device->device, (ID3D11Resource*)bgra, processor->enumerator, &in_view_desc, &in_view))) {
		printf("failed to create video processor input view\n");
		return;
	}
	if (FAILED(ID3D11VideoDevice_CreateVideoProcessorOutputView(device->device, (ID3D11Resource*)nv12, processor->enumerator, &out_view_desc, &out_view))) {
		printf("failed to create video processor output view\n");
		SAFE_RELEASE(in_view);
		return;
	}
	stream.Enable = true;
	stream.pInputSurface = in_view;
	HRESULT hr;
	if (FAILED(hr = ID3D11VideoContext_VideoProcessorBlt(device->context, processor->processor, out_view, 0, 1, &stream))) {
		printf("failed to video processor blt\n");
		SAFE_RELEASE(in_view);
		SAFE_RELEASE(out_view);
		return;
	}
	SAFE_RELEASE(in_view);
	SAFE_RELEASE(out_view);
}

static void _d3d11_create_video_processor(d3d11_video_device_t* device, D3D11_TEXTURE2D_DESC* in_desc, D3D11_TEXTURE2D_DESC* out_desc, d3d11_video_processor_t* processor) {
	D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc;

	content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
	content_desc.InputFrameRate.Numerator = 1;
	content_desc.InputFrameRate.Denominator = 1;
	content_desc.InputHeight = in_desc->Height;
	content_desc.InputWidth = in_desc->Width;
	content_desc.OutputFrameRate.Numerator = 1;
	content_desc.OutputFrameRate.Denominator = 1;
	content_desc.OutputHeight = out_desc->Height;
	content_desc.OutputWidth = out_desc->Width;
	content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

	if (FAILED(ID3D11VideoDevice_CreateVideoProcessorEnumerator(device->device, &content_desc, &processor->enumerator))) {
		printf("failed to create video processor enumerator\n");
		return;
	}
	if (FAILED(ID3D11VideoDevice_CreateVideoProcessor(device->device, processor->enumerator, 0, &processor->processor))) {
		printf("failed to create video processor\n");
		return;
	}
}
void dump_nv12(ID3D11Texture2D* frame, d3d11_device_t* dev, int w, int h) {
	static FILE* bgra;
	D3D11_TEXTURE2D_DESC d3d11_texture_desc;
	ID3D11Texture2D* p_frame;
	IDXGISurface* p_dxgi_surface;
	DXGI_MAPPED_RECT dxgi_mapped_rect;

	if (!bgra) {
		bgra = fopen("dump.nv12", "ab+");
	}

	ID3D11Texture2D_GetDesc(frame, &d3d11_texture_desc);
	d3d11_texture_desc.MipLevels = 1;
	d3d11_texture_desc.ArraySize = 1;
	d3d11_texture_desc.SampleDesc.Count = 1;
	d3d11_texture_desc.Usage = D3D11_USAGE_STAGING;
	d3d11_texture_desc.BindFlags = 0;
	d3d11_texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	d3d11_texture_desc.MiscFlags = 0;

	ID3D11Device_CreateTexture2D(dev->device, &d3d11_texture_desc, NULL, &p_frame);
	ID3D11DeviceContext_CopyResource(dev->context, (ID3D11Resource*)p_frame, (ID3D11Resource*)frame);

	ID3D11Texture2D_QueryInterface(p_frame, &IID_IDXGISurface, &p_dxgi_surface);
	if (p_frame) {
		ID3D11Texture2D_Release(p_frame);
		p_frame = NULL;
	}
	IDXGISurface_Map(p_dxgi_surface, &dxgi_mapped_rect, DXGI_MAP_READ);

	for (int i = 0; i < (int)h; i++) {
		fwrite(dxgi_mapped_rect.pBits + i * dxgi_mapped_rect.Pitch, dxgi_mapped_rect.Pitch, 1, bgra);
	}
	IDXGISurface_Unmap(p_dxgi_surface);

	if (p_dxgi_surface) {
		IDXGISurface_Release(p_dxgi_surface);
		p_dxgi_surface = NULL;
	}
}
video_encode_frame_t* platform_video_encode(video_capture_frame_t* frame) {
	ID3D11Texture2D* texture;
	D3D11_TEXTURE2D_DESC in_desc;
	D3D11_TEXTURE2D_DESC out_desc;

	memset(&in_desc, 0, sizeof(D3D11_TEXTURE2D_DESC));
	memset(&out_desc, 0, sizeof(D3D11_TEXTURE2D_DESC));

	ID3D11Texture2D_GetDesc(frame->frame, &in_desc);

	out_desc.Width = frame->width;
	out_desc.Height = frame->height;
	out_desc.MipLevels = 1;
	out_desc.ArraySize = 1;
	out_desc.Format = DXGI_FORMAT_NV12;
	out_desc.SampleDesc.Count = 1;
	out_desc.Usage = D3D11_USAGE_DEFAULT;
	out_desc.BindFlags = D3D11_BIND_RENDER_TARGET;
	out_desc.CPUAccessFlags = 0;
	
	if (d3d11_device.device == NULL || d3d11_device.context == NULL) {
		_d3d11_create_device(&d3d11_device);
	}
	if (d3d11_video_device.device == NULL || d3d11_video_device.context == NULL) {
		_d3d11_create_video_device(&d3d11_device, &d3d11_video_device);
	}
	if (last_frame_width != frame->width || last_frame_height != frame->height) {
		if (d3d11_video_processor.enumerator == NULL || d3d11_video_processor.processor == NULL) {
			_d3d11_create_video_processor(&d3d11_video_device, &in_desc, &out_desc, &d3d11_video_processor);
		}
		else {
			/** resolution changed, needed to recreate processor */
			SAFE_RELEASE(d3d11_video_processor.enumerator);
			SAFE_RELEASE(d3d11_video_processor.processor);

			_d3d11_create_video_processor(&d3d11_video_device, &in_desc, &out_desc, &d3d11_video_processor);
		}
	}
	if (FAILED(ID3D11Device_CreateTexture2D(d3d11_device.device, &out_desc, NULL, &texture))) {
		printf("faliled to create d3d11texture\n");
		return NULL;
	}
	_d3d11_bgra_to_nv12(&d3d11_video_device, &d3d11_video_processor, frame->frame, texture);

	//dump_nv12(texture, &d3d11_device, 1920, 1080);

	last_frame_width = frame->width;
	last_frame_height = frame->height;
	return NULL;
}