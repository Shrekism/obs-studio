#include <CoreFoundation/CFString.h>
#include <CoreAudio/CoreAudio.h>

#include "../../obs-internal.h"
#include "../../util/dstr.h"

#include "mac-helpers.h"

static inline bool cf_to_cstr(CFStringRef ref, char *buf, size_t size)
{
	if (!ref) return false;
	return (bool)CFStringGetCString(ref, buf, size, kCFStringEncodingUTF8);
}

static void obs_enum_audio_monitoring_device(obs_enum_audio_device_cb cb,
		void *data, AudioDeviceID id)
{
	UInt32      size    = 0;
	CFStringRef cf_name = NULL;
	CFStringRef cf_uid  = NULL;
	char        name[1024];
	char        uid[1024];
	OSStatus    stat;

	AudioObjectPropertyAddress addr = {
		kAudioDevicePropertyStreams,
		kAudioDevicePropertyScopeInput,
		kAudioObjectPropertyElementMaster
	};

	/* check to see if it's a mac input device */
	AudioObjectGetPropertyDataSize(id, &addr, 0, NULL, &size);
	if (!size)
		return;

	size = sizeof(CFStringRef);

	addr.mSelector = kAudioDevicePropertyDeviceUID;
	stat = AudioObjectGetPropertyData(id, &addr, 0, NULL, &size, &cf_uid);
	if (!success(stat, "get audio device UID"))
		return;

	addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
	stat = AudioObjectGetPropertyData(id, &addr, 0, NULL, &size, &cf_name);
	if (!success(stat, "get audio device name"))
		goto fail;

	if (!cf_to_cstr(cf_name, name, sizeof(name))) {
		blog(LOG_WARNING, "%s: failed to convert name", __FUNCTION__);
		goto fail;
	}

	if (!cf_to_cstr(cf_uid, uid, sizeof(uid))) {
		blog(LOG_WARNING, "%s: failed to convert uid", __FUNCTION__);
		goto fail;
	}

	cb(data, name, uid);

fail:
	if (cf_name)
		CFRelease(cf_name);
	if (cf_uid)
		CFRelease(cf_uid);
}

void obs_enum_audio_monitoring_devices(obs_enum_audio_device_cb cb, void *data)
{
	AudioObjectPropertyAddress addr = {
		kAudioHardwarePropertyDevices,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	UInt32        size = 0;
	UInt32        count;
	OSStatus      stat;
	AudioDeviceID *ids;

	stat = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr,
						0, NULL, &size);
	if (!success(stat, "get data size"))
		return;

	ids   = malloc(size);
	count = size / sizeof(AudioDeviceID);

	stat = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr,
						0, NULL, &size, ids);
	if (success(stat, "get data")) {
		for (UInt32 i = 0; i < count; i++)
			obs_enum_audio_monitoring_device(cb, data, ids[i]);
	}

	free(ids);
}

static void alloc_default_id(void *data, const char *name, const char *id)
{
	char **p_id = data;
	UNUSED_PARAMETER(name);

	*p_id = bstrdup(id);
}

static void get_default_id(char **p_id)
{
	AudioObjectPropertyAddress addr = {
		kAudioHardwarePropertyDefaultSystemOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	OSStatus      stat;
	AudioDeviceID id;
	UInt32        size = sizeof(id);

	stat = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0,
			NULL, &size, id);
	if (success(stat, "AudioObjectGetPropertyData"))
		obs_enum_audio_monitoring_device(alloc_default_id, p_id, id);

	if (!*p_id)
		*p_id = bzalloc(1);
}

bool devices_match(const char *id1, const char *id2)
{
	char *default_id = NULL;
	bool match;

	if (!id1 || !id2)
		return false;

	if (strcmp(id1, "default") == 0) {
		get_default_id(&default_id);
		id1 = default_id;
	}
	if (strcmp(id2, "default") == 0) {
		get_default_id(&default_id);
		id2 = default_id;
	}

	match = strcmp(id1, id2) == 0;
	bfree(default_id);

	return match;
}
