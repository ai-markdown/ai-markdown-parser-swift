import AIMarkdownParser
import AIMarkdownCMark
import XCTest

final class ParserBoundaryTests: XCTestCase {
    func testWebTablesRetainActualCellsWithoutWideningHeaderAlignments() throws {
        // The pinned Web raw tree preserves cells; its display handler separately normalizes width.
        let source = "| A | B |\n| - | - |\n| short |\n| a | b | c |\n|"
        let parsed = Document(parsing: source, options: [.parseWebTables, .parseWebPositions])
        let table = try XCTUnwrap(parsed.child(at: 0) as? Table)
        XCTAssertEqual(table.columnAlignments.count, 2)
        XCTAssertEqual(Array(table.body.rows).map(\.childCount), [1, 3, 1])
        XCTAssertEqual(Array(table.body.rows).map(\.parsedCellCount), [1, 3, 1])
        XCTAssertTrue(table.body.rows.allSatisfy { $0.parsedWebRange != nil })
        let copy = try XCTUnwrap(table.withUncheckedChildren(Array(table.children)) as? Table)
        XCTAssertEqual(copy.columnAlignments.count, 2)
        XCTAssertEqual(Array(copy.body.rows).map(\.childCount), [1, 3, 1])
        let legacy = Document(parsing: "| A | B |\n| - | - |\n| short |\n| a | b | c |")
        let old = try XCTUnwrap(legacy.child(at: 0) as? Table)
        XCTAssertEqual(Array(old.body.rows).map(\.childCount), [2, 2])
    }

    func testMathPrefixOnlyEOFAndFootnoteBlankWhitespaceUsePhysicalInput() throws {
        // Exact values from the pinned Web MathBoundary inputs, before display transforms.
        let cases: [(String, String, String?)] = [
            ("> $$\n> x+y\n>", "x+y", nil),
            ("> $$\n> x+y\n> ", "x+y", nil),
            ("term\n: $$ latex\n  x+y\n ", "x+y", "latex"),
            ("term\n: $$ latex\n  x+y\n  ", "x+y", "latex"),
            ("- $$x$$ [^n]\n\n[^n]: note\n\n    $$\n ", " ", nil),
            ("- $$x$$ [^n]\n\n[^n]: note\n\n    $$\n  ", "  ", nil),
            ("- $$x$$ [^n]\n\n[^n]: note\n\n    $$\n   ", "   ", nil),
            ("- $$x$$ [^n]\n\n[^n]: note\n\n    $$\n    ", "    ", nil),
            ("- $$x$$ [^n]\n\n[^n]: note\n\n    $$\n    y\n ", "y\n ", nil),
            ("- $$x$$ [^n]\n\n[^n]: note\n\n    $$\n    y\n  ", "y\n  ", nil),
            ("- $$x$$ [^n]\n\n[^n]: note\n\n    $$\n    y\n   ", "y\n   ", nil),
            ("- $$x$$ [^n]\n\n[^n]: note\n\n    $$\n    y\n    ", "y\n    ", nil),
        ]
        for ending in ["\n", "\r\n", "\r"] {
            for (source, value, meta) in cases {
                let input = source.replacingOccurrences(of: "\n", with: ending)
                let document = Document(parsing: input, options: [.parseMath, .parseFootnotes, .parseDefinitionLists])
                let nodes = descendants(document).compactMap { $0 as? MathBlock }
                XCTAssertEqual(nodes.count, 1, input.debugDescription)
                let math = try XCTUnwrap(nodes.first)
                XCTAssertEqual(math.value, value.replacingOccurrences(of: "\n", with: ending), input.debugDescription)
                XCTAssertEqual(math.meta, meta, input.debugDescription)
            }
        }
    }

    func testMathContainerBoundaryValuesAcrossPackageBoundary() throws {
        // Pinned Web e9ec1799fdc380d99ce0a6313023515895bbd7bf regressions.
        // A container's flow boundary excludes one EOL; it is not trimEnd.
        let cases = [
            ("- Item\n\n  $$\n  x\n\nAfter", "x"),
            ("- Item\n\n  $$\n  x\n\n\nAfter", "x\n"),
            ("- Item\n\n  $$\n  x  \n  \nAfter", "x  "),
            ("- Item\n\n  $$\n  x\n  \t \nAfter", "x\n\t "),
            ("- Item\n\n  $$\n  x\n\n", "x\n"),
            ("- Item\n\n  $$\n  x\n\n  $$\nAfter", "x\n"),
            ("- Item\n\n  $$\n  x\n\n- Next", "x\n"),
            ("- Item\n\n  $$\n  x\n\n> After", "x\n"),
            ("> $$\n> x\n>\nAfter", "x"),
            ("$$\nx\n\n", "x\n"),
        ]
        for eol in ["\n", "\r\n", "\r"] {
            for (raw, value) in cases {
                let source = raw.replacingOccurrences(of: "\n", with: eol)
                let expected = value.replacingOccurrences(of: "\n", with: eol)
                let document = Document(parsing: source, options: [.parseMath])
                let math = try XCTUnwrap(descendants(document).compactMap { $0 as? MathBlock }.first)
                XCTAssertEqual(math.value, expected, source.debugDescription)
                XCTAssertNotNil(math.range)
                XCTAssertTrue(descendants(Document(parsing: source)).allSatisfy { !($0 is MathBlock) })
            }
        }
    }

    func testTaskCallbackRetainsRawLineEndingRemainderAndEventCoordinates() throws {
        for eol in ["\n", "\r\n", "\r"] {
            let document = Document(parsing: "- [x]\(eol)next", options: [.parseWebCharacterReferences, .parseWebPositions, .parseSoftBreakMetadata])
            let paragraph = try XCTUnwrap(descendants(document).compactMap { $0 as? Paragraph }.first)
            XCTAssertEqual(paragraph.parsedWebRange?.lowerBound.line, 1)
            XCTAssertEqual(paragraph.parsedWebRange?.lowerBound.column, 7)
            XCTAssertEqual(paragraph.childCount, eol == "\r\n" ? 2 : 1)
            if eol == "\r\n" {
                let residual = try XCTUnwrap(paragraph.child(at: 0) as? SoftBreak)
                XCTAssertEqual(residual.physicalLineEnding, "\n")
                XCTAssertEqual(residual.parsedWebRange?.lowerBound.line, 1)
                XCTAssertEqual(residual.parsedWebRange?.lowerBound.column, 7)
                XCTAssertEqual(residual.parsedWebRange?.upperBound.line, 2)
                XCTAssertEqual(residual.parsedWebRange?.upperBound.column, 1)
            }
            XCTAssertEqual(descendants(document).compactMap { $0 as? Text }.first?.string, "next")
            let uncaptured = Document(parsing: "- [x]\(eol)next", options: [.parseWebCharacterReferences])
            XCTAssertTrue(descendants(uncaptured).compactMap { $0 as? SoftBreak }.allSatisfy { $0.physicalLineEnding == nil })
        }
    }

    func testListCodeBoundaryUsesFollowingContainerEvent() throws {
        let cases = [
            ("After", "code"), ("# After", "code"),
            ("- next", "code\n"), ("1. next", "code\n"), ("> next", "code\n"),
        ]
        for eol in ["\n", "\r\n", "\r"] {
            for (suffix, value) in cases {
                let source = ("- item\n\n  ```\n  code\n\n" + suffix).replacingOccurrences(of: "\n", with: eol)
                let parsed = Document(parsing: source, options: [.parseCodeMetadata, .parseWebPositions])
                let code = try XCTUnwrap(descendants(parsed).compactMap { $0 as? CodeBlock }.first)
                XCTAssertEqual(code.codeMetadata?.value, value.replacingOccurrences(of: "\n", with: eol))
            }
        }
    }

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
        let definitions = Document(parsing: "- a\n\n- b\n\nterm\n: body", options: [.parseListSpreadMetadata, .parseDefinitionLists])
        XCTAssertEqual(descendants(definitions).compactMap { $0 as? UnorderedList }.map(\.parsedSpread), [true])
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

    func testDefinitionContainersRetainListLedgerAndPhysicalTermPositions() throws {
        // Actual Web e9ec1799 raw events, also frozen in the C definition ledger fixture.
        func point(_ line: Int, _ column: Int) -> SourceLocation { SourceLocation(line: line, column: column, source: nil) }
        let source = "> term\n> : - a\n>\n>   - b\n"
        let parsed = Document(parsing: source, options: [.parseDefinitionLists, .parseListSpreadMetadata, .parseWebPositions])
        let nodes = descendants(parsed)
        let list = try XCTUnwrap(nodes.compactMap { $0 as? UnorderedList }.first)
        XCTAssertEqual(list.parsedSpread, true)
        XCTAssertEqual(nodes.compactMap { $0 as? ListItem }.map(\.parsedSpread), [false, false])
        XCTAssertEqual(list.parsedWebRange, point(2, 5)..<point(4, 8))
        XCTAssertEqual(nodes.first { $0 is DefinitionList }?.parsedWebRange, point(1, 3)..<point(5, 1))
        XCTAssertEqual(nodes.first { $0 is DefinitionTerm }?.parsedWebRange, point(1, 3)..<point(2, 1))
        XCTAssertEqual(nodes.first { $0 is DefinitionDescription }?.parsedWebRange, point(2, 5)..<point(5, 1))
        XCTAssertEqual(nodes.first { $0 is BlockQuote }?.parsedWebRange, point(1, 1)..<point(5, 1))
        XCTAssertEqual(nodes.compactMap { $0 as? Text }.first?.parsedWebRange, point(1, 3)..<point(1, 7))

        let legacy = Document(parsing: source, options: [.parseDefinitionLists])
        XCTAssertTrue(descendants(legacy).allSatisfy { $0.parsedWebRange == nil })
        XCTAssertNil(descendants(legacy).compactMap { $0 as? UnorderedList }.first?.parsedSpread)
        let moved = Document(parsing: "> quote\nTerm\n: body", options: [.parseDefinitionLists, .parseWebPositions])
        XCTAssertEqual(descendants(moved).first { $0 is BlockQuote }?.parsedWebRange, point(1, 1)..<point(2, 1))
        XCTAssertEqual(descendants(moved).compactMap { $0 as? Text }.last?.parsedWebRange, point(3, 3)..<point(3, 7))
    }

    func testUnmatchedTildesAndMultilineDeleteCarryActualTokenPositions() throws {
        func point(_ line: Int, _ column: Int) -> SourceLocation { SourceLocation(line: line, column: column, source: nil) }
        for source in ["~", "~~"] {
            let parsed = Document(parsing: source, options: [.parseWebEmphasis, .parseWebPositions])
            let text = try XCTUnwrap(descendants(parsed).compactMap { $0 as? Text }.first)
            XCTAssertEqual(text.string, source)
            XCTAssertEqual(text.parsedWebRange, point(1, 1)..<point(1, source.utf8.count + 1))
        }
        let source = "> ~~a\r\n> b~~"
        let parsed = Document(parsing: source, options: [.parseWebEmphasis, .parseWebPositions])
        let node = try XCTUnwrap(descendants(parsed).first { $0 is Strikethrough })
        XCTAssertEqual(node.parsedWebRange, point(1, 3)..<point(2, 6))
        let legacy = Document(parsing: source, options: [.parseWebEmphasis])
        XCTAssertTrue(descendants(legacy).allSatisfy { $0.parsedWebRange == nil })
        XCTAssertEqual(descendants(parsed).map(\.range), descendants(legacy).map(\.range))
    }

    func testDefinitionSplitsRetainPhysicalBreakAndPartialTabEvents() throws {
        func point(_ line: Int, _ column: Int) -> SourceLocation { SourceLocation(line: line, column: column, source: nil) }
        let options: ParseOptions = [.parseDefinitionLists, .parseWebPositions, .parseSoftBreakMetadata]
        let hard = Document(parsing: "First  \r\nSecond\r\n: description", options: options)
        XCTAssertEqual(descendants(hard).first { $0 is LineBreak }?.parsedWebRange, point(1, 6)..<point(2, 1))
        let source = "Term\r\n: value\r\n\tSecond\r\n\t: value"
        let parsed = Document(parsing: source, options: options)
        let soft = try XCTUnwrap(descendants(parsed).compactMap { $0 as? SoftBreak }.first)
        XCTAssertEqual(soft.physicalLineEnding, "\r\n")
        XCTAssertEqual(soft.parsedWebRange, point(2, 8)..<point(3, 1))
        let term = try XCTUnwrap(descendants(parsed).compactMap { $0 as? Text }.first { $0.string == "  Second" })
        XCTAssertEqual(term.parsedWebRange, point(3, 2)..<point(3, 8))
    }

    func testParagraphAndThematicBreakRetainPhysicalBlockEvents() throws {
        func point(_ line: Int, _ column: Int) -> SourceLocation { SourceLocation(line: line, column: column, source: nil) }
        let source = "> quote\nTerm\n: description\n\n  * * *  \n"
        let parsed = Document(parsing: source, options: [.parseDefinitionLists, .parseWebPositions, .parseSoftBreakMetadata])
        XCTAssertEqual(descendants(parsed).first { $0 is Paragraph }?.parsedWebRange, point(1, 3)..<point(2, 1))
        XCTAssertEqual(descendants(parsed).first { $0 is ThematicBreak }?.parsedWebRange, point(5, 3)..<point(5, 10))
    }

    func testWebHTMLBlocksRetainLazyContainerAndRemainOptIn() throws {
        func point(_ line: Int, _ column: Int) -> SourceLocation { SourceLocation(line: line, column: column, source: nil) }
        let source = "[^f]: body\n<img>\ntext"
        let parsed = Document(parsing: source, options: [.parseFootnotes, .parseWebHTMLBlocks, .parseWebPositions])
        let html = try XCTUnwrap(descendants(parsed).compactMap { $0 as? HTMLBlock }.first)
        XCTAssertTrue(html.parent is FootnoteDefinition)
        XCTAssertEqual(html.rawHTML, "<img>\n")
        XCTAssertEqual(html.parsedWebRange, point(2, 1)..<point(2, 6))
        let untracked = Document(parsing: source, options: [.parseFootnotes, .parseWebHTMLBlocks])
        XCTAssertEqual(parsed.debugDescription(options: [.printSourceLocations]), untracked.debugDescription(options: [.printSourceLocations]))
        XCTAssertEqual(descendants(parsed).map(\.range), descendants(untracked).map(\.range))
        XCTAssertTrue(descendants(untracked).allSatisfy { $0.parsedWebRange == nil })
        let legacy = Document(parsing: source, options: [.parseFootnotes, .parseWebPositions])
        XCTAssertTrue(descendants(legacy).compactMap { $0 as? HTMLBlock }.first?.parent is Document)
        let declaration = Document(parsing: "text\n<!doctype html>\ntail", options: [.parseWebHTMLBlocks])
        XCTAssertEqual(descendants(declaration).compactMap { $0 as? HTMLBlock }.count, 1)
        XCTAssertEqual(descendants(Document(parsing: "text\n<!doctype html>\ntail")).compactMap { $0 as? HTMLBlock }.count, 0)
    }
}
