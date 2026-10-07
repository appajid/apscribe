#!/usr/bin/env python3

"""Document hyphenation ignore entries must suppress automatic breaks.

For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
"""

import gzip
import os
import tempfile
from xml.etree import ElementTree

import scribus


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def document_tree(path):
    if path.endswith(".gz"):
        with gzip.open(path, "rb") as stream:
            return ElementTree.parse(stream)
    return ElementTree.parse(path)


def saved_text(tree, frame):
    story = tree.find(f".//PageObject[@AutoName='{frame}']/StoryText")
    check(story is not None, "missing saved story: " + frame)
    return "".join(node.get("Chars", "") for node in story.iter("Content"))


output_dir = os.environ.get("SCRIBUS_TEST_OUTPUT_DIR", tempfile.gettempdir())
os.makedirs(output_dir, exist_ok=True)
baseline_path = os.path.join(output_dir, "hyphenation-baseline.sla")
fixture_path = os.path.join(output_dir, "hyphenation-ignore-fixture.sla")
result_path = os.path.join(output_dir, "hyphenation-ignored.sla")
word = "representation"
control_word = "typography"
selected_text = word + " " + control_word + " " + word
manual_text = "repre\u00adsentation"

check(scribus.newDocument(
    scribus.PAPER_A4, (20, 20, 20, 20), scribus.PORTRAIT, 1,
    scribus.UNIT_POINTS, scribus.PAGE_1, 0, 1,
), "could not create document")
scribus.createCharStyle(name="English UK", language="en_GB")
scribus.createParagraphStyle(name="English paragraph", charstyle="English UK")
for index, (name, text) in enumerate((
    ("Ignored", word), ("Control", control_word),
    ("Selected", selected_text), ("Manual", manual_text),
    ("Long minimum", word),
)):
    frame = scribus.createText(50, 50 + index * 100, 300, 80, name)
    scribus.setText(text, frame)
    scribus.setParagraphStyle("English paragraph", frame)
    scribus.hyphenateText(frame)
scribus.saveDocAs(baseline_path)
scribus.closeDoc()

tree = document_tree(baseline_path)
check("\u00ad" in saved_text(tree, "Ignored"), "English dictionary did not hyphenate the fixture word")
check("\u00ad" in saved_text(tree, "Control"), "English dictionary did not hyphenate the control word")
hyphenator = tree.find(".//Document/Hyphenator")
check(hyphenator is not None, "missing document hyphenation settings")
ElementTree.SubElement(hyphenator, "Ignore", Word=word)
for content in tree.findall(".//PageObject[@AutoName='Long minimum']/StoryText/Content"):
    content.set("HyphenWordMinimum", "50")
tree.write(fixture_path, encoding="utf-8", xml_declaration=True)
check(scribus.openDoc(fixture_path), "could not open document with ignore entry")

# Adding an ignore entry must clear existing automatic breaks when the user
# rehyphenates. Check that selection offsets leave the unselected text alone.
selection_start = len(word) + 1 + len(control_word) + 1
scribus.selectText(selection_start, len(word), "Selected")
scribus.hyphenateText("Selected")
scribus.selectText(0, 0, "Selected")
scribus.hyphenateText("Ignored")
scribus.hyphenateText("Manual")
scribus.hyphenateText("Long minimum")
scribus.saveDocAs(result_path)
result = document_tree(result_path)
check(saved_text(result, "Ignored") == word, "ignored word retained automatic hyphenation")
check(saved_text(result, "Long minimum") == word, "minimum word length prevented clearing an ignored word")
parts = saved_text(result, "Selected").split(" ")
check("\u00ad" in parts[0], "hyphenation changed outside the selected range")
check("\u00ad" in parts[1], "hyphenation changed the unselected control word")
check(parts[2] == word, "selected ignored word retained automatic hyphenation")
check(saved_text(result, "Manual") == manual_text.replace("\u00ad", "\u00ad\u00ad"), "explicit soft hyphen was lost")

# Repeat the reported dehyphenate/rehyphenate sequence and reopen compressed
# SLA to verify that ignore entries and their behavior persist.
for _ in range(2):
    scribus.dehyphenateText("Ignored")
    scribus.hyphenateText("Ignored")
scribus.hyphenateText("Selected")
compressed_path = os.path.join(output_dir, "hyphenation-ignored.sla.gz")
scribus.saveDocAs(compressed_path)
scribus.closeDoc()
check(scribus.openDoc(compressed_path), "could not reopen compressed document")
scribus.hyphenateText("Ignored")
scribus.hyphenateText("Control")
scribus.saveDocAs(result_path)
result = document_tree(result_path)
check(result.find(f".//Hyphenator/Ignore[@Word='{word}']") is not None, "ignore entry was not saved")
check(saved_text(result, "Ignored") == word, "ignored word was hyphenated after reopen")
check("\u00ad" in saved_text(result, "Control"), "ignore entry suppressed an unrelated word")
check(saved_text(result, "Selected").split(" ")[::2] == [word, word], "whole-frame rehyphenation did not honor ignore entry")
scribus.closeDoc()
print("HYPHENATION_IGNORE_LIST_PASSED", flush=True)
