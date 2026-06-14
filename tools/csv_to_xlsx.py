#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import re
import zipfile
from pathlib import Path
from xml.sax.saxutils import escape


NUMERIC_RE = re.compile(r"^-?(?:\d+|\d+\.\d+|\.\d+)(?:[eE][+-]?\d+)?$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert CSV files to simple single-sheet XLSX files without external dependencies."
    )
    parser.add_argument("--input", help="Single CSV file to convert.")
    parser.add_argument("--output", help="Target XLSX path for --input mode.")
    parser.add_argument("--tree", help="Directory tree where every CSV should be converted next to itself.")
    return parser.parse_args()


def column_name(index: int) -> str:
    name = []
    value = index
    while value > 0:
        value, rem = divmod(value - 1, 26)
        name.append(chr(ord("A") + rem))
    return "".join(reversed(name))


def is_numeric_cell(value: str) -> bool:
    stripped = value.strip()
    if not stripped:
        return False
    if stripped.startswith(("0x", "+0x", "-0x")):
        return False
    if "/" in stripped or "^" in stripped:
        return False
    if stripped.startswith("0") and len(stripped) > 1 and not stripped.startswith(("0.", "0e", "0E")):
        return False
    if stripped.startswith("-0") and len(stripped) > 2 and not stripped.startswith(("-0.", "-0e", "-0E")):
        return False
    return bool(NUMERIC_RE.fullmatch(stripped))


def write_static_parts(zf: zipfile.ZipFile) -> None:
    zf.writestr(
        "[Content_Types].xml",
        """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>
  <Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>
  <Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>
  <Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>
  <Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>
</Types>
""",
    )
    zf.writestr(
        "_rels/.rels",
        """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>
  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" Target="docProps/core.xml"/>
  <Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties" Target="docProps/app.xml"/>
</Relationships>
""",
    )
    zf.writestr(
        "docProps/core.xml",
        """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:dcterms="http://purl.org/dc/terms/"
    xmlns:dcmitype="http://purl.org/dc/dcmitype/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <dc:title>CSV export</dc:title>
  <dc:creator>Codex</dc:creator>
</cp:coreProperties>
""",
    )
    zf.writestr(
        "docProps/app.xml",
        """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties"
    xmlns:vt="http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes">
  <Application>Codex</Application>
</Properties>
""",
    )
    zf.writestr(
        "xl/workbook.xml",
        """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
    xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
  <sheets>
    <sheet name="Sheet1" sheetId="1" r:id="rId1"/>
  </sheets>
</workbook>
""",
    )
    zf.writestr(
        "xl/_rels/workbook.xml.rels",
        """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>
</Relationships>
""",
    )
    zf.writestr(
        "xl/styles.xml",
        """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
  <fonts count="1">
    <font>
      <sz val="11"/>
      <name val="Calibri"/>
    </font>
  </fonts>
  <fills count="2">
    <fill><patternFill patternType="none"/></fill>
    <fill><patternFill patternType="gray125"/></fill>
  </fills>
  <borders count="1">
    <border><left/><right/><top/><bottom/><diagonal/></border>
  </borders>
  <cellStyleXfs count="1">
    <xf numFmtId="0" fontId="0" fillId="0" borderId="0"/>
  </cellStyleXfs>
  <cellXfs count="1">
    <xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>
  </cellXfs>
  <cellStyles count="1">
    <cellStyle name="Normal" xfId="0" builtinId="0"/>
  </cellStyles>
</styleSheet>
""",
    )


def write_worksheet(zf: zipfile.ZipFile, csv_path: Path) -> None:
    with zf.open("xl/worksheets/sheet1.xml", "w") as raw:
        raw.write(
            b'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            b'<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">\n'
            b'  <sheetData>\n'
        )

        with csv_path.open("r", encoding="utf-8", newline="") as f:
            reader = csv.reader(f)
            for row_index, row in enumerate(reader, start=1):
                raw.write(f'    <row r="{row_index}">'.encode("utf-8"))
                for col_index, value in enumerate(row, start=1):
                    if value == "":
                        continue
                    cell_ref = f"{column_name(col_index)}{row_index}"
                    if is_numeric_cell(value):
                        raw.write(
                            f'<c r="{cell_ref}"><v>{escape(value.strip())}</v></c>'.encode("utf-8")
                        )
                    else:
                        text = escape(value)
                        raw.write(
                            f'<c r="{cell_ref}" t="inlineStr"><is><t xml:space="preserve">{text}</t></is></c>'.encode(
                                "utf-8"
                            )
                        )
                raw.write(b"</row>\n")

        raw.write(b"  </sheetData>\n</worksheet>\n")


def convert_csv_to_xlsx(csv_path: Path, xlsx_path: Path) -> None:
    xlsx_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(xlsx_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        write_static_parts(zf)
        write_worksheet(zf, csv_path)


def convert_tree(root: Path) -> int:
    converted = 0
    for csv_path in sorted(root.rglob("*.csv")):
        xlsx_path = csv_path.with_suffix(".xlsx")
        convert_csv_to_xlsx(csv_path, xlsx_path)
        converted += 1
    return converted


def main() -> int:
    args = parse_args()

    if bool(args.input) == bool(args.tree):
        raise SystemExit("Use either --input/--output or --tree")

    if args.tree:
        count = convert_tree(Path(args.tree))
        print(f"converted_csv_files={count}")
        return 0

    if not args.output:
        raise SystemExit("--output is required with --input")

    convert_csv_to_xlsx(Path(args.input), Path(args.output))
    print(f"converted={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
