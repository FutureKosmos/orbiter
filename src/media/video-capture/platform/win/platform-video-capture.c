#include "cdk.h"
#include "media/video-capture/platform/platform-video-capture.h"
#include "dxgi-video-capture.h"

static atomic_flag _once = ATOMIC_FLAG_INIT;
int fps = 0;
bool exec_once = true;

void platform_video_capture(video_capture_frame_t* frame, video_capture_type_t type) {
	dxgi_device_t dev;
	dxgi_dup_t dup;

	if (!atomic_flag_test_and_set(&_once)) {
		memset(&dev, 0, sizeof(dxgi_device_t));
		memset(&dup, 0, sizeof(dxgi_dup_t));
		memset(&frame, 0, sizeof(dxgi_frame_t));

		dxgi_create_device(&dev);
		dxgi_create_duplicator(&dev, &dup);
	}

	dxgi_status_t r;
	while (true) {
		static uint64_t base;
		if (exec_once) {
			base = cdk_timespec_get();
			exec_once = false;
		}
		dxgi_frame_t* frame = malloc(sizeof(dxgi_frame_t));

		r = dxgi_capture_frame(&dup, &frame);
		if (r == DXGI_STATUS_TIMEOUT || r == DXGI_STATUS_NO_UPDATE || r == DXGI_STATUS_FAILURE) {
			continue;
		}
		if (r == DXGI_STATUS_RECREATE_DUP) {
			printf("need re-create dup.\n");
			dxgi_create_duplicator(&dev, &dup);
			continue;
		}
		uint64_t end = cdk_timespec_get();
		if (end - base >= 1000) {
			printf("fps: %d\n", fps);
			base = end;
			fps = 0;
		}
		else {
			fps++;
		}
		//dump_bgra(&frame, &d3d_dev, &dup);
	}
}