#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    // Confoig Types
    typedef enum 
    {
        CONFIG_FILE = 0,
        CONFIG_LVDB = 1,
    } ConfigType;

    // Manifest Types
    typedef enum 
    {
        MANIFEST_FILE = 0,
        MANIFEST_LVDB = 1,
    } ManifestType;

    // System Mode
    typedef enum 
    {
        MODE_NORMAL      = 0,
        MODE_PRODUCTION  = 1,
        MODE_UPDATE      = 2,
        MODE_CALIBRATION = 3,
        MODE_MAINTENANCE = 4,
    } SystemModeType;


#ifdef __cplusplus
}
#endif
