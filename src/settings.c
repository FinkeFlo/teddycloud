
#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#include "debug.h"
#include "settings.h"

#include "fs_port.h"

#define MAX_OVERLAYS 2 + 1
static settings_t Settings_Overlay[MAX_OVERLAYS];
static setting_item_t *Option_Map_Overlay[MAX_OVERLAYS];

static void option_map_init(const char *overlay, setting_item_t **option_map)
{
    settings_t *settings = get_settings_ovl(overlay);

    OPTION_START()

    OPTION_INTERNAL_UNSIGNED("configVersion", &settings->configVersion, 0, 0, 255, "Config version")
    OPTION_UNSIGNED("log.level", &settings->log.level, 4, 0, 6, "0=off - 6=verbose")
    OPTION_BOOL("log.color", &settings->log.color, TRUE, "Colored log")

    /* settings for HTTPS server */
    OPTION_STRING("core.commonName", &settings->core.commonName, "", "common name of the certificate (for overlays)")
    OPTION_UNSIGNED("core.server.https_port", &settings->core.http_port, 443, 1, 65535, "HTTPS port")
    OPTION_UNSIGNED("core.server.http_port", &settings->core.https_port, 80, 1, 65535, "HTTP port")
    OPTION_STRING("core.certdir", &settings->core.certdir, "certs/client", "Directory where to upload genuine client certs to")
    OPTION_STRING("core.contentdir", &settings->core.contentdir, "default", "Directory where cloud content is placed")
    OPTION_STRING("core.datadir", &settings->core.datadir, "data", "Base directory for contentdir/wwwdir when relative")
    OPTION_STRING("core.wwwdir", &settings->core.wwwdir, "www", "Directory where web content is placed")

    OPTION_STRING("core.server_cert.file.ca", &settings->core.server_cert.file.ca, "certs/server/ca-root.pem", "Server CA")
    OPTION_STRING("core.server_cert.file.crt", &settings->core.server_cert.file.crt, "certs/server/teddy-cert.pem", "Server certificate")
    OPTION_STRING("core.server_cert.file.key", &settings->core.server_cert.file.key, "certs/server/teddy-key.pem", "Server key")
    OPTION_STRING("core.server_cert.data.ca", &settings->core.server_cert.data.ca, "", "Server CA data")
    OPTION_STRING("core.server_cert.data.crt", &settings->core.server_cert.data.crt, "", "Server certificate data")
    OPTION_STRING("core.server_cert.data.key", &settings->core.server_cert.data.key, "", "Server key data")

    /* settings for HTTPS/cloud client */
    OPTION_STRING("core.client_cert.file.ca", &settings->core.client_cert.file.ca, "certs/client/ca.der", "Client CA")
    OPTION_STRING("core.client_cert.file.crt", &settings->core.client_cert.file.crt, "certs/client/client.der", "Client certificate")
    OPTION_STRING("core.client_cert.file.key", &settings->core.client_cert.file.key, "certs/client/private.der", "Client key")
    OPTION_STRING("core.client_cert.data.ca", &settings->core.client_cert.data.ca, "", "Client CA")
    OPTION_STRING("core.client_cert.data.crt", &settings->core.client_cert.data.crt, "", "Client certificate data")
    OPTION_STRING("core.client_cert.data.key", &settings->core.client_cert.data.key, "", "Client key data")

    OPTION_STRING("core.allowOrigin", &settings->core.allowOrigin, "", "Set CORS Access-Control-Allow-Origin header")

    OPTION_INTERNAL_STRING("internal.server.ca", &settings->internal.server.ca, "", "Server CA data")
    OPTION_INTERNAL_STRING("internal.server.crt", &settings->internal.server.crt, "", "Server certificate data")
    OPTION_INTERNAL_STRING("internal.server.key", &settings->internal.server.key, "", "Server key data")
    OPTION_INTERNAL_STRING("internal.client.ca", &settings->internal.client.ca, "", "Client CA")
    OPTION_INTERNAL_STRING("internal.client.crt", &settings->internal.client.crt, "", "Client certificate data")
    OPTION_INTERNAL_STRING("internal.client.key", &settings->internal.client.key, "", "Client key data")

    OPTION_INTERNAL_BOOL("internal.exit", &settings->internal.exit, FALSE, "Exit the server")
    OPTION_INTERNAL_SIGNED("internal.returncode", &settings->internal.returncode, 0, -128, 127, "Returncode when exiting")
    OPTION_INTERNAL_BOOL("internal.config_init", &settings->internal.config_init, TRUE, "Config initialized?")
    OPTION_INTERNAL_BOOL("internal.config_changed", &settings->internal.config_changed, FALSE, "Config changed and unsaved?")
    OPTION_INTERNAL_STRING("internal.contentdirfull", &settings->internal.contentdirfull, "", "Directory where cloud content is placed (absolute)")
    OPTION_INTERNAL_STRING("internal.wwwdirfull", &settings->internal.wwwdirfull, "", "Directory where web content is placed (absolute)")
    OPTION_INTERNAL_STRING("internal.overlayName", &settings->internal.overlayName, "", "Name of the overlay")

    OPTION_BOOL("cloud.enabled", &settings->cloud.enabled, FALSE, "Generally enable cloud operation")
    OPTION_STRING("cloud.hostname", &settings->cloud.remote_hostname, "prod.de.tbs.toys", "Hostname of remote cloud server")
    OPTION_UNSIGNED("cloud.port", &settings->cloud.remote_port, 443, 1, 65535, "Port of remote cloud server")
    OPTION_BOOL("cloud.enableV1Claim", &settings->cloud.enableV1Claim, TRUE, "Pass 'claim' queries to boxine cloud")
    OPTION_BOOL("cloud.enableV1CloudReset", &settings->cloud.enableV1CloudReset, FALSE, "Pass 'cloudReset' queries to boxine cloud")
    OPTION_BOOL("cloud.enableV1FreshnessCheck", &settings->cloud.enableV1FreshnessCheck, TRUE, "Pass 'freshnessCheck' queries to boxine cloud")
    OPTION_BOOL("cloud.enableV1Log", &settings->cloud.enableV1Log, FALSE, "Pass 'log' queries to boxine cloud")
    OPTION_BOOL("cloud.enableV1Time", &settings->cloud.enableV1Time, FALSE, "Pass 'time' queries to boxine cloud")
    OPTION_BOOL("cloud.enableV1Ota", &settings->cloud.enableV1Ota, FALSE, "Pass 'ota' queries to boxine cloud")
    OPTION_BOOL("cloud.enableV2Content", &settings->cloud.enableV2Content, TRUE, "Pass 'content' queries to boxine cloud")
    OPTION_BOOL("cloud.cacheContent", &settings->cloud.cacheContent, FALSE, "Cache cloud content on local server")
    OPTION_BOOL("cloud.markCustomTagByPass", &settings->cloud.markCustomTagByPass, TRUE, "Automatically mark custom tags by password")
    OPTION_BOOL("cloud.markCustomTagByUid", &settings->cloud.markCustomTagByUid, TRUE, "Automatically mark custom tags by Uid")

    OPTION_BOOL("toniebox.overrideCloud", &settings->toniebox.overrideCloud, TRUE, "Override toniebox settings from the boxine cloud")
    OPTION_UNSIGNED("toniebox.max_vol_spk", &settings->toniebox.max_vol_spk, 3, 0, 3, "Limit speaker volume (0-3)")
    OPTION_UNSIGNED("toniebox.max_vol_hdp", &settings->toniebox.max_vol_hdp, 3, 0, 3, "Limit headphone volume (0-3)")
    OPTION_BOOL("toniebox.slap_enabled", &settings->toniebox.slap_enabled, TRUE, "Enable slapping to skip a track")
    OPTION_BOOL("toniebox.slap_back_left", &settings->toniebox.slap_back_left, FALSE, "False=left-backwards - True=left-forward")
    OPTION_UNSIGNED("toniebox.led", &settings->toniebox.led, 0, 0, 2, "0=on, 1=off, 2=dimmed")

    OPTION_BOOL("rtnl.logRaw", &settings->rtnl.logRaw, FALSE, "Enable raw rtnl logging")
    OPTION_BOOL("rtnl.logHuman", &settings->rtnl.logHuman, FALSE, "Enable human readable rtnl logging")
    OPTION_STRING("rtnl.logRawFile", &settings->rtnl.logRawFile, "config/rtnl.bin", "RTNL raw logfile")
    OPTION_STRING("rtnl.logHumanFile", &settings->rtnl.logHumanFile, "config/rtnl.csv", "RTNL human readable logfile")

    OPTION_BOOL("mqtt.enabled", &settings->mqtt.enabled, FALSE, "Enable MQTT service")
    OPTION_STRING("mqtt.hostname", &settings->mqtt.hostname, "", "MQTT hostname")
    OPTION_STRING("mqtt.username", &settings->mqtt.username, "", "Username")
    OPTION_STRING("mqtt.password", &settings->mqtt.password, "", "Password")
    OPTION_STRING("mqtt.identification", &settings->mqtt.identification, "", "Client identification")
    OPTION_END()

    if (*option_map == NULL)
        *option_map = osAllocMem(sizeof(setting_item_t) * sizeof(option_map_array));
    osMemcpy(*option_map, option_map_array, sizeof(option_map_array));
}
static setting_item_t *get_option_map(const char *overlay)
{
    return Option_Map_Overlay[get_overlay_id(overlay)];
}
void overlay_settings_init()
{
    for (uint8_t i = 1; i < MAX_OVERLAYS; i++)
    {
        osMemcpy(&Settings_Overlay[i], get_settings(), sizeof(settings_t));
        //  TODO clone strings and free them
        option_map_init(NULL, &Option_Map_Overlay[i]);
        Settings_Overlay[i].internal.config_init = false;
    }
}

settings_t *get_settings()
{
    return &Settings_Overlay[0];
}
settings_t *get_settings_ovl(const char *overlay)
{
    return &Settings_Overlay[get_overlay_id(overlay)];
}

uint8_t get_overlay_id(const char *overlay)
{
    if (overlay == NULL)
        return 0;
    for (uint8_t i = 1; i < MAX_OVERLAYS; i++)
    {
        if (osStrcmp(Settings_Overlay[i].internal.overlayName, overlay))
        {
            return i;
        }
    }
    return 0;
}

void settings_resolve_dir(char **resolvedPath, char *path, char *basePath)
{
    if (path[0] == '/')
    {
        osSprintf(*resolvedPath, "%s", path);
    }
    else
    {
        osSprintf(*resolvedPath, "%s/%s", basePath, path);
    }
}
void settings_generate_internal_dirs(settings_t *settings)
{
    free(settings->internal.contentdirfull);
    free(settings->internal.wwwdirfull);

    settings->internal.contentdirfull = osAllocMem(256);
    settings->internal.wwwdirfull = osAllocMem(256);

    char contentPath[256];
    char *contentPathPointer = contentPath;

    settings_resolve_dir(&contentPathPointer, "content", settings->core.datadir);
    settings_resolve_dir(&settings->internal.contentdirfull, settings->core.contentdir, contentPath);

    settings_resolve_dir(&settings->internal.wwwdirfull, settings->core.wwwdir, settings->core.datadir);
}

void settings_changed()
{
    Settings_Overlay[0].internal.config_changed = true;
    settings_generate_internal_dirs(get_settings());
    overlay_settings_init();
}

void settings_deinit()
{
    for (uint8_t i = 0; i < MAX_OVERLAYS; i++)
    {
        int pos = 0;
        setting_item_t *option_map = Option_Map_Overlay[i];
        while (option_map[pos].type != TYPE_END)
        {
            setting_item_t *opt = &option_map[pos];

            switch (opt->type)
            {
            case TYPE_STRING:
                if (*((char **)opt->ptr))
                {
                    osFreeMem(*((char **)opt->ptr));
                }
                break;
            default:
                break;
            }
            pos++;
        }
        osFreeMem(option_map);
    }
}

void settings_init()
{
    option_map_init(NULL, &Option_Map_Overlay[0]);

    Settings_Overlay[0].log.level = LOGLEVEL_INFO;

    int pos = 0;
    setting_item_t *option_map = get_option_map(NULL);
    while (option_map[pos].type != TYPE_END)
    {
        setting_item_t *opt = &option_map[pos];

        switch (opt->type)
        {
        case TYPE_BOOL:
            TRACE_DEBUG("  %s = %s\r\n", opt->option_name, opt->init.bool_value ? "true" : "false");
            *((bool *)opt->ptr) = opt->init.bool_value;
            break;
        case TYPE_SIGNED:
            TRACE_DEBUG("  %s = %d\r\n", opt->option_name, opt->init.signed_value);
            *((uint32_t *)opt->ptr) = opt->init.signed_value;
            break;
        case TYPE_UNSIGNED:
        case TYPE_HEX:
            TRACE_DEBUG("  %s = %d\r\n", opt->option_name, opt->init.unsigned_value);
            *((uint32_t *)opt->ptr) = opt->init.unsigned_value;
            break;
        case TYPE_FLOAT:
            TRACE_DEBUG("  %s = %f\r\n", opt->option_name, opt->init.float_value);
            *((uint32_t *)opt->ptr) = opt->init.float_value;
            break;
        case TYPE_STRING:
            TRACE_DEBUG("  %s = %s\r\n", opt->option_name, opt->init.string_value);
            *((char **)opt->ptr) = strdup(opt->init.string_value);
            break;
        default:
            break;
        }
        pos++;
    }
    settings_changed();
    settings_load();
}

void settings_save()
{
    settings_save_ovl(NULL);
}
void settings_save_ovl(bool overlay)
{
    char_t *config_path = (!overlay ? CONFIG_PATH : CONFIG_OVERLAY_PATH);

    TRACE_INFO("Save settings to %s\r\n", config_path);
    FsFile *file = fsOpenFile(config_path, FS_FILE_MODE_WRITE | FS_FILE_MODE_TRUNC);
    if (file == NULL)
    {
        TRACE_WARNING("Failed to open config file for writing\r\n");
        return;
    }

    if (overlay)
    {
        fsCloseFile(file);
        return;
    }

    Settings_Overlay[0].configVersion = CONFIG_VERSION;

    int pos = 0;
    char buffer[256]; // Buffer to hold the file content
    while (get_option_map(NULL)[pos].type != TYPE_END)
    {
        if (!get_option_map(NULL)[pos].internal || !osStrcmp(get_option_map(NULL)[pos].option_name, "configVersion"))
        {
            setting_item_t *opt = &get_option_map(NULL)[pos];

            switch (opt->type)
            {
            case TYPE_BOOL:
                sprintf(buffer, "%s=%s\n", opt->option_name, *((bool *)opt->ptr) ? "true" : "false");
                break;
            case TYPE_SIGNED:
                sprintf(buffer, "%s=%d\n", opt->option_name, *((int32_t *)opt->ptr));
                break;
            case TYPE_UNSIGNED:
            case TYPE_HEX:
                sprintf(buffer, "%s=%u\n", opt->option_name, *((uint32_t *)opt->ptr));
                break;
            case TYPE_FLOAT:
                sprintf(buffer, "%s=%f\n", opt->option_name, *((float *)opt->ptr));
                break;
            case TYPE_STRING:
                sprintf(buffer, "%s=%s\n", opt->option_name, *((char **)opt->ptr));
                break;
            default:
                buffer[0] = 0;
                break;
            }
            if (osStrlen(buffer) > 0)
                fsWriteFile(file, buffer, osStrlen(buffer));
        }
        pos++;

        fsCloseFile(file);
        Settings_Overlay[0].internal.config_changed = false;
    }
}
void settings_load()
{
    settings_load_ovl(NULL);
}
void settings_load_ovl(bool overlay)
{
    char_t *config_path = (!overlay ? CONFIG_PATH : CONFIG_OVERLAY_PATH);

    TRACE_INFO("Load settings from %s\r\n", config_path);
    if (!fsFileExists(config_path))
    {
        TRACE_WARNING("Config file does not exist, creating it...\r\n");
        settings_save_ovl(overlay);
        return;
    }

    uint32_t file_size;
    error_t result = fsGetFileSize(config_path, &file_size);
    if (result != NO_ERROR)
    {
        TRACE_WARNING("Failed to get config file size\r\n");
        return;
    }

    FsFile *file = fsOpenFile(config_path, FS_FILE_MODE_READ);
    if (file == NULL)
    {
        TRACE_WARNING("Failed to open config file for reading\r\n");
        return;
    }

    // Buffer to hold the file content
    char buffer[256];
    size_t from_read;
    size_t read_length;
    bool last_line_incomplete = false;
    char *line;
    from_read = 0;
    while (fsReadFile(file, &buffer[from_read], sizeof(buffer) - from_read - 1, &read_length) == NO_ERROR || last_line_incomplete)
    {
        read_length = from_read + read_length;
        buffer[read_length] = '\0';

        // Process each line in the buffer
        line = buffer;
        char *next_line;

        while ((next_line = strchr(line, '\n')) != NULL)
        {
            *next_line = '\0'; // Terminate the line at the newline character

            // Skip empty lines or lines starting with a comment character '#'
            if (*line != '\0' && *line != '#')
            {
                // Split the line into option_name and value
                char *option_name = strtok(line, "=");
                char *value_str = &line[osStrlen(option_name) + 1];

                char *overlay_name = NULL;

                if (option_name != NULL && value_str != NULL)
                {
                    // Find the corresponding setting item
                    setting_item_t *opt = settings_get_by_name_ovl(option_name, overlay_name);
                    if (opt != NULL)
                    {
                        // Update the setting value based on the type
                        switch (opt->type)
                        {
                        case TYPE_BOOL:
                            if (strcmp(value_str, "true") == 0)
                                *((bool *)opt->ptr) = true;
                            else if (strcmp(value_str, "false") == 0)
                                *((bool *)opt->ptr) = false;
                            else
                                TRACE_WARNING("Invalid boolean value '%s' for setting '%s'\r\n", value_str, option_name);
                            TRACE_DEBUG("%s=%s\r\n", opt->option_name, *((bool *)opt->ptr) ? "true" : "false");
                            break;
                        case TYPE_SIGNED:
                            *((int32_t *)opt->ptr) = atoi(value_str);
                            TRACE_DEBUG("%s=%d\r\n", opt->option_name, *((int32_t *)opt->ptr));
                            break;
                        case TYPE_UNSIGNED:
                        case TYPE_HEX:
                            *((uint32_t *)opt->ptr) = strtoul(value_str, NULL, 10);
                            TRACE_DEBUG("%s=%u\r\n", opt->option_name, *((uint32_t *)opt->ptr));
                            break;
                        case TYPE_FLOAT:
                            *((float *)opt->ptr) = strtof(value_str, NULL);
                            TRACE_DEBUG("%s=%f\r\n", opt->option_name, *((float *)opt->ptr));
                            break;
                        case TYPE_STRING:
                            free(*((char **)opt->ptr));
                            *((char **)opt->ptr) = strdup(value_str);
                            TRACE_DEBUG("%s=%s\r\n", opt->option_name, *((char **)opt->ptr));
                            break;

                        default:
                            break;
                        }
                    }
                    else
                    {
                        TRACE_WARNING("Setting item '%s' not found\r\n", option_name);
                    }
                }
            }

            line = next_line + 1; // Move to the next line
        }

        if (last_line_incomplete && read_length == 0)
            break;

        // Check if the last line is incomplete (does not end with a newline character)
        last_line_incomplete = (buffer[read_length - 1] != '\n');
        if (last_line_incomplete)
        {
            from_read = strlen(line);
            memmove(buffer, line, from_read);
        }
        else
        {
            from_read = 0;
        }
    }
    fsCloseFile(file);
    settings_generate_internal_dirs(get_settings());

    if (Settings_Overlay[0].configVersion < CONFIG_VERSION)
    {
        settings_save();
    }
    Settings_Overlay[0].internal.config_changed = false;
}

setting_item_t *settings_get(int index)
{
    return settings_get_ovl(index, NULL);
}
setting_item_t *settings_get_ovl(int index, const char *overlay_name)
{
    int pos = 0;
    while (get_option_map(overlay_name)[pos].type != TYPE_END)
    {
        if (pos == index)
        {
            return &get_option_map(overlay_name)[pos];
        }
        pos++;
    }
    TRACE_WARNING("Setting item #%d not found\r\n", index);
    return NULL;
}

setting_item_t *settings_get_by_name(const char *item)
{
    return settings_get_by_name_ovl(item, NULL);
}
setting_item_t *settings_get_by_name_ovl(const char *item, const char *overlay_name)
{
    int pos = 0;
    while (get_option_map(overlay_name)[pos].type != TYPE_END)
    {
        if (!strcmp(item, get_option_map(overlay_name)[pos].option_name))
        {
            return &get_option_map(overlay_name)[pos];
        }
        pos++;
    }
    TRACE_WARNING("Setting item '%s' not found\r\n", item);
    return NULL;
}

bool settings_get_bool(const char *item)
{
    return settings_get_bool_ovl(item, NULL);
}
bool settings_get_bool_ovl(const char *item, const char *overlay_name)
{
    if (!item)
    {
        return false;
    }

    setting_item_t *opt = settings_get_by_name_ovl(item, overlay_name);
    if (!opt || opt->type != TYPE_BOOL)
    {
        return false;
    }

    return *((bool *)opt->ptr);
}

bool settings_set_bool(const char *item, bool value)
{
    if (!item)
    {
        return false;
    }

    setting_item_t *opt = settings_get_by_name(item);
    if (!opt || opt->type != TYPE_BOOL)
    {
        return false;
    }

    if (!opt->internal)
        settings_changed();
    *((bool *)opt->ptr) = value;
    return true;
}

int32_t settings_get_signed(const char *item)
{
    return settings_get_signed_ovl(item, NULL);
}
int32_t settings_get_signed_ovl(const char *item, const char *overlay_name)
{
    if (!item)
    {
        return 0;
    }

    setting_item_t *opt = settings_get_by_name_ovl(item, overlay_name);
    if (!opt || opt->type != TYPE_SIGNED)
    {
        return 0;
    }

    return *((int32_t *)opt->ptr);
}

bool settings_set_signed(const char *item, int32_t value)
{
    if (!item)
    {
        return false;
    }

    setting_item_t *opt = settings_get_by_name(item);
    if (!opt || opt->type != TYPE_SIGNED)
    {
        return false;
    }

    if (value < opt->min.signed_value || value > opt->max.signed_value)
    {
        TRACE_ERROR("  %s = %d out of bounds\r\n", opt->option_name, value);
        return false;
    }

    if (!opt->internal)
        settings_changed();
    *((int32_t *)opt->ptr) = value;
    return true;
}

uint32_t settings_get_unsigned(const char *item)
{
    return settings_get_unsigned_ovl(item, NULL);
}
uint32_t settings_get_unsigned_ovl(const char *item, const char *overlay_name)
{
    if (!item)
    {
        return 0;
    }

    setting_item_t *opt = settings_get_by_name_ovl(item, overlay_name);
    if (!opt || opt->type != TYPE_UNSIGNED)
    {
        return 0;
    }

    return *((uint32_t *)opt->ptr);
}

bool settings_set_unsigned(const char *item, uint32_t value)
{
    if (!item)
    {
        return false;
    }

    setting_item_t *opt = settings_get_by_name(item);
    if (!opt || opt->type != TYPE_UNSIGNED)
    {
        return false;
    }

    if (value < opt->min.unsigned_value || value > opt->max.unsigned_value)
    {
        TRACE_ERROR("  %s = %d out of bounds\r\n", opt->option_name, value);
        return false;
    }

    if (!opt->internal)
        settings_changed();
    *((uint32_t *)opt->ptr) = value;
    return true;
}

float settings_get_float(const char *item)
{
    return settings_get_float_ovl(item, NULL);
}
float settings_get_float_ovl(const char *item, const char *overlay_name)
{
    if (!item)
    {
        return 0;
    }

    setting_item_t *opt = settings_get_by_name_ovl(item, overlay_name);
    if (!opt || opt->type != TYPE_FLOAT)
    {
        return 0;
    }

    return *((float *)opt->ptr);
}

bool settings_set_float(const char *item, float value)
{
    if (!item)
    {
        return false;
    }

    setting_item_t *opt = settings_get_by_name(item);
    if (!opt || opt->type != TYPE_FLOAT)
    {
        return false;
    }

    if (value < opt->min.float_value || value > opt->max.float_value)
    {
        TRACE_ERROR("  %s = %f out of bounds\r\n", opt->option_name, value);
        return false;
    }

    if (!opt->internal)
        settings_changed();
    *((float *)opt->ptr) = value;
    return true;
}

const char *settings_get_string(const char *item)
{
    return settings_get_string_ovl(item, NULL);
}
const char *settings_get_string_ovl(const char *item, const char *overlay_name)
{
    if (!item)
    {
        return NULL;
    }

    setting_item_t *opt = settings_get_by_name_ovl(item, overlay_name);
    if (!opt || opt->type != TYPE_STRING)
    {
        return NULL;
    }

    return *(const char **)opt->ptr;
}

bool settings_set_string(const char *item, const char *value)
{
    if (!item || !value)
    {
        return false;
    }

    setting_item_t *opt = settings_get_by_name(item);
    if (!opt || opt->type != TYPE_STRING)
    {
        return false;
    }

    char **ptr = (char **)opt->ptr;

    if (*ptr)
    {
        free(*ptr);
    }

    if (!opt->internal)
        settings_changed();
    *ptr = strdup(value);
    return true;
}
