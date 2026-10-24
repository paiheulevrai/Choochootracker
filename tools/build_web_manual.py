"""Build the readable web manual from docs/USER_MANUAL.md."""
from html import escape
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / ".tmp" / "manual-web-build"))
import markdown


def slug(text):
    return re.sub(r"[^a-z0-9]+", "-", re.sub(r"<.*?>", "", text).lower()).strip("-")


source = (ROOT / "docs" / "USER_MANUAL.md").read_text(encoding="utf-8")
body = markdown.markdown(source, extensions=["tables", "fenced_code", "sane_lists"])


def anchor(match):
    level, title = match.groups()
    return f'<h{level} id="{escape(slug(title), quote=True)}">{title}</h{level}>'


body = re.sub(r"<h([234])>(.*?)</h\1>", anchor, body)
headings = re.findall(r'<h([23]) id="([^"]+)">(.*?)</h\1>', body)
toc_parts = []
for level, ident, title in headings:
    label = re.sub(r"<.*?>", "", title)
    if level == "2":
        if toc_parts:
            toc_parts.append("</details>")
        toc_parts.append(f'<details><summary><a href="#{ident}">{label}</a></summary>')
    else:
        toc_parts.append(f'<a class="level-{level}" href="#{ident}">{label}</a>')
if toc_parts:
    toc_parts.append("</details>")
toc = "".join(toc_parts)
assert "mme" in toc and "<h1>" in body

style = """
:root{color-scheme:dark;font:16px/1.55 system-ui,sans-serif;background:#07101f;color:#d7dce8}
*{box-sizing:border-box}body{margin:0}a{color:#83d9ff}header{border-bottom:1px solid #294560;background:#0b1930;padding:1.2rem max(1.25rem,calc((100vw - 1160px)/2));position:sticky;top:0;z-index:2}header a{color:#ffd665;text-decoration:none;font-weight:700}.menu-toggle{display:none}.layout{max-width:1160px;margin:auto;display:grid;grid-template-columns:13rem minmax(0,1fr);gap:3rem;padding:2rem 1.25rem 5rem}nav{position:sticky;top:5rem;align-self:start;max-height:calc(100vh - 6rem);overflow:auto}nav details{margin-bottom:.25rem}nav summary{cursor:pointer;color:#ffd665;font-size:.85rem}nav summary a{color:inherit;text-decoration:none}nav a{display:block;padding:.25rem 0;text-decoration:none;font-size:.85rem}.level-3{margin-left:1rem}main{max-width:52rem;min-width:0}h1{font-size:clamp(2rem,5vw,3.5rem);line-height:1.1;color:#ffd665;margin:0 0 2rem}h2{font-size:clamp(1.6rem,3vw,2.2rem);line-height:1.2;color:#ffd665;border-bottom:1px solid #294560;margin:4rem 0 1.25rem;padding-bottom:.45rem;scroll-margin-top:5rem}h3{font-size:1.35rem;color:#83d9ff;margin:2.5rem 0 .75rem;scroll-margin-top:5rem}h4{font-size:1.1rem;color:#ffb199;margin:2rem 0 .5rem;scroll-margin-top:5rem}p,li{max-width:75ch}table{border-collapse:collapse;display:block;max-width:100%;overflow:auto;margin:1rem 0}th,td{border:1px solid #294560;padding:.45rem .6rem;text-align:left;vertical-align:top}th{background:#10284a;color:#ffd665}code{background:#10284a;color:#b9efff;padding:.08rem .25rem}pre{background:#030814;padding:1rem;overflow:auto}blockquote{border-left:3px solid #83d9ff;margin:1rem 0;padding-left:1rem;color:#b6c5d8}@media(max-width:760px){header{position:sticky;display:flex;align-items:center;justify-content:space-between;padding:.8rem 1rem}.menu-toggle{display:block;border:1px solid #52799b;border-radius:.25rem;background:#10284a;color:#d7dce8;padding:.35rem .55rem;font:inherit;font-size:.85rem}.layout{display:block;padding:1rem}.layout nav{display:none}.menu-open .layout nav{display:block;position:fixed;z-index:3;top:3.7rem;bottom:0;left:0;width:min(20rem,88vw);max-height:none;margin:0;padding:1rem;background:#0b1930;border-right:1px solid #294560;box-shadow:1rem 0 2rem #0008;columns:1}.menu-open .layout nav a{padding:.45rem 0}h2{margin-top:3rem}}
"""

script = """const button=document.querySelector('.menu-toggle'),menu=document.querySelector('#toc');button.onclick=()=>{const open=document.body.classList.toggle('menu-open');button.setAttribute('aria-expanded',open)};menu.querySelectorAll('a').forEach(link=>link.onclick=()=>{document.body.classList.remove('menu-open');button.setAttribute('aria-expanded','false')});"""
html = f"""<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>ChooChooTracker User Manual</title><style>{style}</style></head><body><header><a href=\"#top\">ChooChooTracker — User Manual</a><button class=\"menu-toggle\" aria-controls=\"toc\" aria-expanded=\"false\">&#9776; Contents</button></header><div class=\"layout\"><nav id=\"toc\" aria-label=\"Chapters\">{toc}</nav><main id=\"top\">{body}</main></div><script>{script}</script></body></html>"""
(ROOT / "web" / "dist" / "manual.html").write_text(html, encoding="utf-8")
