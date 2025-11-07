#include "common_type.h"
#include "subsystem_abi.h"
#include <string>
#include <vector>
#include <new>

#define SUBSYS_SAMPLE_VERSION "1.0.0"

struct SubsystemHandle{
    bool initialized = false;
    bool running = false;
    ConfigType  config_type;
    std::string config_path;
    ManifestType  manifest_type;
    std::string manifest_path;
};

static int sample_init(void *h)
{
    if (!h)
        return SUBSYS_ERR_INVALID_ARG;
    static_cast<SubsystemHandle *>(h)->initialized = true;
    return SUBSYS_OK;
}

static int sample_start(void *h)
{
    if (!h)
        return SUBSYS_ERR_INVALID_ARG;
    static_cast<SubsystemHandle *>(h)->running = true;
    return SUBSYS_OK;
}

static int sample_stop(void *h)
{
    if (!h)
        return SUBSYS_ERR_INVALID_ARG;
    static_cast<SubsystemHandle *>(h)->running = false;
    return SUBSYS_OK;
}

static int sample_query(void *h, uint32_t code, const void *in, void *out)
{
    (void)code;
    (void)in;
    (void)out;
    if (!h)
        return SUBSYS_ERR_INVALID_ARG;

    // TODO: handle query codes
    return SUBSYS_OK;
}

static const SubsystemVTable A_VTABLE = {
    /* .size = */ (uint32_t)sizeof(SubsystemVTable),
    /* .abi_version = */ 1,
    /* .init =  */ &sample_init,
    /* .start = */ &sample_start,
    /* .stop  = */ &sample_stop,
    /* .query = */ &sample_query,
};

static int sample_create(const SubsystemParams *params, SubsystemHandle **out)
{
    if (!out)
        return SUBSYS_ERR_INVALID_ARG;

    auto *obj = new (std::nothrow) SubsystemHandle();
    if (!obj)
        return SUBSYS_ERR;

    if (params)
    {
        if (params->config_type)
            obj->config_type = params->config_type;
        if (params->config_path)
            obj->config_path = params->config_path;
        if (params->manifest_type)
            obj->manifest_type = params->manifest_type;
        if (params->manifest_path)
            obj->manifest_path = params->manifest_path;
    }

    *out = obj;
    return SUBSYS_OK;
}

static void sample_destroy(SubsystemHandle *h)
{
    delete h;
}


static int sample_registry_type(const SubsystemParams *params)
{
    (void)out;
    // TODO: Register (e.g., register service, domain, others...)
    return SUBSYS_OK;
}



static int sample_registry_access(SubsystemHandle *out)
{
    (void)out;
    // TODO: Register access (e.g., register access layer components)
    return SUBSYS_OK;
}



extern "C" __attribute__((visibility("default")))
const SubsystemDescriptor *subsystem_descriptor(void)
{
    static const SubsystemDescriptor DESC = {
        /* .abi_version     = */ 1,
        /* .name            = */ "Sample",
        /* .version_str     = */ SUBSYS_SAMPLE_VERSION,
        /* .vtable          = */ &A_VTABLE,
        /* .create          = */ &sample_create,
        /* .destroy         = */ &sample_destroy,
        /* .registry_type   = */ &sample_registry_type,
        /* .registry_access = */ &sample_registry_access,
    };
    return &DESC;
}
