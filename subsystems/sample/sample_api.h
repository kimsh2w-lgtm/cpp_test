#pragma once
#include "subsystem_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for types referenced in prototypes
typedef struct SubsystemHandle SubsystemHandle;
typedef struct SubsystemVTable SubsystemVTable;

// ---- 플러그인 공개 심볼(호스트가 dlsym) ----
int  sample_get_api_version(void);
const SubsystemVTable* sample_get_vtable(void);
int  sample_create(SubsystemHandle** out_handle);

#ifdef __cplusplus
} // extern "C"
#endif
