"""Regenerate the translated manuals from docs/USER_MANUAL.md.

Requires OPENAI_API_KEY.  The API is called only when this script is run.
"""
import json
import hashlib
import os
import re
import sys
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs" / "USER_MANUAL.md"
TARGETS = {
    "de": "German", "es": "Spanish", "fr": "French", "it": "Italian",
    "ja": "Japanese", "nl": "Dutch", "pl": "Polish", "pt": "Brazilian Portuguese",
    "ru": "Russian", "zh": "Simplified Chinese",
}
MODEL = os.getenv("OPENAI_MANUAL_TRANSLATION_MODEL", "gpt-4.1-mini")
LIMIT = 12000
REQUEST_TIMEOUT = 60
CHECKPOINTS = ROOT / ".tmp" / "manual-translation" / "checkpoints"


def chunks(text, limit=LIMIT):
    parts = re.split(r"(\n\n+)", text)
    result, current = [], ""
    for part in parts:
        if current and len(current) + len(part) > limit:
            result.append(current)
            current = ""
        current += part
    if current:
        result.append(current)
    return result


def translate(language, text):
    prompt = f"""Translate this Markdown user manual from English to {language}.
Return only the translation. Preserve the Markdown structure exactly: headings,
tables, lists, links, code fences and inline code. Do not translate text inside
backticks, FX codes, file extensions, engine names, or application UI names such
as Song, Chain, Phrase, Instrument, Table, Mixer, Settings, EDIT, OPT, SELECT,
PLAY, Bank and Preset. Keep TODO markers exactly as written. Write concise,
natural documentation for musicians.\n\n{text}"""
    data = json.dumps({"model": MODEL, "messages": [{"role": "user", "content": prompt}]}).encode()
    request = urllib.request.Request(
        "https://api.openai.com/v1/chat/completions", data=data,
        headers={"Authorization": "Bearer " + os.environ["OPENAI_API_KEY"], "Content-Type": "application/json"},
    )
    with urllib.request.urlopen(request, timeout=REQUEST_TIMEOUT) as response:
        return json.load(response)["choices"][0]["message"]["content"]


def translate_part(language, text):
    return "".join(translate(language, request_part) for request_part in chunks(text, LIMIT // 2))


def check(source, output):
    if source.count("```") != output.count("```"):
        raise ValueError("Markdown code fences changed")
    if source.count("#TODO") != output.count("#TODO"):
        raise ValueError("TODO marker count changed")


def load_checkpoint(code, fingerprint):
    path = CHECKPOINTS / f"{code}.json"
    if not path.exists():
        return []
    data = json.loads(path.read_text(encoding="utf-8"))
    return data["parts"] if data.get("fingerprint") == fingerprint else []


def save_checkpoint(code, fingerprint, translated):
    CHECKPOINTS.mkdir(parents=True, exist_ok=True)
    path = CHECKPOINTS / f"{code}.json"
    temporary = path.with_suffix(".tmp")
    temporary.write_text(
        json.dumps({"fingerprint": fingerprint, "parts": translated}, ensure_ascii=False),
        encoding="utf-8",
    )
    temporary.replace(path)


def main():
    if not os.getenv("OPENAI_API_KEY"):
        raise SystemExit("OPENAI_API_KEY is required")
    english = SOURCE.read_text(encoding="utf-8")
    preamble = (
        "\n> This is an automatically generated translation of `docs/USER_MANUAL.md` "
        "from the current English manual. If it differs from English, the English text prevails.\n"
        ">\n> **Conventions.** The application interface is in English. Keep screen, menu, "
        "field and button names, FX codes, module and engine names, and file extensions in English.\n\n"
    )
    source = english.replace("\n", preamble, 1)
    wanted = sys.argv[1:] or TARGETS
    for code in wanted:
        if code not in TARGETS:
            raise SystemExit(f"Unknown language: {code}")
        language = TARGETS[code]
        print(f"{code}: translating", flush=True)
        parts = chunks(source)
        fingerprint = hashlib.sha256(source.encode()).hexdigest()
        output = load_checkpoint(code, fingerprint)
        if len(output) > len(parts):
            output = []
        for index, part in enumerate(parts[len(output):], len(output) + 1):
            print(f"{code}: {index}/{len(parts)}", flush=True)
            output.append(translate_part(language, part))
            save_checkpoint(code, fingerprint, output)
        translated = "".join(output)
        check(source, translated)
        (ROOT / "docs" / "translations" / f"USER_MANUAL.{code}.md").write_text(translated, encoding="utf-8-sig")
        (CHECKPOINTS / f"{code}.json").unlink(missing_ok=True)
        print(f"{code}: done", flush=True)


if __name__ == "__main__":
    main()
