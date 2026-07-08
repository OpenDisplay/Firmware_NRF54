#include "opendisplay_config_storage.h"

#include <stddef.h>
#include <string.h>
#include <zephyr/settings/settings.h>

#define OD_SETTINGS_KEY "od/config"
#define CONFIG_STORAGE_MAGIC 0xDEADBEEFu
#define CONFIG_STORAGE_VERSION 1u

static opendisplay_config_storage_t s_cached;
static bool s_loaded;

uint32_t calculateConfigCRC(uint8_t *data, uint32_t len)
{
	uint32_t crc = 0xFFFFFFFFu;

	for (uint32_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (int j = 0; j < 8; j++) {
			if (crc & 1u) {
				crc = (crc >> 1) ^ 0xEDB88320u;
			} else {
				crc >>= 1;
			}
		}
	}
	return ~crc;
}

bool initConfigStorage(void)
{
	return settings_subsys_init() == 0;
}

bool saveConfig(uint8_t *config_data, uint32_t len)
{
	opendisplay_config_storage_t rec;

	if (len > MAX_CONFIG_SIZE) {
		return false;
	}
	memset(&rec, 0, sizeof(rec));
	rec.magic = CONFIG_STORAGE_MAGIC;
	rec.version = CONFIG_STORAGE_VERSION;
	rec.data_len = len;
	rec.crc = calculateConfigCRC(config_data, len);
	memcpy(rec.data, config_data, len);
	s_cached = rec;
	s_loaded = true;
	return settings_save_one(OD_SETTINGS_KEY, &rec,
				 offsetof(opendisplay_config_storage_t, data) + len) == 0;
}

bool loadConfig(uint8_t *config_data, uint32_t *len)
{
	opendisplay_config_storage_t rec;
	ssize_t got;

	if (config_data == NULL || len == NULL) {
		return false;
	}
	if (s_loaded) {
		if (s_cached.magic != CONFIG_STORAGE_MAGIC || s_cached.data_len > MAX_CONFIG_SIZE) {
			return false;
		}
		memcpy(config_data, s_cached.data, s_cached.data_len);
		*len = s_cached.data_len;
		return true;
	}
	memset(&rec, 0, sizeof(rec));
	got = settings_load_one(OD_SETTINGS_KEY, &rec, sizeof(rec));
	if (got < (ssize_t)offsetof(opendisplay_config_storage_t, data)) {
		return false;
	}
	if (rec.magic != CONFIG_STORAGE_MAGIC || rec.data_len > MAX_CONFIG_SIZE) {
		return false;
	}
	if (rec.crc != calculateConfigCRC(rec.data, rec.data_len)) {
		return false;
	}
	s_cached = rec;
	s_loaded = true;
	memcpy(config_data, rec.data, rec.data_len);
	*len = rec.data_len;
	return true;
}

bool clearStoredConfig(void)
{
	(void)settings_delete(OD_SETTINGS_KEY);
	memset(&s_cached, 0, sizeof(s_cached));
	s_loaded = false;
	return true;
}
