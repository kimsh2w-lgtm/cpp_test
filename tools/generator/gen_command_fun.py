#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse, os, re, shutil, sys

# ---------- list utils ----------
def _normalize_list_arg(x):
    if isinstance(x, str):
        s = x.strip()
        if s.startswith("[") and s.endswith("]"):
            s = s[1:-1]
        return [p.strip() for p in s.split(",") if p.strip()]
    return list(x)

def _fmt_list_no_space(seq):
    return "[" + ",".join(seq) + "]"

# ---------- comment blocks ----------
def make_command_annotation(command_name, allowed_modes, emit_list, desc):
    allowed_list = _normalize_list_arg(allowed_modes)
    emit_items   = _normalize_list_arg(emit_list)
    return (
        "/**\n"
        f" * @type: command\n"
        f" * @command: {command_name}\n"
        f" * @allowed_modes: {_fmt_list_no_space(allowed_list)}\n"
        f" * @emit: {_fmt_list_no_space(emit_items)}\n"
        f" * @description: {desc}\n"
        " */\n"
    )

def make_event_annotation(event_name, durability, causation, desc):
    causation_list = _normalize_list_arg(causation)
    durability_val = (durability[0] if isinstance(durability, list) and durability else durability) or "volatile"
    return (
        "/**\n"
        f" * @type: event\n"
        f" * @command: {event_name}\n"
        f" * @durability: {durability_val}\n"
        f" * @causation: {_fmt_list_no_space(causation_list)}\n"
        f" * @description: {desc}\n"
        " */\n"
    )

# ---------- class parsing ----------
def find_class_bounds(text, class_name):
    m = re.search(rf'\bclass\s+{re.escape(class_name)}\b', text)
    if not m: return None
    i = m.end()
    brace_open = text.find('{', i)
    if brace_open == -1: return None
    depth = 0
    for j in range(brace_open, len(text)):
        c = text[j]
        if c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return (m.start(), brace_open, j)
    return None

def _line_after(body, idx):
    nl = body.find('\n', idx)
    return (nl + 1) if nl != -1 else len(body)

def detect_insertion_pos(body, access_label, type_tag):
    """
    Priority:
      1) tagged: 'public: // ...{type_tag}...' (case-insensitive)
      2) first plain 'public:' (case-insensitive)
      3) none
    """
    access_pat = re.compile(
        rf'(?mi)^[ \t]*{re.escape(access_label)}[ \t]*:[ \t]*(//[^\n]*|/\*.*?\*/)?[ \t]*$',
        re.M
    )
    tagged_pos = None
    plain_pos = None
    plain_match_end = None
    for m in access_pat.finditer(body):
        comment = (m.group(1) or "")
        if type_tag and re.search(rf'\b{re.escape(type_tag)}\b', comment, flags=re.I):
            nl = body.find('\n', m.end())
            tagged_pos = (nl + 1) if nl != -1 else len(body)
            break
        if plain_pos is None:
            nl = body.find('\n', m.end())
            plain_pos = (nl + 1) if nl != -1 else len(body)
            plain_match_end = m.end()
    if tagged_pos is not None:
        return tagged_pos, 'tagged', None
    if plain_pos is not None:
        return plain_pos, 'plain', plain_match_end
    return 0, 'none', None

# ---------- signature normalization ----------
_ws = re.compile(r'\s+')

def _strip_ws(s: str) -> str:
    return _ws.sub(' ', s.strip())

def _strip_attributes(s: str) -> str:
    return re.sub(r'\[\[[^\]]*\]\]\s*', '', s)

def _normalize_cv(tokens):
    cv = [t for t in tokens if t in ('const','volatile')]
    rest = [t for t in tokens if t not in ('const','volatile')]
    return cv + rest

def _normalize_param_type(part: str) -> str:
    s = _strip_attributes(part)
    s = re.sub(r'//.*?$|/\*.*?\*/', '', s, flags=re.S)
    s = re.split(r'\s*=\s*', s, maxsplit=1)[0].strip()
    m = re.match(r'(.+?)([*&]{0,2})\s*([A-Za-z_]\w*)\s*$', s)
    if m:
        left, ptrref, maybe_name = m.groups()
        s = (left + ptrref).strip()
    tokens = [t for t in re.split(r'\s+', s) if t]
    tokens = _normalize_cv(tokens)
    return _strip_ws(' '.join(tokens))

def _only_types_inside_params(params: str) -> str:
    p = params.strip()
    if not p: return ""
    p = _strip_attributes(p)
    p = re.sub(r'//.*?$|/\*.*?\*/', '', p, flags=re.S)
    parts = [x.strip() for x in re.split(r'\s*,\s*', p) if x.strip()]
    return ", ".join(_normalize_param_type(part) for part in parts)

def _signature_key_by_parts(ret_prefix: str, fname: str, params: str, quals: str) -> str:
    ret_prefix = _strip_ws(_strip_attributes(ret_prefix))
    params_key = _only_types_inside_params(params)
    quals_s = _strip_ws(quals)
    known = []
    for q in ('const','noexcept','override','final'):
        if re.search(rf'\b{q}\b', quals_s):
            known.append(q)
    quals_key = " ".join(known)
    return _ws.sub(' ', f"{ret_prefix} {fname}({params_key}) {quals_key}".strip())

def _function_name_from_signature(signature: str) -> str:
    sig = signature.strip().rstrip(';')
    sig = _strip_attributes(sig)
    m = re.match(r'.*?\b([A-Za-z_]\w*)\s*\(', sig)
    return m.group(1) if m else ""

# ---------- duplication checks ----------
def method_exists_in_class(body: str, signature: str) -> bool:
    fname = _function_name_from_signature(signature)
    if not fname:
        return False
    m = re.match(r'(.+?)\b([A-Za-z_]\w*)\s*\((.*)\)\s*(.*)$', signature.strip().rstrip(';'))
    if not m:
        return False
    ret_prefix, _fname, params, quals = m.groups()
    target_key = _signature_key_by_parts(ret_prefix, fname, params, quals)

    pat = re.compile(rf'(?<!::)\b{re.escape(fname)}\s*\(', re.M)
    for hit in pat.finditer(body):
        start = hit.start()
        line_start = body.rfind('\n', 0, start)
        window_start = line_start + 1 if line_start != -1 else 0
        i = hit.end(); depth = 1
        while i < len(body) and depth > 0:
            if body[i] == '(':
                depth += 1
            elif body[i] == ')':
                depth -= 1
            i += 1
        params_end = i
        head = body[window_start:hit.start()]
        cut = max(head.rfind(';'), head.rfind('{'), head.rfind('}'))
        ret = head[cut+1:] if cut != -1 else head
        found_params = body[hit.end():params_end-1]
        rest = body[params_end: params_end+200]
        mquals = re.findall(r'\b(const|noexcept|override|final)\b', rest)
        cand_key = _signature_key_by_parts(ret, fname, found_params, " ".join(mquals))
        if cand_key == target_key:
            return True
    return False

def definition_exists_in_cpp(src_text, full_qualified_name, params, qualifiers):
    squash = lambda s: re.sub(r'\s+', '', _strip_attributes(s))
    base = f"{full_qualified_name}({params})"
    qtext = f"{base} {qualifiers}" if qualifiers.strip() else base
    if squash(qtext) in squash(src_text):
        return True
    pat = re.compile(rf'\b{re.escape(full_qualified_name)}\s*\(', re.M)
    for m in pat.finditer(src_text):
        i = m.end(); depth = 1
        while i < len(src_text) and depth > 0:
            if src_text[i] == '(':
                depth += 1
            elif src_text[i] == ')':
                depth -= 1
            i += 1
        params_end = i
        found_params = src_text[m.end():params_end-1]
        rest = src_text[params_end: params_end+200]
        mquals = re.findall(r'\b(const|noexcept|override|final)\b', rest)
        if _only_types_inside_params(found_params) == _only_types_inside_params(params):
            target_q = set(q.strip() for q in qualifiers.split() if q.strip())
            if set(mquals) == target_q:
                return True
    return False

# ---------- formatting ----------
def indent_block(block, indent):
    return "".join((indent + ln) if ln.strip() else ln for ln in block.splitlines(True))

# ---------- header edit ----------
def insert_into_header(header_path, cls, block, signature, type_tag):
    with open(header_path, 'r', encoding='utf-8') as f:
        text = f.read()
    bounds = find_class_bounds(text, cls)
    if not bounds:
        print(f"[ERROR] class '{cls}' not found in {header_path}", file=sys.stderr)
        sys.exit(1)
    _, brace_open, brace_close = bounds
    head = text[:brace_open+1]
    body = text[brace_open+1:brace_close]
    tail = text[brace_close:]

    if method_exists_in_class(body, signature):
        print("[INFO] Header already contains the same method declaration. Skipped header insert.")
        return False

    pos, mode, plain_match_end = detect_insertion_pos(body, "public", type_tag)

    if mode == 'tagged':
        indent = "    "
        insertion = "\n" + indent_block(block, indent) + indent + signature + ";\n"
        new_body = body[:pos] + insertion + body[pos:]
    elif mode == 'plain':
        line_start = body.rfind('\n', 0, plain_match_end) + 1
        access_line_end = body.find('\n', plain_match_end)
        access_line_end = (access_line_end + 1) if access_line_end != -1 else len(body)
        access_line = body[line_start:access_line_end]
        base_indent = re.match(r'^\s*', access_line).group(0) if access_line else ""
        tag_line = f"{base_indent}public: // {type_tag}\n"
        body = body[:line_start] + tag_line + body[line_start:]
        insert_pos = line_start + len(tag_line)
        member_indent = base_indent + "    "
        insertion = indent_block("\n" + block, member_indent) + member_indent + signature + ";\n"
        new_body = body[:insert_pos] + insertion + body[insert_pos:]
    else:
        first_line = body.splitlines(True)[:1]
        base_indent = re.match(r'^\s*', first_line[0]).group(0) if first_line else ""
        open_public   = f"\n{base_indent}public:\n"
        tagged_public = f"{base_indent}public: // {type_tag}\n"
        member_indent = base_indent + "    "
        insertion     = indent_block(block, member_indent) + member_indent + signature + ";\n"
        reopen_public = f"{base_indent}public:\n"
        new_body = open_public + tagged_public + "\n" + insertion + reopen_public + body

    shutil.copyfile(header_path, header_path + ".bak")
    with open(header_path, 'w', encoding='utf-8') as f:
        f.write(head + new_body + tail)
    print(f"[OK] Inserted declaration into header: {header_path}")
    return True

# ---------- cpp edit ----------
def append_definition_to_cpp(source_path, include_header, return_type, fqname, params, qualifiers, also_comment, block_text):
    created = False
    if not os.path.exists(source_path):
        created = True
        with open(source_path, 'w', encoding='utf-8') as f:
            f.write(f'#include "{include_header}"\n\n')
    with open(source_path, 'r', encoding='utf-8') as f:
        src = f.read()
    if definition_exists_in_cpp(src, fqname, params, qualifiers):
        print("[INFO] Source already contains the same method definition. Skipped cpp insert.")
        return False, created
    stub = ""
    if also_comment and block_text:
        stub += block_text
    sig_line = f"{return_type} {fqname}({params})"
    if qualifiers:
        sig_line += f" {qualifiers}"
    stub += sig_line + " {\n"
    if return_type.strip() != "void":
        stub += "    return {};\n"
    stub += "}\n"
    if not src.endswith('\n'):
        src += '\n'
    src += '\n' + stub + '\n'
    with open(source_path, 'w', encoding='utf-8') as f:
        f.write(src)
    print(f"[OK] Added definition to source: {source_path}")
    return True, created

# ---------- main ----------
def main():
    ap = argparse.ArgumentParser(description="Insert declaration and definition")
    ap.add_argument("--header", required=True)
    ap.add_argument("--source", required=True)
    ap.add_argument("--include", help='Header include name for the .cpp (default: basename of --header)')
    ap.add_argument("--class", dest="cls", required=True)
    ap.add_argument("-f", "--function-name", required=True)
    ap.add_argument("--return-type", default="void")
    ap.add_argument("--params", default="")
    ap.add_argument("--qualifiers", default="")
    ap.add_argument("--ns", default="")
    ap.add_argument("--class-prefix", default="")
    ap.add_argument("--type", choices=["command", "event"], default="command")
    ap.add_argument("--description", default="")
    ap.add_argument("--allowed-modes", nargs="*", default=[])
    ap.add_argument("--emit", nargs="*", default=[])
    ap.add_argument("--durability", choices=["volatile", "persistent"], default="volatile")
    ap.add_argument("--causation", nargs="*", default=[])
    ap.add_argument("--also-comment-in-cpp", action="store_true")
    args = ap.parse_args()

    block = make_command_annotation(args.function_name, args.allowed_modes, args.emit, args.description) \
        if args.type == "command" else \
        make_event_annotation(args.function_name, args.durability, args.causation, args.description)

    signature = f"{args.return_type} {args.function_name}({args.params})"
    if args.qualifiers:
        signature += f" {args.qualifiers}"

    insert_into_header(args.header, args.cls, block, signature, args.type)

    include_name = args.include if args.include else os.path.basename(args.header)
    qualifiers_ns = []
    if args.ns.strip(): qualifiers_ns.append(args.ns.strip())
    if args.class_prefix.strip(): qualifiers_ns.append(args.class_prefix.strip())
    qualifiers_ns.append(args.cls)
    fqclass = "::".join(qualifiers_ns)
    fqname = f"{fqclass}::{args.function_name}"

    append_definition_to_cpp(
        args.source, include_name,
        args.return_type, fqname, args.params, args.qualifiers,
        args.also_comment_in_cpp, block if args.also_comment_in_cpp else ""
    )

if __name__ == "__main__":
    main()
