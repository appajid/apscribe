#!/usr/bin/env python3

"""
Exercise current-format SLA persistence of paragraph GREP style rules.

For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
"""

import gzip
import json
import os
import tempfile
import xml.etree.ElementTree as ET
from xml.sax.saxutils import quoteattr

import scribus


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def read_sla(path):
    with open(path, "rb") as saved_file:
        data = saved_file.read()
    return (gzip.decompress(data), True) if data.startswith(b"\x1f\x8b") else (data, False)


output_dir = os.environ.get("SCRIBUS_TEST_OUTPUT_DIR", tempfile.gettempdir())
os.makedirs(output_dir, exist_ok=True)
source_path = os.path.join(output_dir, "grep_style_source.sla")
roundtrip_path = os.path.join(output_dir, "grep_style_roundtrip.sla")
for path in (source_path, roundtrip_path):
    if os.path.exists(path):
        os.remove(path)

check(
    scribus.newDocument(
        scribus.PAPER_A4,
        (36, 36, 36, 36),
        scribus.PORTRAIT,
        1,
        scribus.UNIT_POINTS,
        scribus.PAGE_1,
        0,
        1,
    ),
    "could not create GREP style test document",
)
scribus.createCharStyle("GREP Accent")
scribus.createParagraphStyle("GREP Paragraph")
scribus.createParagraphStyle("GREP Clear")
scribus.saveDocAs(source_path)
scribus.closeDoc()

source_data, compressed = read_sla(source_path)
rules = [
    {
        "pattern": r"chapter\s+\d+",
        "characterStyle": "GREP Accent",
        "ignoreCase": True,
    }
]
rule_json = json.dumps(rules, ensure_ascii=False, separators=(",", ":"))
rule_attribute = (" GrepStyleRules=" + quoteattr(rule_json)).encode("utf-8")
for style_name, attribute in (
    ("GREP Paragraph", rule_attribute),
    ("GREP Clear", b' Parent="GREP Paragraph" GrepStyleRules=""'),
):
    marker = ('<ParagraphStyle Name="%s"' % style_name).encode("utf-8")
    check(source_data.count(marker) == 1, "missing paragraph style %s" % style_name)
    source_data = source_data.replace(marker, marker + attribute, 1)
with open(source_path, "wb") as source_file:
    source_file.write(gzip.compress(source_data) if compressed else source_data)

check(scribus.openDoc(source_path), "could not open GREP style fixture")
scribus.saveDocAs(roundtrip_path)
scribus.closeDoc()

roundtrip_data, _ = read_sla(roundtrip_path)
root = ET.fromstring(roundtrip_data)
styles = {element.get("Name"): element for element in root.findall(".//ParagraphStyle")}
check("GREP Paragraph" in styles, "GREP paragraph style was lost")
check("GREP Clear" in styles, "explicit empty rule override was lost")
check(json.loads(styles["GREP Paragraph"].get("GrepStyleRules", "null")) == rules,
      "GREP rule did not survive SLA round-trip")
check(styles["GREP Clear"].get("Parent") == "GREP Paragraph",
      "GREP style inheritance was lost")
check(styles["GREP Clear"].get("GrepStyleRules") == "",
      "explicit empty rule override did not survive SLA round-trip")
print("GREP_STYLE_PERSISTENCE_PASSED", flush=True)
