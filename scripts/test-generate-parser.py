#!/usr/bin/env python3
"""Regression checks for identifier isolation before header-name rewriting."""
from pathlib import Path
import runpy
import unittest


TOKENS = runpy.run_path(str(Path(__file__).with_name("generate-parser.py")))["TOKENS"]


def isolate(source):
    mapping = {"cmark_ctype": "aimarkdown_cmark_ctype"}
    return TOKENS.sub(lambda match: mapping.get(match[0], match[0]), source)


class IncludeIsolationTests(unittest.TestCase):
    def test_angle_header_remains_available_to_header_name_rewriting(self):
        # table.c exposed this failure: identifier isolation ran inside <...>.
        source = "#include <cmark_ctype.h>\ncmark_ctype(value);\n"
        self.assertEqual(isolate(source), "#include <cmark_ctype.h>\naimarkdown_cmark_ctype(value);\n")

    def test_directive_spacing_and_trailing_comment_do_not_hide_code(self):
        source = "\t#  include\t<cmark_ctype.h> // cmark_ctype\ncmark_ctype;"
        self.assertEqual(isolate(source), "\t#  include\t<cmark_ctype.h> // cmark_ctype\naimarkdown_cmark_ctype;")

    def test_quoted_headers_runtime_strings_and_comments_remain_literal(self):
        source = '#include "cmark_ctype.h"\n"cmark_ctype"; /* cmark_ctype */ cmark_ctype;'
        self.assertEqual(isolate(source), '#include "cmark_ctype.h"\n"cmark_ctype"; /* cmark_ctype */ aimarkdown_cmark_ctype;')

    def test_angle_expression_is_not_an_include(self):
        self.assertEqual(isolate("x < cmark_ctype > y;"), "x < aimarkdown_cmark_ctype > y;")


if __name__ == "__main__":
    unittest.main()
