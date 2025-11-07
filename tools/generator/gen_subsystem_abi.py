#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import re
import os
from pathlib import Path

TEMPLATE = r'''#include "common_type.h"
#include "subsystem_abi.h"
#include <string>
#include <vector>
#include <new>

#define SUBSYS_{NAME_UPPER}_VERSION "{version}"

struct SubsystemHandle{{
    bool initialized = false;
    bool running = false;
    ConfigType  config_type;
    std::string config_path;
    ManifestType  manifest_type;
    std::string manifest_path;
}};

static int {name}_init(void *h)
{{
    if (!h)
        return SUBSYS_ERR_INVALID_ARG;
    static_cast<SubsystemHandle *>(h)->initialized = true;
    return SUBSYS_OK;
}}

static int {name}_start(void *h)
{{
    if (!h)
        return SUBSYS_ERR_INVALID_ARG;
    static_cast<SubsystemHandle *>(h)->running = true;
    return SUBSYS_OK;
}}

static int {name}_stop(void *h)
{{
    if (!h)
        return SUBSYS_ERR_INVALID_ARG;
    static_cast<SubsystemHandle *>(h)->running = false;
    return SUBSYS_OK;
}}

static int {name}_query(void *h, uint32_t code, const void *in, void *out)
{{
    (void)code;
    (void)in;
    (void)out;
    if (!h)
        return SUBSYS_ERR_INVALID_ARG;

    // TODO: handle query codes
    return SUBSYS_OK;
}}

static const SubsystemVTable A_VTABLE = {{
    /* .size = */ (uint32_t)sizeof(SubsystemVTable),
    /* .abi_version = */ {abi_version},
    /* .init =  */ &{name}_init,
    /* .start = */ &{name}_start,
    /* .stop  = */ &{name}_stop,
    /* .query = */ &{name}_query,
}};

static int {name}_registry_type(SubsystemHandle *out)
{{
    (void)out;
    // TODO: Register (e.g., register commands, events, resources)
    return SUBSYS_OK;
}}

static int {name}_create(const SubsystemParams *params, SubsystemHandle **out)
{{
    if (!out)
        return SUBSYS_ERR_INVALID_ARG;

    auto *obj = new (std::nothrow) SubsystemHandle();
    if (!obj)
        return SUBSYS_ERR;

    if (params)
    {{
        if (params->config_type)
            obj->config_type = params->config_type;
        if (params->config_path)
            obj->config_path = params->config_path;
        if (params->manifest_type)
            obj->manifest_type = params->manifest_type;
        if (params->manifest_path)
            obj->manifest_path = params->manifest_path;
    }}

    *out = obj;
    return SUBSYS_OK;
}}

static void {name}_destroy(SubsystemHandle *h)
{{
    delete h;
}}

extern "C" __attribute__((visibility("default")))
const SubsystemDescriptor *subsystem_descriptor(void)
{{
    static const SubsystemDescriptor DESC = {{
        /* .abi_version = */ {abi_version},
        /* .name        = */ "{subsystem}",
        /* .version_str = */ SUBSYS_{NAME_UPPER}_VERSION,
        /* .vtable      = */ &A_VTABLE,
        /* .registry_type = */ &{name}_registry_type,
        /* .create      = */ &{name}_create,
        /* .destroy     = */ &{name}_destroy,
    }};
    return &DESC;
}}
'''

def to_snake_case(name: str) -> str:
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    s2 = re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1)
    return re.sub(r'[\s\-]+', '_', s2).lower()

def make_file(dir: str, name: str, ext:str, content: str, style: str, force: bool):
    if(style == "snake"):
        filename = to_snake_case(name)

    file_path = f"{dir}/{filename}.{ext}"
    path = Path(file_path).resolve()
    
    if os.path.exists(path) and not force:
        print(f"SKIP: '{path}' already exists.")
        return
    
    Path(dir).mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"Creted: {path}")


def main():
    ap = argparse.ArgumentParser(description="Generate a Subsystem C++ implementation file.")
    ap.add_argument("name", help="Subsystem name (e.g., Sample)")
    ap.add_argument("-o", "--outdir", default=".", 
                    help="Output directory (default: .)")
    ap.add_argument("--version", default="1.0.0", 
                    help="version string (default: 1.0.0)")
    ap.add_argument("--abi-version", type=int, default=1, 
                    help="ABI version integer (default: 1)")
    ap.add_argument("--filename-style", choices=["snake", "pascal"], default="snake", 
                    help="Filename style (default: snake)")
    ap.add_argument("--force", action="store_true", 
                    help="Overwrite existing files.")
    args = ap.parse_args()

    name = args.name.strip()
    name_lower = name.lower()
    NAME_UPPER = name.upper()

    cpp = TEMPLATE.format(
        name=name_lower,
        NAME_UPPER=NAME_UPPER,
        subsystem=name,
        version=args.version,
        abi_version=args.abi_version,
    )
    make_file(
        dir=args.outdir,
        name=f"{args.name}_abi",
        ext="cpp",
        content=cpp,
        style=args.filename_style,
        force=args.force
    )
        

if __name__ == "__main__":
    main()
