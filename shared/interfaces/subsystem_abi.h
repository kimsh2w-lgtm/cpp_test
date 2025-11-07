#pragma once
#include <stdint.h>
#include "common_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define SUBSYS_ABI_VERSION 1

    enum
    {
        SUBSYS_OK = 0,
        SUBSYS_ERR = -1,
        SUBSYS_ERR_INCOMPATIBLE_ABI = -2,
        SUBSYS_ERR_INVALID_ARG = -3,
    };

    typedef struct SubsystemVTable
    {
        uint32_t size;        // sizeof(SubsystemVTable)
        uint32_t abi_version; 
        int (*init)(void *self);
        int (*self_test)(void *self);
        int (*configure)(void *self);
        int (*ready)(void *self);
        int (*start)(void *self);
        int (*pause)(void *self);
        int (*stop)(void *self);
        int (*recovery)(void *self);
        int (*safe)(void *self);
        int (*system_mode)(void *self, uint32_t mode);
        int  (*query)(void* self, uint32_t code, void* in, void* out);
    } SubsystemVTable;

    typedef struct SubsystemHandle SubsystemHandle;

    typedef struct SubsystemParams
    {
        ConfigType config_type;
        const char *config_path; 
        ManifestType manifest_type;
        const char *manifest_path; 
    } SubsystemParams;

    typedef struct SubsystemDescriptor
    {
        uint32_t abi_version;          
        const char *name;              
        const char *version_str;       
        const SubsystemVTable *vtable; 
        
        int (*create)(const SubsystemParams *params, SubsystemHandle **out);
        void (*destroy)(SubsystemHandle *handle);

        int (*registry)(const SubsystemParams *params);
        int (*registry_module)(const SubsystemParams *params);
    } SubsystemDescriptor;

    #define SUBSYSTEM_DESCRIPTOR_SYMBOL "subsystem_descriptor"

    typedef const SubsystemDescriptor *(*fn_subsystem_descriptor_t)(void);

#ifdef __cplusplus
}
#endif
