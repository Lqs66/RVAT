"""Build the Markdown documentation into the existing docs/ hosting directory."""

import argparse
from dataclasses import dataclass
from hashlib import sha256
from html import escape
from pathlib import Path
import posixpath
import re
import shutil
from urllib.parse import quote, unquote, urlsplit, urlunsplit

from bs4 import BeautifulSoup
from markdown_it import MarkdownIt
from mdit_py_plugins.dollarmath import dollarmath_plugin


ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs"
REPOSITORY = "https://github.com/Lqs66/RVAT"
POLICY = re.compile(r"^(?:AP_|PX_|A\.|PX\.)[A-Za-z0-9_.]+")


@dataclass(frozen=True)
class Page:
    source: str
    route: str
    title: str
    section: str
    description: str
    wide: bool = False


PAGES = [
    Page("properties/properties.md", "properties/index.html", "Properties", "Properties",
         "Browse autopilot policies, formulas, and comparative testing results."),
    Page("properties/tab:ardupilot_properties.md", "properties/ardupilot.html", "ArduPilot Properties", "Properties",
         "ArduPilot policies and UPPAAL formulas, with RVAT, PGFuzz, and RouthSearch results.", True),
    Page("properties/tab:px4_properties.md", "properties/px4.html", "PX4 Properties", "Properties",
         "PX4 policies and UPPAAL formulas, with RVAT, PGFuzz, and RouthSearch results.", True),
    Page("example/running_example.md", "example/running_example.html", "Running Example", "Running Example",
         "Follow PX_RCFS_P1 from a natural-language policy to a timed control flow automaton."),
    Page("tcfa-data-valuation.md", "tcfa/data-valuation.html", "TCFA Data Valuation", "Data Valuation",
         "Operational semantics and data valuation update rules for LLVM IR."),
    Page("properties/tab:testing_results.md", "results/testing_results.html", "Testing Results", "Results",
         "Model sizes and testing outcomes at depths 3, 4, and 5.", True),
    Page("properties/tab:violation_report.md", "results/violations.html", "Violation Reports", "Results",
         "Reported policy violations and their recorded upstream issue and pull request status.", True),
    Page("prompts/prompts.md", "prompts/index.html", "Prompts", "Prompts",
         "TCTL–UPPAAL correspondence and the prompt templates used by RVAT."),
    Page("prompts/tab:prompt_conversion.md", "prompts/extraction.html", "Property Extraction", "Prompts",
         "Convert natural-language policies into UPPAAL-TCTL formulas."),
    Page("prompts/tab:prompt_rag.md", "prompts/rag.html", "RAG Slicing Criteria", "Prompts",
         "Map property formulas to source code variables and slicing criteria."),
    Page("prompts/tab:prompt_refine.md", "prompts/correction.html", "Formula Correction", "Prompts",
         "Check and correct property formulas against their behavioral policies."),
]
BY_SOURCE = {page.source: page for page in PAGES}
SECTIONS = {
    "Home": "index.html",
    "Prompts": "prompts/index.html",
    "Properties": "properties/index.html",
    "Running Example": "example/running_example.html",
    "Data Valuation": "tcfa/data-valuation.html",
    "Results": "results/testing_results.html",
}


def relative(target, route):
    return posixpath.relpath(target, posixpath.dirname(route) or ".")


def fill(name, **values):
    template = (DOCS / "templates" / name).read_text(encoding="utf-8")
    for key, value in values.items():
        template = template.replace("{{" + key + "}}", str(value))
    return template


def math_html(tex, display=False):
    tag = "div" if display else "span"
    return (f'<{tag} data-math="{escape(tex, quote=True)}" '
            f'data-display="{str(display).lower()}">{escape(tex)}</{tag}>')


def round_math(state, silent):
    """Keep parenthesized TeX intact without changing Markdown code or links."""
    if not state.src.startswith(r"\(", state.pos):
        return False
    end = state.src.find(r"\)", state.pos + 2)
    if end < 0:
        return False
    if not silent:
        token = state.push("math_inline", "math", 0)
        token.content = state.src[state.pos + 2:end]
    state.pos = end + 2
    return True


def render_math(self, tokens, index, options, env):
    return math_html(tokens[index].content, tokens[index].type.startswith("math_block"))


def markdown_renderer():
    renderer = MarkdownIt("commonmark", {"html": True, "linkify": True})
    renderer.enable(["table", "strikethrough", "linkify"])
    renderer.use(dollarmath_plugin, allow_labels=False)
    renderer.inline.ruler.before("escape", "round_math", round_math)
    for token in ("math_inline", "math_block"):
        renderer.add_render_rule(token, render_math)
    return renderer


def headings(soup):
    used = {node["id"] for node in soup.select("[id]")}
    for heading in soup.find_all(re.compile(r"^h[1-6]$")):
        if heading.get("id"):
            continue
        slug = re.sub(r"[^\w\- ]", "", heading.get_text().lower())
        slug = re.sub(r"\s", "-", slug.strip()) or "section"
        candidate, suffix = slug, 0
        while candidate in used:
            suffix += 1
            candidate = f"{slug}-{suffix}"
        heading["id"] = candidate
        used.add(candidate)


def rewrite_links(soup, page, output_dir):
    for node in soup.select("a[href], img[src]"):
        attribute = "src" if node.name == "img" else "href"
        original = str(node[attribute])
        url = urlsplit(original)
        if url.scheme or url.netloc:
            continue
        if not url.path:
            continue
        source = posixpath.normpath(posixpath.join(posixpath.dirname(page.source), unquote(url.path)))
        if source in BY_SOURCE:
            target = relative(BY_SOURCE[source].route, page.route)
        elif node.name == "img" and (ROOT / source).is_file():
            asset = "assets/" + source
            destination = output_dir / asset
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / source, destination)
            target = relative(asset, page.route)
        else:
            target = f"{REPOSITORY}/blob/master/{quote(source, safe='/')}"
        node[attribute] = urlunsplit(("", "", target, url.query, url.fragment))
    for image in soup.find_all("img"):
        image["loading"] = "lazy"


def prepare_tables(soup, page):
    for index, table in enumerate(soup.find_all("table")):
        table["class"] = [*table.get("class", []), "data-table"]
        label = f"{page.title} table {index + 1}"
        wrapper = soup.new_tag("div", attrs={
            "class": "table-scroll", "tabindex": "0", "role": "region", "aria-label": label,
        })
        table.wrap(wrapper)
        for cell in table.select("thead th"):
            cell["scope"] = "colgroup" if cell.has_attr("colspan") else "col"
        for row in table.select("tbody tr"):
            for cell in row.find_all(["td", "th"], recursive=False):
                policy = POLICY.match(cell.get_text(" ", strip=True))
                if policy:
                    row["id"] = policy.group()
                    cell["class"] = [*cell.get("class", []), "policy-id"]
                    break


def prepare_results(soup):
    table = soup.find("table")
    if table is None or len(soup.find_all("table")) != 1:
        raise ValueError("Testing results must contain exactly one table")
    table["id"] = "testing-results"
    table["class"].append("results-table")
    rows = table.select("tbody tr")
    if not rows or len(table.select("thead tr")) != 2:
        raise ValueError("Expected policy rows and a two-row testing results header")
    for row in rows:
        cells = row.find_all(["td", "th"], recursive=False)
        if len(cells) != 17:
            raise ValueError(f"Expected 17 columns in results row: {row.get('id')}")
        cells[0].name = "th"
        cells[0]["scope"] = "row"
        cells[0]["class"] = ["policy-cell"]
        for index in (1, 6, 11, 16):
            cells[index]["class"] = ["group-start"]
        for index in (1, 6, 11):
            cell = cells[index]
            cell["class"].append("result-cell")
            value = cell.get_text(strip=True)
            status = {"✓": "pass", "✗": "fail", "—": "inconclusive"}.get(value)
            if status:
                badge = soup.new_tag("span", attrs={"class": f"result {status}"})
                badge.string = value
                cell.clear()
                cell.append(badge)
    header = table.select_one("thead tr th")
    header["class"] = ["policy-cell"]
    return len(rows)


def navigation(route, section):
    links = []
    for name, target in SECTIONS.items():
        active = ' aria-current="page"' if name == section else ""
        links.append(f'<a href="{relative(target, route)}"{active}>{name}</a>')
    links.append(f'<a href="{REPOSITORY}">GitHub ↗</a>')
    return "".join(links)


def sidebar(route, section):
    links = []
    for page in PAGES:
        if page.section != section:
            continue
        active = ' aria-current="page"' if page.route == route else ""
        links.append(f'<li><a href="{relative(page.route, route)}"{active}>{page.title}</a></li>')
    return (f'<aside class="sidebar"><nav class="sidebar-group" aria-label="{section} pages">'
            f'<h2>{section}</h2><ul>{"".join(links)}</ul></nav></aside>')


def document_main(page, soup):
    toc = "".join(f'<li class="toc-level-{heading.name[1]}"><a href="#{heading["id"]}">'
                  f'{escape(heading.get_text(" ", strip=True))}</a></li>'
                  for heading in soup.find_all(["h2", "h3"]))
    return (f'<div class="site-shell">{sidebar(page.route, page.section)}'
            '<main id="main-content" class="main-content">'
            f'<header class="page-header"><p class="eyebrow">{page.section}</p><h1>{page.title}</h1>'
            f'<p class="page-description">{escape(page.description)}</p></header>'
            f'<article id="article" class="document-content">{soup}</article></main>'
            f'<aside class="toc"><nav aria-label="On this page"><h2>On this page</h2><ul>{toc}</ul>'
            '</nav></aside></div>')


def write_page(output_dir, route, title, section, description, main, wide=False):
    root = relative("index.html", route).removesuffix("index.html")
    css_version = sha256((DOCS / "assets/site.css").read_bytes()).hexdigest()[:10]
    body_class = "home-page" if route == "index.html" else "document-page"
    math_assets = ""
    if "data-math=" in main:
        math_assets = (
            '<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/katex@0.19.1/dist/katex.min.css" '
            'integrity="sha384-WwGtaC6ywCHxA5cstpj/3+xIZvTez8dq2IcBQaXRkenHZ7FVVq1ZROtURwfbtTlG" crossorigin="anonymous">'
            '<script defer src="https://cdn.jsdelivr.net/npm/katex@0.19.1/dist/katex.min.js" '
            'integrity="sha384-a1WQdjRj162i159OYq041X6I4K8NenkrI09LlPmiHg0YOb7CR+wJOraXwK9abpnE" crossorigin="anonymous"></script>'
        )
    html = fill("page.html", TITLE=escape(title), DESCRIPTION=escape(description, quote=True), ROOT=root,
                CSS_VERSION=css_version,
                BODY_CLASS=body_class + (" wide-page" if wide else ""), MATH_ASSETS=math_assets,
                NAVIGATION=navigation(route, section), MAIN=main)
    target = output_dir / route
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(html, encoding="utf-8")


def build(output_dir):
    output_dir = output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    renderer = markdown_renderer()
    for page in PAGES:
        content = (ROOT / page.source).read_text(encoding="utf-8-sig")
        soup = BeautifulSoup(renderer.render(content), "html.parser")
        headings(soup)
        first_heading = soup.find("h1")
        if first_heading:
            first_heading.decompose()
        rewrite_links(soup, page, output_dir)
        prepare_tables(soup, page)
        if page.route == "results/testing_results.html":
            count = prepare_results(soup)
            table = soup.select_one(".table-scroll").extract()
            notes = soup.get_text(" ", strip=True)
            soup = BeautifulSoup(fill("testing_results.html", RESULTS_NOTES=escape(notes),
                                      RESULTS_TABLE=str(table), POLICY_COUNT=count), "html.parser")
        write_page(output_dir, page.route, page.title, page.section, page.description,
                   document_main(page, soup), page.wide)
    write_page(output_dir, "index.html", "Home", "Home",
               "RVAT detects logic vulnerabilities in robotic vehicles through timed-automata-based simulation.",
               '<main id="main-content" class="main-content">' + fill("home.html") + '</main>')
    for filename in ("site.css", "site.js", "favicon.svg"):
        destination = output_dir / "assets" / filename
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.resolve() != (DOCS / "assets" / filename).resolve():
            shutil.copyfile(DOCS / "assets" / filename, destination)
    (output_dir / ".nojekyll").touch()
    print(f"Built {len(PAGES) + 1} pages in {output_dir}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DOCS,
                        help="Static site destination (default: docs/)")
    build(parser.parse_args().output_dir)


if __name__ == "__main__":
    main()
