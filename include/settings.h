#include <stdbool.h>
#include <stdint.h>

#ifndef SETTINGS_H
#define SETTINGS_H

typedef struct
{
    bool enabled;
    bool enableV1Claim;
    bool enableV1FreshnessCheck;
    bool enableV1Log;
    bool enableV1Time;
    bool enableV1Ota;
    bool enableV2Content;
    bool cacheContent;
} settings_cloud_t;

typedef struct
{
    bool overrideCloud;
    uint32_t max_vol_spk;
    uint32_t max_vol_hdp;
    bool slap_enabled;
    bool slap_back_left;
    uint32_t led;
} settings_toniebox_t;

typedef struct
{
    bool exit;
    int32_t returncode;
} settings_internal_t;

typedef struct
{
    settings_cloud_t cloud;
    settings_toniebox_t toniebox;
    settings_internal_t internal;
} settings_t;

typedef enum
{
    TYPE_BOOL,
    TYPE_SIGNED,
    TYPE_UNSIGNED,
    TYPE_HEX,
    TYPE_STRING,
    TYPE_FLOAT,
    TYPE_END
} settings_type;

typedef union
{
    bool bool_value;
    int32_t signed_value;
    uint32_t unsigned_value;
    uint32_t hex_value;
    float float_value;
} setting_value_t;

/**
 * @typedef struct setting_item_t
 * @brief Structure representing a settings item.
 *
 * This structure encapsulates all the relevant information for a setting in the system,
 * such as its name, description, value pointer, type, initial value, range (min, max) and whether it is an internal setting.
 *
 * @var const char *option_name
 * The name of the setting item.
 *
 * @var const char *description
 * A description of the setting item, which can be used to give users an understanding of its purpose.
 *
 * @var void *ptr
 * A pointer to the value of the setting. The type of the value pointed to should match the specified setting type.
 *
 * @var settings_type type
 * The type of the setting item's value. This determines how the value should be interpreted.
 *
 * @var setting_value_t init
 * The initial value for the setting item. This is used when initializing the settings subsystem.
 *
 * @var setting_value_t min
 * The minimum allowable value for the setting item. This is used to enforce boundaries on setting values.
 *
 * @var setting_value_t max
 * The maximum allowable value for the setting item. This is used to enforce boundaries on setting values.
 *
 * @var bool internal
 * If true, this setting is intended for internal use by the system and should not be exposed to end users.
 */
typedef struct
{
    const char *option_name;
    const char *description;
    void *ptr;
    settings_type type;
    setting_value_t init;
    setting_value_t min;
    setting_value_t max;
    bool internal;
} setting_item_t;

#define OPTION_START() setting_item_t option_map[] = {
#define OPTION_ADV_BOOL(o, p, d, desc, i) {.option_name = o, .ptr = p, .init = {.bool_value = d}, .type = TYPE_BOOL, .description = desc, .internal = i},
#define OPTION_ADV_SIGNED(o, p, d, minVal, maxVal, desc, i) {.option_name = o, .ptr = p, .init = {.signed_value = d}, .min = {.signed_value = minVal}, .max = {.signed_value = maxVal}, .type = TYPE_SIGNED, .description = desc, .internal = i},
#define OPTION_ADV_UNSIGNED(o, p, d, minVal, maxVal, desc, i) {.option_name = o, .ptr = p, .init = {.unsigned_value = d}, .min = {.unsigned_value = minVal}, .max = {.unsigned_value = maxVal}, .type = TYPE_UNSIGNED, .description = desc, .internal = i},
#define OPTION_ADV_FLOAT(o, p, d, minVal, maxVal, desc, i) {.option_name = o, .ptr = p, .init = {.float_value = d}, .min = {.float_value = minVal}, .max = {.float_value = maxVal}, .type = TYPE_FLOAT, .description = desc, .internal = i},

#define OPTION_BOOL(o, p, d, desc) OPTION_ADV_BOOL(o, p, d, desc, false)
#define OPTION_SIGNED(o, p, d, min, max, desc) OPTION_ADV_SIGNED(o, p, d, min, max, desc, false)
#define OPTION_UNSIGNED(o, p, d, min, max, desc) OPTION_ADV_UNSIGNED(o, p, d, min, max, desc, false)
#define OPTION_FLOAT(o, p, d, min, max, desc) OPTION_ADV_FLOAT(o, p, d, min, max, desc, false)

#define OPTION_INTERNAL_BOOL(o, p, d, desc) OPTION_ADV_BOOL(o, p, d, desc, true)
#define OPTION_INTERNAL_SIGNED(o, p, d, min, max, desc) OPTION_ADV_SIGNED(o, p, d, min, max, desc, true)
#define OPTION_INTERNAL_UNSIGNED(o, p, d, min, max, desc) OPTION_ADV_UNSIGNED(o, p, d, min, max, desc, true)
#define OPTION_INTERNAL_FLOAT(o, p, d, min, max, desc) OPTION_ADV_FLOAT(o, p, d, min, max, desc, true)

#define OPTION_END()     \
    {                    \
        .type = TYPE_END \
    }                    \
    }                    \
    ;

extern settings_t Settings;

/**
 * @brief Initializes the settings subsystem.
 *
 * This function should be called once, before any other settings functions are used.
 */
void settings_init();

/**
 * @brief Gets the setting item at a specific index.
 *
 * @param index The index of the setting item.
 * @return A pointer to the setting item, or NULL if the index is out of bounds.
 */
setting_item_t *settings_get(int index);

/**
 * @brief Sets the value of a boolean setting item.
 *
 * @param item The name of the setting item.
 * @param value The new value for the setting item.
 */
bool settings_set_bool(const char *item, bool value);

/**
 * @brief Gets the value of a boolean setting item.
 *
 * @param item The name of the setting item.
 * @return The current value of the setting item. If the item does not exist or is not a boolean, the behavior is undefined.
 */
bool settings_get_bool(const char *item);

/**
 * @brief Gets the value of an signed integer setting item.
 *
 * @param item The name of the setting item.
 * @return The current value of the setting item. If the item does not exist or is not an integer, the behavior is undefined.
 */
int32_t settings_get_signed(const char *item);

/**
 * @brief Sets the value of an integer setting item.
 *
 * @param item The name of the setting item.
 * @param value The new value for the setting item.
 */
bool settings_set_signed(const char *item, int32_t value);

/**
 * @brief Gets the value of an unsigned integer setting item.
 *
 * @param item The name of the setting item.
 * @return The current value of the setting item. If the item does not exist or is not an integer, the behavior is undefined.
 */
uint32_t settings_get_unsigned(const char *item);

/**
 * @brief Sets the value of an unsigned integer setting item.
 *
 * @param item The name of the setting item.
 * @param value The new value for the setting item.
 */
bool settings_set_unsigned(const char *item, uint32_t value);

/**
 * @brief Retrieves a setting item by its name.
 *
 * @param item The name of the setting item.
 * @return A pointer to the setting item, or NULL if the item does not exist.
 */
setting_item_t *settings_get_by_name(const char *item);

/**
 * @brief Retrieves the value of a string setting item.
 *
 * @param item The name of the setting item.
 * @return The current string value of the setting item. If the item does not exist or is not a string, the behavior is undefined.
 */
const char *settings_get_string(const char *item);

/**
 * @brief Sets the value of a string setting item.
 *
 * @param item The name of the setting item.
 * @param value The new string value for the setting item.
 */
bool settings_set_string(const char *item, const char *value);

/**
 * @brief Retrieves the value of a floating point setting item.
 *
 * @param item The name of the setting item.
 * @return The current float value of the setting item. If the item does not exist or is not a float, the behavior is undefined.
 */
float settings_get_float(const char *item);

/**
 * @brief Sets the value of a floating point setting item.
 *
 * @param item The name of the setting item.
 * @param value The variable where the floating point value of the setting item will be stored. If the item does not exist or is not a float, the behavior is undefined.
 */
bool settings_set_float(const char *item, float value);

#endif