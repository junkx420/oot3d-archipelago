#!/usr/bin/env python3
"""Map OoT3D_Randomizer linker symbols onto the executed TriAevum code image.

OoT3D_Randomizer's ``code/oot.ld`` lists every vanilla symbol and patch site
with its USA address and piecewise EUR offsets (``_LD_OFF = _LD_EUR ? X``).
TriAevum executes the EUR code image (SHA-256 16a6b0aa...) for both regions,
so the EUR address is the execution address.

The script never copies code bytes into its output. Each function or patch
site is recorded with the SHA-256 of its first 32 bytes in the local, private
code image, so a startup self-check can detect a different image. Additional
evidence per entry:

* ``aot_function_entry``: the address is a function entry in TriAevum's own,
  independently discovered whole-AOT function selection.
* ``arm_prologue``: the first instruction is a PUSH/STMFD that saves LR, a
  single-register LR push, or a SUB SP.
* ``block_after_call`` / ``block_after_branch``: a client hook block
  (CLIENT_HOOK_BLOCKS) starts right after a BL or another branch, where every
  execution mode starts a block, at a fixed distance from the randomizer's
  patch site.
* ``branch_target``: a client hook block is the target of the branch at a
  fixed distance from the patch site.

An entry without either is reported as unverified, not dropped.

Usage:
  python map_rando_addresses.py --ld <oot.ld> --code <private code.bin>
      --functions tools/oot3d/native_a32_runtime/whole_aot_functions.json
      --output tools/oot3d/archipelago/addresses.json
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

CODE_BASE = 0x00100000
EXPECTED_CODE_SHA256 = "16a6b0aa4c4784680220a6f780f7f8a73cfb205557aa9f9f0e705179e0613220"
# oot.ld: "0x4C99A7: End of vanilla code in .text section" (USA numbering).
USA_TEXT_END = 0x004C99A8
SIGNATURE_BYTES = 32

OFFSET_RE = re.compile(r"_LD_OFF\s*=\s*_LD_EUR\s*\?\s*(-?0x[0-9A-Fa-f]+|-?\d+)\s*:\s*_LD_OFF\s*;")
SYMBOL_RE = re.compile(r"^\s*(\w+)\s*=\s*0x([0-9A-Fa-f]+)\s*\+\s*_LD_OFF\s*;")
# Patch sites the client observes through a block near them: a block-entry
# hook needs a block start. Name -> (patch site name, block start relative to
# the site, evidence). Evidence: "block_after_call" or "block_after_branch"
# (the instruction before the block is a BL or any branch, after which every
# execution mode starts a block), or ("branch_target", offset): the branch at
# site + offset targets the block.
CLIENT_HOOK_BLOCKS = {
    # Link's death without a fairy: the game-over state is set just before.
    "GameOverStartBlock": ("GameOverStart", -4, "block_after_call"),
    # Quick text (OoT3D_Randomizer message.c, settings.c Settings_IsTurboText).
    # The block before the Message_CheckForControlCodes call: instant text.
    "QuickTextCheckControlCodeBlock": ("CheckForTextControlCode", -8, "block_after_branch"),
    # The block that dispatches a control code: unskippable and instant-off.
    "QuickTextHandleControlCodeBlock": ("HandleTextControlCode", -4, "block_after_branch"),
    # A child blue warp's scene change (OoT3D_Randomizer cutscenes.c,
    # Cutscene_BlueWarpOverride): the site itself starts a block.
    "ChildBlueWarpBlock": ("ChildBlueWarpOverride", 0, "block_after_branch"),
}


def is_branch(word: int) -> bool:
    """B, BL (any condition) or BX/BLX register."""
    return (word & 0x0E000000) == 0x0A000000 or (word & 0x0FFFFFD0) == 0x012FFF10


def branch_target(word: int, address: int) -> int | None:
    if (word & 0x0E000000) != 0x0A000000:
        return None
    offset = word & 0x00FFFFFF
    if offset & 0x00800000:
        offset -= 0x01000000
    return address + 8 + offset * 4

PATCH_RE = re.compile(r"^\s*\.patch_(\w+)\s+0x([0-9A-Fa-f]+)\s*\+\s*_LD_OFF\s*:")


def parse_ld(text: str) -> list[dict]:
    entries = []
    eur_offset = 0
    for line_number, line in enumerate(text.splitlines(), 1):
        if (match := OFFSET_RE.search(line)) is not None:
            eur_offset = int(match.group(1), 0)
            continue
        if (match := SYMBOL_RE.match(line)) is not None:
            name, kind = match.group(1), "symbol"
        elif (match := PATCH_RE.match(line)) is not None:
            name, kind = match.group(1), "patch_site"
        else:
            continue
        usa = int(match.group(2), 16)
        entries.append({
            "name": name,
            "kind": kind,
            "usa_ref": usa,
            "eur_offset": eur_offset,
            "exec_addr": usa + eur_offset,
            "ld_line": line_number,
        })
    return entries


def is_prologue(word: int) -> bool:
    if (word & 0xFFFF0000) == 0xE92D0000 and word & (1 << 14):
        return True  # STMFD SP!, {..., LR}
    if word == 0xE52DE004:
        return True  # STR LR, [SP, #-4]!
    if (word & 0xFFFFF000) == 0xE24DD000:
        return True  # SUB SP, SP, #imm
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--ld", type=Path, required=True)
    parser.add_argument("--code", type=Path, required=True)
    parser.add_argument("--functions", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rando-commit", default="")
    arguments = parser.parse_args()

    code = arguments.code.read_bytes()
    code_sha256 = hashlib.sha256(code).hexdigest()
    if code_sha256 != EXPECTED_CODE_SHA256:
        print(f"error: code image {code_sha256} is not the EUR execution image", file=sys.stderr)
        return 1
    selection = json.loads(arguments.functions.read_text(encoding="utf-8"))
    aot_entries = {int(function["entry"]) for function in selection["functions"]}

    entries = parse_ld(arguments.ld.read_text(encoding="utf-8"))
    counts = {"verified": 0, "unverified": 0}
    for entry in entries:
        address = entry["exec_addr"]
        offset = address - CODE_BASE
        is_code = entry["usa_ref"] < USA_TEXT_END
        entry["section"] = "text" if is_code else "data"
        evidence = ["oot3d_randomizer_ld_eur_offset"]
        if is_code:
            in_range = address % 4 == 0 and 0 <= offset <= len(code) - SIGNATURE_BYTES
            if in_range:
                window = code[offset:offset + SIGNATURE_BYTES]
                entry["signature"] = "sha256:" + hashlib.sha256(window).hexdigest()
                if entry["kind"] == "symbol":
                    if address in aot_entries:
                        evidence.append("aot_function_entry")
                    if is_prologue(struct.unpack_from("<I", code, offset)[0]):
                        evidence.append("arm_prologue")
            else:
                evidence.append("out_of_image")
            # Patch sites are mid-function by design; the window hash only
            # pins them to this image and is no evidence of the mapping.
            strong = entry["kind"] == "symbol" and len(evidence) > 1 and "out_of_image" not in evidence
        else:
            # Data addresses carry no code signature; their offsets are the
            # randomizer's EUR build values and need runtime confirmation.
            strong = False
        entry["verified_by"] = evidence
        if strong:
            entry["status"] = "signature_pinned"
        elif entry["kind"] == "patch_site" and "signature" in entry:
            entry["status"] = "image_pinned_only"
        else:
            entry["status"] = "needs_runtime_check"
        counts["verified" if strong else "unverified"] += 1
        entry["usa_ref"] = f"0x{entry['usa_ref']:08X}"
        entry["exec_addr"] = f"0x{address:08X}"
        entry["eur_offset"] = f"{entry['eur_offset']:+#x}"

    by_name = {entry["name"]: entry for entry in entries}
    for name, (site_name, block_offset, evidence) in CLIENT_HOOK_BLOCKS.items():
        site = by_name[site_name]
        site_address = int(site["exec_addr"], 16)
        start = site_address + block_offset
        previous = struct.unpack_from("<I", code, start - 4 - CODE_BASE)[0]
        if evidence == "block_after_call":
            ok = (previous & 0x0F000000) == 0x0B000000  # BL, any condition
        elif evidence == "block_after_branch":
            ok = is_branch(previous)
        else:
            branch_address = site_address + evidence[1]
            word = struct.unpack_from("<I", code, branch_address - CODE_BASE)[0]
            ok = branch_target(word, branch_address) == start
            evidence = "branch_target"
        if not ok:
            print(f"error: {name}: 0x{start:08X} lacks its evidence ({evidence})", file=sys.stderr)
            return 1
        window = code[start - CODE_BASE:start - CODE_BASE + SIGNATURE_BYTES]
        entries.append({
            "name": name,
            "kind": "hook_block",
            "usa_ref": site["usa_ref"],
            "eur_offset": site["eur_offset"],
            "exec_addr": f"0x{start:08X}",
            "ld_line": site["ld_line"],
            "section": "text",
            "signature": "sha256:" + hashlib.sha256(window).hexdigest(),
            "verified_by": ["oot3d_randomizer_ld_eur_offset", evidence],
            "status": "signature_pinned",
        })
        counts["verified"] += 1

    document = {
        "format": "triaevum_archipelago_addresses_v1",
        "code_sha256": code_sha256,
        "source": "OoT3D_Randomizer code/oot.ld",
        "source_commit": arguments.rando_commit,
        "signature_bytes": SIGNATURE_BYTES,
        "counts": counts,
        "entries": entries,
    }
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(document, indent=1) + "\n", encoding="utf-8", newline="\n")
    print(f"{len(entries)} entries: {counts}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
