#!/usr/bin/env python3
"""
annotate_header_methods.py — Insert Doxygen comments above specified function declarations in a C++ header.

Usage examples:
  # 1) In-place annotate two methods with @brief text (backup .bak created)
  python annotate_header_methods.py include/SampleManager.hpp \
    --func "ServiceFactory:서비스 객체 생성" \
    --func "getHalFactory:Get HAL factory" \
    --in-place

  # 2) Write to a new file (no backup)
  python annotate_header_methods.py include/SampleManager.hpp -o include/SampleManager_annot.hpp \
    --func "DomainFactory:도메인 객체 생성"

  # 3) Use regex to match multiple overloads
  python annotate_header_methods.py include/Foo.hpp --func "set.*:setter 함수" --regex --in-place

Notes:
  - We match declarations (ending with ';') inside classes; free functions are ignored by default.
  - Skips functions that already have a Doxygen block (unless --overwrite-comments).
  - Creates a .bak backup when --in-place is used (unless --no-backup).
"""

import argparse
import re
from pathlib import Path
from typing import List, Tuple, Optional

# Match beginning of a Doxygen block /** ... */
RE_DOXY_START = re.compile(r'^\s*/\*\*')
RE_DOXY_END   = re.compile(r'\*/\s*$')

# Rough class/struct open and brace tracking
RE_CLASS_OPEN = re.compile(r'^\s*(class|struct)\s+([A-Za-z_]\w*)\b.*{')
RE_ACCESS     = re.compile(r'^\s*(public|private|protected)\s*:\s*$')
RE_TEMPLATE   = re.compile(r'^\s*template\s*<')

# Very forgiving method declaration matcher (ends with ';', not template)
# We will inject the target name in place of {{NAME}}
METHOD_DECL_TEMPLATE = re.compile(
    r'''^\s*
        (?:(?:virtual|static|constexpr|inline|explicit)\s+)*   # qualifiers
        [^();{}=:<>]*?                                         # return type-ish (very loose)
        \b{{NAME}}\b                                           # function name
        \s*\( [^;{}]* \)                                       # args (no nested parens)
        \s*(?:const\b|noexcept\b|\&\&?|\s)*                    # qualifiers tail
        \s*(?:override\b|final\b|\s)*                          # trailing
        \s*;                                                   # declaration end
        \s*$
    ''', re.VERBOSE
)

def compile_method_regex(name: str, is_regex: bool) -> re.Pattern:
    if is_regex:
        name_pat = name
    else:
        # exact identifier
        name_pat = re.escape(name)
    pattern_text = METHOD_DECL_TEMPLATE.pattern.replace(r'{{NAME}}', name_pat)
    return re.compile(pattern_text, re.VERBOSE)

def parse_kv_pairs(values: List[str]) -> List[Tuple[str, str]]:
    """
    Parse --func "Name:Brief text" pairs.
    If ':' is missing, brief becomes "Name function".
    """
    out = []
    for v in values:
        if ':' in v:
            k, brief = v.split(':', 1)
            out.append((k.strip(), brief.strip()))
        else:
            k = v.strip()
            out.append((k, f"{k} function"))
    return out

def prev_nonempty_index(lines: List[str], i: int) -> Optional[int]:
    j = i - 1
    while j >= 0:
        if lines[j].strip() == "":
            j -= 1
            continue
        return j
    return None

def already_has_doxygen_above(lines: List[str], i: int) -> bool:
    """
    Check if there's a /** ... */ block immediately above the declaration (skipping blank lines).
    """
    j = prev_nonempty_index(lines, i)
    if j is None:
        return False
    # If the previous non-empty line is inside a Doxygen block or the start of it, consider it present
    if RE_DOXY_START.search(lines[j]):
        return True
    # Also handle a one-line /** ... */ comment
    if '/**' in lines[j] and '*/' in lines[j]:
        return True
    # If the previous lines end with */, walk backwards until a /** is found (then assume attached)
    if '*/' in lines[j]:
        # walk back to /** within a few lines
        k = j
        found_start = False
        while k >= 0 and k >= j - 10:  # small window
            if RE_DOXY_START.search(lines[k]):
                found_start = True
                break
            k -= 1
        if found_start:
            # ensure no blank line between end of block and decl
            # i.e., previous non-empty is the block end
            return True
    return False

def make_doxygen_block(indent: str, brief: str) -> List[str]:
    return [
        f"{indent}/**",
        f"{indent} * @brief {brief}",
        f"{indent} */",
    ]

def annotate_header(
    text: str,
    targets: List[Tuple[str, str]],
    use_regex: bool,
    only_inside_classes: bool,
    overwrite_comments: bool
) -> str:
    """
    Insert Doxygen above matched declarations. Returns updated text.
    """
    lines = text.splitlines()
    # Pre-compile regexes per target
    compiled = [(compile_method_regex(name, use_regex), brief, name) for name, brief in targets]

    # Track whether we're inside a class body (brace depth from class open)
    inside_class = False
    brace_depth = 0

    i = 0
    while i < len(lines):
        line = lines[i]

        # Identify start of a class/struct with an opening brace on same line
        if RE_CLASS_OPEN.search(line):
            inside_class = True
            # initialize depth for this class body
            brace_depth += line.count('{') - line.count('}')
            i += 1
            continue

        # Track brace depth while inside class
        if inside_class:
            brace_depth += line.count('{') - line.count('}')
            if brace_depth <= 0:
                # exited the class body
                inside_class = False
                brace_depth = 0
                i += 1
                continue

            # skip access labels and templates (we don't inject there)
            if RE_ACCESS.match(line) or RE_TEMPLATE.match(line):
                i += 1
                continue

            # Try to match each target against the current (possibly multi-line) declaration
            # Assemble a small window until we hit a ';' (declaration end)
            if ';' not in line:
                # If line doesn't end a declaration, peek ahead a few lines to match combined
                lookahead_buf = [line]
                j = i + 1
                while j < len(lines) and ';' not in lines[j] and (j - i) <= 8:
                    lookahead_buf.append(lines[j])
                    j += 1
                if j < len(lines):
                    lookahead_buf.append(lines[j])
                probe = " ".join(s.strip() for s in lookahead_buf)
                # Try match compiled targets against the concatenated probe
                for regex, brief, name_raw in compiled:
                    if regex.match(probe):
                        # Found a declaration spanning i..j; annotate at i
                        # Determine indentation from the first line containing non-space
                        indent = re.match(r'^(\s*)', lines[i]).group(1)
                        if not overwrite_comments and already_has_doxygen_above(lines, i):
                            pass
                        else:
                            # Insert block above i
                            block = make_doxygen_block(indent, brief)
                            lines[i:i] = block
                            i += len(block)  # move past inserted block
                        break
                # Move to the end of this chunk
                i = j + 1 if j > i else i + 1
                continue
            else:
                # Single-line declaration; test directly
                for regex, brief, name_raw in compiled:
                    if regex.match(line.strip()):
                        indent = re.match(r'^(\s*)', line).group(1)
                        if not overwrite_comments and already_has_doxygen_above(lines, i):
                            break
                        block = make_doxygen_block(indent, brief)
                        lines[i:i] = block
                        i += len(block) + 1
                        break
                else:
                    i += 1
                continue
        else:
            i += 1

    return "\n".join(lines) + ("\n" if not text.endswith("\n") else "")

def main():
    ap = argparse.ArgumentParser(description="Insert Doxygen @brief above specified methods in a header.")
    ap.add_argument("header", help="Path to the header file (.h/.hpp)")
    ap.add_argument("-o", "--output", help="Output file (default: print or in-place if --in-place)")
    ap.add_argument("--func", action="append", default=[],
                    help='Function target in form "Name:Brief text". Repeatable. '
                         'If :Brief is omitted, uses "Name function".')
    ap.add_argument("--regex", action="store_true",
                    help="Treat function Name as a regular expression (matches identifiers).")
    ap.add_argument("--include-free", action="store_true",
                    help="Also annotate free functions (top-level). Default is inside classes only.")
    ap.add_argument("--overwrite-comments", action="store_true",
                    help="If a Doxygen block already exists above a matched declaration, replace it.")
    ap.add_argument("--in-place", action="store_true",
                    help="Edit the file in place (creates .bak backup unless --no-backup).")
    ap.add_argument("--no-backup", action="store_true",
                    help="When using --in-place, do not create a .bak backup.")
    args = ap.parse_args()

    if not args.func:
        raise SystemExit("No --func provided. Use --func \"Method:Brief\" (repeatable).")

    header_path = Path(args.header)
    if not header_path.exists():
        raise SystemExit(f"Header not found: {header_path}")

    targets = parse_kv_pairs(args.func)
    src_text = header_path.read_text(encoding="utf-8", errors="ignore")

    updated = annotate_header(
        text=src_text,
        targets=targets,
        use_regex=args.regex,
        only_inside_classes=not args.include_free,
        overwrite_comments=args.overwrite_comments
    )

    if args.in-place:
        if not args.no_backup:
            bak = header_path.with_suffix(header_path.suffix + ".bak")
            bak.write_text(src_text, encoding="utf-8")
        header_path.write_text(updated, encoding="utf-8")
        print(f"Updated in place: {header_path}")
        return

    if args.output:
        Path(args.output).write_text(updated, encoding="utf-8")
        print(f"Wrote: {args.output}")
        return

    # Default: print to stdout
    print(updated, end="")

if __name__ == "__main__":
    main()
