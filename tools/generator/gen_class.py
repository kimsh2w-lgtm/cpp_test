#!/usr/bin/env python3
"""
gen_class.py — Generate C++ header (.hpp) and source (.cpp) with optional inheritance.

Examples:
  # Minimal
  python gen_class.py SampleManager -n sample

  # With bases (multiple inheritance) and override on all methods
  python gen_class.py SampleManager -n sample \
    --bases "public ISubsystemManager, private detail::Noncopyable" \
    --methods "void ServiceFactory();,void DomainFactory();,void AccessFactory();,IHalFactory getHalFactory() const;" \
    --override-all

  # With base-specific includes and virtual destructor
  python gen_class.py Controller -n sys \
    --bases "public IController" --base-includes "IController.hpp" \
    --methods "void tick();,int state() const;" --virtual-dtor
"""
import argparse
import os
import re
from pathlib import Path

HEADER_TEMPLATE = """\
#pragma once

{includes}

namespace {namespace} {{

class {classname}{base_clause} {{
public:
    {classname}();
    {dtor_sig}

{method_decls}
private:
    int value_;
}};

}} // namespace {namespace}

"""

CPP_TEMPLATE = """#include "{header_filename}"

namespace {namespace} {{

{classname}::{classname}() = default;
{classname}::{dtor_name} = default;

{method_defs}

}} // namespace {namespace}
"""

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


def to_header_includes(std_includes, user_includes):
    lines = []
    for h in sorted(set(i.strip() for i in (std_includes or []) if i.strip())):
        lines.append(f"#include <{h}>")
    for h in sorted(set(i.strip() for i in (user_includes or []) if i.strip())):
        # user_includes are quoted headers (project headers)
        lines.append(f"#include \"{h}\"")
    return "\n".join(lines) if lines else "// (no additional includes)"

METHOD_PATTERN = re.compile(
    r'^\s*(?P<ret>[^();{}]+?)\s+(?P<name>[A-Za-z_]\w*)\s*\((?P<args>[^()]*)\)\s*(?P<cv>const)?\s*;\s*$'
)

def split_top_level_commas(s: str):
    if not s:
        return []
    parts, buf, depth = [], [], 0
    for ch in s:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth = max(0, depth - 1)
        if ch == ',' and depth == 0:
            parts.append(''.join(buf))
            buf = []
        else:
            buf.append(ch)
    if buf:
        parts.append(''.join(buf))
    return parts

def normalize_methods(methods_raw: str, add_override: bool):
    """
    Parse a comma-separated list of method declarations. Return list of dicts.
    Keys: ret, name, args, cv, decl, defn. If parse fails, defn=None (kept in header).
    """
    methods = []
    if not methods_raw:
        return methods
    for p in split_top_level_commas(methods_raw):
        s = p.strip()
        if not s:
            continue
        if not s.endswith(';'):
            s += ';'
        m = METHOD_PATTERN.match(s)
        if not m:
            methods.append({"ret": None, "name": None, "args": None, "cv": None, "decl": s, "defn": None})
            continue
        ret = ' '.join(m.group('ret').split())
        name = m.group('name')
        args = ' '.join(m.group('args').split()) if m.group('args') else ''
        cv = ' const' if m.group('cv') else ''
        decl = f"{ret} {name}({args}){cv}"
        if add_override:
            # Avoid duplicate override if the user already added it
            if not decl.strip().endswith(" override"):
                decl += " override"
        decl += ";"
        methods.append({"ret": ret, "name": name, "args": args, "cv": cv, "decl": decl, "defn": None})
    return methods

def make_defn(classname, m):
    if not m.get("name") or not m.get("ret"):
        return None
    ret, name = m["ret"].strip(), m["name"]
    args, cv = m["args"] or "", m["cv"] or ""
    signature = f"{ret} {classname}::{name}({args}){cv}"
    body = "    // TODO: implement\n"
    if re.match(r'\bvoid\b', ret):
        return f"{signature} {{\n{body}}}\n"
    default = "0"
    if "bool" in ret:
        default = "false"
    elif "std::string" in ret or ("basic_string" in ret):
        default = 'std::string{}'
    elif ret.endswith('*'):
        default = "nullptr"
    elif re.search(r'\b(unique_ptr|shared_ptr|optional|vector|map|set|pair|tuple)\b', ret):
        default = f"{ret}{{}}"
    return f"{signature} {{\n{body}    return {default};\n}}\n"

def ensure_string_include(includes_set, methods):
    need_string = any(
        (m.get("ret") and "std::string" in m["ret"]) or
        (m.get("args") and "std::string" in m["args"])
        for m in methods
    )
    if need_string:
        includes_set.add("string")

def parse_bases(bases_raw: str):
    """
    Input like: "public ISubsystemManager, private detail::Noncopyable"
    Returns:
      - base_clause string: " : public ISubsystemManager, private detail::Noncopyable" or ""
    """
    if not bases_raw or not bases_raw.strip():
        return ""
    parts = [b.strip() for b in bases_raw.split(",") if b.strip()]
    # Basic validation: if user did not include access specifier, default to public
    fixed = []
    for b in parts:
        if re.match(r'^(public|private|protected)\s+', b):
            fixed.append(b)
        else:
            fixed.append(f"public {b}")
    return " : " + ", ".join(fixed)

def generate_files(
    classname: str,
    namespace: str,
    outdir: str,
    header_ext: str,
    methods_raw: str,
    std_includes_csv: str,
    base_includes_csv: str,
    bases_raw: str,
    force: bool,
    override_all: bool,
    virtual_dtor: bool,
    final_class: bool,
    file_style: str
):
    if(file_style == "snake"):
        filename = to_snake_case(classname)

    header_filename = os.path.join(outdir, f"{filename}.hpp" if header_ext == "hpp" else f"{filename}.h")

    # Methods
    methods = normalize_methods(methods_raw, add_override=override_all)
    std_includes = set([h.strip() for h in (std_includes_csv or "").split(",") if h.strip()])
    ensure_string_include(std_includes, methods)
    user_includes = [h.strip() for h in (base_includes_csv or "").split(",") if h.strip()]

    includes = to_header_includes(sorted(std_includes), user_includes)
    

    method_decls = "\n".join(f"    {m['decl']}" for m in methods) if methods else ""
    base_clause = parse_bases(bases_raw)

    dtor_sig = f"~{classname}();" if not virtual_dtor else f"virtual ~{classname}();"
    dtor_name = f"~{classname}()"  # for cpp template

    class_decl_name = classname + (" final" if final_class else "")
    header_content = HEADER_TEMPLATE.format(
        includes=includes,
        namespace=namespace,
        classname=class_decl_name,
        base_clause=base_clause,
        dtor_sig=dtor_sig,
        method_decls=method_decls
    )

    method_defs = []
    for m in methods:
        d = make_defn(classname, m)
        if d:
            method_defs.append(d.rstrip())
    cpp_content = CPP_TEMPLATE.format(
        header_filename=Path(header_filename).name,
        namespace=namespace,
        classname=classname,
        dtor_name=dtor_name,
        method_defs="\n\n".join(method_defs)
    )

    make_file(
        dir=outdir,
        name=classname,
        ext="hpp" if header_ext == "hpp" else "h",
        content=header_content,
        style=file_style,
        force=force
    ) 
    
    make_file(
        dir=outdir,
        name=classname,
        ext="cpp",
        content=cpp_content,
        style=file_style,
        force=force
    ) 
    

def main():
    ap = argparse.ArgumentParser(description="Generate C++ class header/source pair (with optional inheritance).")
    ap.add_argument("classname", help="Class name (e.g., SampleClass)")
    ap.add_argument("-n", "--namespace", default="sample", help="C++ namespace (default: sample)")
    ap.add_argument("-o", "--outdir", default=".", help="Output directory (default: .)")
    ap.add_argument("--filename-style", choices=["snake", "pascal"], default="snake", help="Filename style (default: snake)")
    ap.add_argument("--header-ext", choices=["h", "hpp"], default="hpp", help="Header extension (default: hpp)")
    ap.add_argument("--methods",
                    help="Comma-separated method declarations WITH ';'. "
                         "Example: \"void foo();,int bar() const;\"")
    ap.add_argument("--includes",
                    help="Comma-separated standard headers to include (no angle brackets), e.g. \"string,vector\"")
    ap.add_argument("--base-includes",
                    help="Comma-separated project headers to include for bases, e.g. \"ISubsystemManager.hpp,Noncopyable.hpp\"")
    ap.add_argument("--bases",
                    help="Comma-separated base classes with optional access, e.g. "
                         "\"public IInterface, private detail::Noncopyable\"")
    ap.add_argument("--override-all", action="store_true",
                    help="Append 'override' to all parsed method declarations.")
    ap.add_argument("--virtual-dtor", action="store_true",
                    help="Make the destructor virtual.")
    ap.add_argument("--final", dest="final_class", action="store_true",
                    help="Mark the class 'final'.")
    ap.add_argument("--force", action="store_true", help="Overwrite existing files.")
    args = ap.parse_args()

    # Remove ctor/dtor from --methods if user mistakenly added them
    if args.methods:
        cleaned = []
        for piece in split_top_level_commas(args.methods):
            s = piece.strip()
            if not s:
                continue
            if re.search(rf'\b{re.escape(args.classname)}\s*\(', s) or re.search(rf'~\s*{re.escape(args.classname)}\s*\(', s):
                continue
            cleaned.append(s)
        args.methods = ",".join(cleaned)

    generate_files(
        classname=args.classname,
        namespace=to_snake_case(args.namespace),
        outdir=args.outdir,
        header_ext=args.header_ext,
        methods_raw=args.methods or "",
        std_includes_csv=args.includes or "",
        base_includes_csv=args.base_includes or "",
        bases_raw=args.bases or "",
        force=args.force,
        override_all=args.override_all,
        virtual_dtor=args.virtual_dtor,
        final_class=args.final_class,
        file_style=args.filename_style
    )

if __name__ == "__main__":
    main()
