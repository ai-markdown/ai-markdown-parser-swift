import AIMarkdownParser
import AIMarkdownCMark
import XCTest

final class ParserBoundaryTests: XCTestCase {
    private func descendants(_ root: any Markup) -> [any Markup] {
        var result: [any Markup] = [], stack: [any Markup] = [root]
        while let node = stack.popLast() {
            result.append(node)
            stack.append(contentsOf: node.children.reversed())
        }
        return result
    }

    func testPublicFootnoteLabelMetadataAcrossPackageBoundary() throws {
        let label = try XCTUnwrap(FootnoteLabelMetadata(rawSource: "A&amp;B"))
        XCTAssertEqual(label.identifier, "a&amp;b")
        XCTAssertEqual(label.label, "A&B")
        XCTAssertNil(FootnoteLabelMetadata(rawSource: "a\0b"))
    }

    func testSoftBreakPhysicalEndingsAndLegacyOutput() throws {
        for eol in ["\n", "\r", "\r\n"] {
            let input = "> [x]: /u\(eol)> a\(eol)> b"
            let original = Document(parsing: input)
            let captured = Document(parsing: input, options: [.parseSoftBreakMetadata])
            let breaks = descendants(captured).compactMap { $0 as? SoftBreak }
            XCTAssertEqual(breaks.count, 1)
            XCTAssertEqual(breaks.first?.physicalLineEnding, eol)
            XCTAssertTrue(descendants(original).compactMap { $0 as? SoftBreak }.allSatisfy { $0.physicalLineEnding == nil })
            XCTAssertEqual(original.format(), captured.format())
        }
    }

    func testCodeMetadataAndLongDelimiterRemainOptIn() throws {
        let delimiter = String(repeating: "`", count: 100)
        let input = "prefix " + delimiter + " a\r\nb " + delimiter
        let parsed = Document(parsing: input, options: [.parseCodeMetadata, .parseWebCodeSyntax])
        let code = try XCTUnwrap(descendants(parsed).compactMap { $0 as? InlineCode }.first)
        XCTAssertEqual(code.codeMetadata?.value, "a\r\nb")
        XCTAssertEqual(code.code, "a b")
        XCTAssertNil(InlineCode("programmatic").codeMetadata)
    }

    func testReferenceIdentityAndLosslessFootnoteNodes() {
        let parsed = Document(parsing: "[name][ref]\n\n[ref]: /u\n\n[^n]: body\n\nuse[^n]", options: [.parseReferenceUsages, .parseFootnotes])
        let all = descendants(parsed)
        XCTAssertTrue(all.contains { $0 is LinkReferenceDefinition })
        XCTAssertTrue(all.contains { $0 is FootnoteDefinition })
        XCTAssertTrue(all.contains { $0 is FootnoteReference })
        XCTAssertEqual(all.compactMap { $0 as? Link }.first?.referenceIdentity?.identifier, "ref")
    }

    func testMathHighlightAndDefinitionListExtensions() {
        let parsed = Document(parsing: "==mark== and $$x$$\n\nterm\n: body", options: [.parseMath, .parseHighlight, .parseDefinitionLists])
        let kinds = Set(descendants(parsed).map { String(describing: type(of: $0)) })
        XCTAssertTrue(kinds.contains("Highlight"))
        XCTAssertTrue(kinds.contains("InlineMath"))
        XCTAssertTrue(kinds.contains("DefinitionList"))
    }

    func testCNodeTypeRewriteClearsEvidence() throws {
        let parser = try XCTUnwrap(aimarkdown_cmark_parser_new(Int32(AIMARKDOWN_CMARK_OPT_SOFTBREAK_METADATA)))
        defer { aimarkdown_cmark_parser_free(parser) }
        for part in ["a\r", "\nb"] {
            part.withCString { aimarkdown_cmark_parser_feed(parser, $0, part.utf8.count) }
        }
        let root = try XCTUnwrap(aimarkdown_cmark_parser_finish(parser))
        defer { aimarkdown_cmark_node_free(root) }
        let paragraph = try XCTUnwrap(aimarkdown_cmark_node_first_child(root))
        let softBreak = try XCTUnwrap(aimarkdown_cmark_node_next(aimarkdown_cmark_node_first_child(paragraph)))
        XCTAssertEqual(aimarkdown_cmark_node_get_softbreak_ending(softBreak), 0x0D0A)
        XCTAssertEqual(aimarkdown_cmark_node_set_type(softBreak, AIMARKDOWN_CMARK_NODE_LINEBREAK), 1)
        XCTAssertEqual(aimarkdown_cmark_node_set_type(softBreak, AIMARKDOWN_CMARK_NODE_SOFTBREAK), 1)
        XCTAssertEqual(aimarkdown_cmark_node_get_softbreak_ending(softBreak), 0)
    }
}
