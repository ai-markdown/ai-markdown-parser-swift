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

    func testListSpreadIsOptInParserEvidence() throws {
        // Expected values are the Web mdast `spread` fields for these inputs.
        let cases: [(String, [Bool])] = [
            ("- a\n- b", [false, false, false]),
            ("- a\n\n- b", [true, false, false]),
            ("- a\n\n  b", [false, true]),
            ("> - a\n>\n>", [true, false]),
            ("> - a\n>\n", [false, false]),
            ("1. a\r\n\r\n   b\r\n2. c", [false, true, false]),
        ]
        for (input, expected) in cases {
            let original = Document(parsing: input)
            let captured = Document(parsing: input, options: [.parseListSpreadMetadata])
            let spread: [Bool?] = descendants(captured).compactMap { node in
                switch node {
                case let list as UnorderedList: return .some(list.parsedSpread)
                case let list as OrderedList: return .some(list.parsedSpread)
                case let item as ListItem: return .some(item.parsedSpread)
                default: return nil
                }
            }
            XCTAssertEqual(spread, expected.map { $0 }, input)
            XCTAssertTrue(descendants(original).allSatisfy { node in
                (node as? ListItem)?.parsedSpread == nil && (node as? UnorderedList)?.parsedSpread == nil &&
                    (node as? OrderedList)?.parsedSpread == nil
            })
            XCTAssertEqual(original.format(), captured.format())
        }
        let unsupported = Document(parsing: "- a\n\n- b\n\nterm\n: body", options: [.parseListSpreadMetadata, .parseDefinitionLists])
        XCTAssertTrue(descendants(unsupported).compactMap { $0 as? UnorderedList }.allSatisfy { $0.parsedSpread == nil })
        XCTAssertNil(ListItem(Paragraph(Text("x"))).parsedSpread)
        XCTAssertNil(UnorderedList(ListItem(Paragraph(Text("x")))).parsedSpread)
        var item = try XCTUnwrap(descendants(Document(parsing: "- a\n\n  b", options: [.parseListSpreadMetadata])).compactMap { $0 as? ListItem }.first)
        XCTAssertEqual(item.parsedSpread, true)
        item.checkbox = .checked
        XCTAssertNil(item.parsedSpread)
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

    func testWebPositionsAreOptInParserEvidence() throws {
        // Expected ranges are the Web mdast positions for these inputs (1-based lines, UTF-8 columns, exclusive end).
        func location(_ line: Int, _ column: Int) -> SourceLocation { SourceLocation(line: line, column: column, source: nil) }
        let input = "- ```\n  a\n- b\n\n| a |\n| - |\n| b | c |\n\nx  \ny"
        let original = Document(parsing: input)
        let captured = Document(parsing: input, options: [.parseWebPositions])
        XCTAssertTrue(descendants(original).allSatisfy { $0.parsedWebRange == nil })
        XCTAssertEqual(descendants(original).map(\.range), descendants(captured).map(\.range))
        XCTAssertEqual(original.format(), captured.format())

        let items = descendants(captured).compactMap { $0 as? ListItem }
        // The unclosed fence swallows its line ending, so the item ends after the next item's prefix.
        XCTAssertEqual(items.first?.parsedWebRange, location(1, 1)..<location(3, 3))
        XCTAssertEqual(descendants(captured).first { $0 is CodeBlock }?.parsedWebRange, location(1, 3)..<location(3, 1))
        let rows = descendants(captured).compactMap { $0 as? Table.Row }
        XCTAssertEqual(rows.first?.parsedCellCount, 2)
        XCTAssertEqual(rows.first?.childCount, 1)
        XCTAssertEqual(rows.first?.child(at: 0)?.parsedWebRange, location(7, 1)..<location(7, 5))
        let lineBreak = descendants(captured).first { $0 is LineBreak }
        XCTAssertEqual(lineBreak?.parsedWebRange, location(9, 2)..<location(10, 1))

        XCTAssertNil(ListItem(Paragraph(Text("x"))).parsedWebRange)
        XCTAssertNil(Table.Row([Table.Cell(Text("x"))]).parsedCellCount)
        var item = try XCTUnwrap(items.first)
        item.checkbox = .checked
        XCTAssertNil(item.parsedWebRange)
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
