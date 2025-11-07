#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re
import yaml
import argparse
from pathlib import Path

def to_snake_case(name: str) -> str:
    # 대문자 앞에 밑줄 추가 (단, 맨 앞은 제외)
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    # 소문자 + 대문자 → 소문자_대문자 패턴으로 변환
    s2 = re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1)
    # 공백이나 특수문자는 밑줄로 치환
    return re.sub(r'[\s\-]+', '_', s2).lower()

def _parse_list(value: str):
    """
    '@allowed_modes' 같은 필드에 대해
    - [a, b, c] 형태를 우선 지원 (literal_eval)
    - 'a, b, c' 형태도 백업 파서로 지원
    """
    s = value.strip()
    if not s:
        return []
    
    # 1) 대괄호로 감싸진 경우: YAML로 파싱 (따옴표 없어도 됨)
    if s.startswith('[') and s.endswith(']'):
        try:
            v = yaml.safe_load(s)  # e.g. [normal, low_power]
            if isinstance(v, (list, tuple)):
                return [str(x).strip() for x in v if str(x).strip()]
        except Exception:
            # YAML 파싱 실패 시 CSV fallback로 진행
            pass


    # 2) CSV/커스텀 구분자 fallback
    parts = re.split(r'[,\|;]+', s)
    return [p.strip() for p in parts if p.strip()]

def parse_header(header_text: str, subsystem: str, default_modes):
    """
    헤더 파일 안에서 /** ... */ 주석 + 'ret name(params);' 시그니처를 페어로 찾음
    주석 내 태그:
      @type: command
      @command: SampleCommand
      @description: sample command
      @allowed_modes: [normal, low_power, diagnostics]
      @emit: ev.x, ev.y
    """
    # 주석 블록 + 그 다음 함수 선언(세미콜론까지) 매칭
    pattern = re.compile(
        r"/\*\*(.*?)\*/\s*([^\(;]+)\(([^)]*)\)\s*;",
        re.DOTALL | re.MULTILINE
    )

    items = []
    for m in pattern.finditer(header_text):
        comment_block, func_decl, params = m.groups()

        # 주석 태그 파싱
        meta = {}
        for raw in comment_block.splitlines():
            line = raw.strip(" *\t\r\n")
            if line.startswith("@"):
                key, _, val = line.partition(":")
                meta[key.strip("@").strip()] = val.strip()

        if meta.get("type", "").lower() != "command":
            continue  # command만 대상

        # 함수 이름
        fn_match = re.search(r"\b([A-Za-z_]\w*)\s*$", func_decl.strip())
        func_name = fn_match.group(1) if fn_match else meta.get("command", "").strip() or "UnknownCommand"
        func_name_snake = to_snake_case(func_name)

        # 파라미터(첫 번째) 타입 추출
        param_type = "void"
        params_s = params.strip()
        if params_s and params_s != "void":
            # 첫 파라미터 타입만 간단 파싱 (템플릿/참조/포인터 가볍게 처리)
            first = params_s.split(",")[0].strip()
            # 예: 'const SampleModel& model' → 'const SampleModel&'
            # 괄호/기호 제거는 최소화
            # 타입은 토큰에서 마지막 식별자 앞까지 취급
            # 가장 단순/견고하게는 변수명 제거:
            #   뒤에서부터 첫 공백 기준 좌측을 타입으로 간주
            if " " in first:
                param_type = first.rsplit(" ", 1)[0].strip()
            else:
                param_type = first  # 변수명 생략 케이스

        # allowed_modes
        allowed_raw = meta.get("allowed_modes", "")
        allowed_modes = _parse_list(allowed_raw) if allowed_raw else list(default_modes)

        # emits (질문 예시에 맞춰 키 이름 'emit' 사용)
        emit_raw = meta.get("emit", "")
        emits = _parse_list(emit_raw)

        description = meta.get("description", "")
        subsystem_name = subsystem.strip() if subsystem.strip() != "" else "default"
        subsystem_name_snake = to_snake_case(subsystem_name)

        item = {
            "id": f"cmd.{subsystem_name_snake}.{func_name_snake}",
            "name": func_name,
            "allowed_modes": allowed_modes,
            "request": {"type": param_type if param_type != "void" else "void"},
            "emit": emits,
            "description": description
        }
        items.append(item)

    return items

class FlowList(list):
    pass

def mark_flow_lists(obj, keys=('modes', 'allowed_modes')):
    """
    Recursively walk a dict/list structure.
    For any dict key in `keys` whose value is a list, wrap it in FlowList.
    """
    if isinstance(obj, dict):
        newd = {}
        for k, v in obj.items():
            v2 = mark_flow_lists(v, keys)
            if k in keys and isinstance(v2, list) and not isinstance(v2, FlowList):
                v2 = FlowList(v2)
            newd[k] = v2
        return newd
    elif isinstance(obj, list):
        return [mark_flow_lists(x, keys) for x in obj]
    else:
        return obj

def _represent_flow_list(dumper, data):
    return dumper.represent_sequence('tag:yaml.org,2002:seq', data, flow_style=True)

def main():
    ap = argparse.ArgumentParser(description="Parse C++ header comments to manifest YAML")
    ap.add_argument("header", help="입력 헤더 파일 (예: sample_service.hpp)")
    ap.add_argument("-o", "--output", default="sample_service_command.yaml", help="출력 YAML 파일")
    ap.add_argument("--version", type=int, default=1)
    ap.add_argument("--subsystem", default="", help="Command system")
    ap.add_argument("--modes", default="normal,diagnostics,provisioning,recovery,low_power",
                    help="상단 modes 기본값(콤마구분)")
    args = ap.parse_args()

    header_path = Path(args.header)
    header_text = header_path.read_text(encoding="utf-8")

    # service = 헤더 파일명(확장자 제거)
    service_name = header_path.stem  # e.g. sample_service.hpp -> sample_service
    subsystem = args.subsystem.strip() if args.subsystem.strip() != "" else service_name

    default_modes = [m.strip() for m in args.modes.split(",") if m.strip()]

    commands = parse_header(header_text, subsystem, default_modes)

    manifest = {
        "version": int(args.version),
        "modes": default_modes,
        "subsystem": subsystem,
        "commands": commands
    }

    yaml.add_representer(FlowList, _represent_flow_list, Dumper=yaml.SafeDumper)

    manifest_flow = mark_flow_lists(manifest, keys=("modes", "allowed_modes"))

    out_path = Path(args.output)
    out_path.write_text(yaml.dump(
        manifest_flow, 
        Dumper=yaml.SafeDumper,
        sort_keys=False, 
        allow_unicode=True, 
        default_flow_style=False), 
    encoding="utf-8")
    print(f"[OK] manifest written: {out_path}")

if __name__ == "__main__":
    main()
