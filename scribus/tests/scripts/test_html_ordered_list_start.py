#!/usr/bin/env python3

"""Regression for HTML ordered-list start values in the Scripter importer.

For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
"""

import os
import tempfile

import scribus


if not scribus.newDocument(
    scribus.PAPER_A4,
    (20, 20, 20, 20),
    scribus.PORTRAIT,
    1,
    scribus.UNIT_POINTS,
    scribus.PAGE_1,
    0,
    1,
):
    raise AssertionError("could not create document")

frame = scribus.createText(50, 50, 300, 300, "HTML List")
output_dir = os.environ.get("SCRIBUS_TEST_OUTPUT_DIR", tempfile.gettempdir())
os.makedirs(output_dir, exist_ok=True)
html_path = os.path.join(output_dir, "ordered-list-start.html")
with open(html_path, "w", encoding="utf-8") as html_file:
    html_file.write(
        "<html><body><ol start=\"4\"><li>Alpha</li><li>Beta</li></ol>"
        "<ol><li>Gamma</li></ol></body></html>"
    )

scribus.insertHtmlText(html_path, frame)
text = scribus.getAllText(frame)
for expected in ("4. Alpha", "5. Beta", "1. Gamma"):
    if expected not in text:
        raise AssertionError(f"missing {expected!r} in imported text: {text!r}")

scribus.closeDoc()
print("HTML_ORDERED_LIST_START_PASSED", flush=True)
