import argparse
from html import escape
from html.parser import HTMLParser
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]


class ResultsTable(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=False)
        self.output = []
        self.section = ""
        self.header_row = -1
        self.column = 0
        self.cell = None
        self.policy_count = 0

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag in ("thead", "tbody"):
            self.section = tag
        if tag == "table":
            attrs.update(id="testing-results", **{"class": "results-table"})
        if tag == "tr":
            self.column = 0
            if self.section == "thead":
                self.header_row += 1
            elif self.section == "tbody":
                self.policy_count += 1
        if tag in ("th", "td"):
            classes = []
            output_tag = tag
            if self.section == "tbody":
                if self.column == 0:
                    output_tag = "th"
                    attrs["scope"] = "row"
                    classes.append("policy-cell")
                if self.column in (1, 6, 11):
                    classes.append("result-cell")
                if self.column in (1, 6, 11, 16):
                    classes.append("group-start")
            elif self.section == "thead":
                attrs["scope"] = "colgroup" if "colspan" in attrs else "col"
                if self.header_row == 0 and self.column == 0:
                    classes.append("policy-cell")
                elif self.header_row == 0 or self.column in (0, 5, 10):
                    classes.append("group-start")
            if classes:
                attrs["class"] = " ".join(classes)
            self.cell = (tag, output_tag)
            tag = output_tag
        attributes = "".join(
            f' {key}="{escape(value, quote=True)}"' if value is not None else f" {key}"
            for key, value in attrs.items()
        )
        self.output.append(f"<{tag}{attributes}>")
        if tag == "table":
            self.output.append(
                '<caption class="sr-only">Policy testing results at depths 3, 4, and 5. '
                'Modeling and testing times are in seconds.</caption>'
            )

    def handle_endtag(self, tag):
        if tag in ("td", "th") and self.cell:
            tag = self.cell[1]
            self.cell = None
            self.column += 1
        self.output.append(f"</{tag}>")
        if tag == "tr" and self.section == "tbody" and self.column != 17:
            raise ValueError(f"Expected 17 columns in policy row {self.policy_count}")
        if tag in ("thead", "tbody"):
            self.section = ""

    def handle_data(self, data):
        status = {"✓": "pass", "✗": "fail", "—": "inconclusive"}.get(data)
        if self.section == "tbody" and self.column in (1, 6, 11) and status:
            self.output.append(f'<span class="result {status}">{data}</span>')
        else:
            self.output.append(data)

    def handle_entityref(self, name):
        self.output.append(f"&{name};")

    def handle_charref(self, name):
        self.output.append(f"&#{name};")


def build(source, output_dir):
    markdown = source.read_text(encoding="utf-8-sig")
    tables = list(re.finditer(r"<table\b[^>]*>.*?</table>", markdown, re.I | re.S))
    if len(tables) != 1:
        raise ValueError("The Markdown results must contain exactly one HTML table")
    table = ResultsTable()
    table.feed(tables[0].group())
    table.close()
    if not table.policy_count or table.header_row != 1:
        raise ValueError("Expected policy rows and a two-row table header")
    notes = markdown[:tables[0].start()]
    notes = re.sub(r"^# .*\n", "", notes, count=1).strip()
    notes_html = re.sub(r"\x60([^\x60]+)\x60", r"<code>\1</code>", escape(notes))
    page = (ROOT / "pyscript/templates/testing_results.html").read_text(encoding="utf-8")
    for marker, value in {
        "{{RESULTS_TABLE}}": "".join(table.output),
        "{{RESULTS_NOTES}}": notes_html,
        "{{POLICY_COUNT}}": str(table.policy_count),
    }.items():
        if page.count(marker) != 1:
            raise ValueError(f"Expected one template marker: {marker}")
        page = page.replace(marker, value)
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "testing_results.html").write_text(page, encoding="utf-8")
    (output_dir / "index.html").write_text(
        '<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
        '<meta http-equiv="refresh" content="0; url=./testing_results.html">\n'
        '<title>RVAT testing results</title>\n</head>\n<body>\n'
        '<p><a href="./testing_results.html">View RVAT testing results</a></p>\n'
        '</body>\n</html>\n', encoding="utf-8",
    )
    (output_dir / ".nojekyll").touch()
    print(f"Built {output_dir / 'testing_results.html'} ({table.policy_count} policies)")


def main():
    parser = argparse.ArgumentParser(description="Build a standalone results page from the Markdown table.")
    parser.add_argument("--source", type=Path, help="Markdown results (auto-detected by default)")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "docs")
    args = parser.parse_args()
    source = args.source
    if source is None:
        candidates = list((ROOT / "properties").glob("tab*testing_results.md"))
        if len(candidates) != 1:
            parser.error("Expected one properties/tab*testing_results.md; use --source")
        source = candidates[0]
    build(source, args.output_dir)


if __name__ == "__main__":
    main()
