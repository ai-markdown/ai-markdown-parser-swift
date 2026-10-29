// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2021-2023 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
 See https://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

import AIMarkdownCMark
import AIMarkdownCMarkExtensions
import Foundation

/// String-based CommonMark node type identifiers.
///
/// CommonMark node types do have a raw-value enum `cmark_node_type`.
/// However, in light of extensions, these enum values are not public and
/// must use strings instead to identify types.
///
/// For example, consider the task list item:
///
/// ```markdown
/// - [x] Completed
/// ```
///
/// This internally reuses the regular `CMARK_NODE_ITEM` enum type but will
/// return the type name string "tasklist" or "item" depending on whether
/// there was a `[ ]` or `[x]` after the list marker.
/// So, the raw `cmark_node_type` is no longer reliable on its own, unfortunately.
///
/// These values are taken from the `cmark_node_get_type_string` implementation
/// in the underlying cmark dependency.
///
/// > Warning: **Do not make these public.**.
fileprivate enum CommonMarkNodeType: String {
    case document
    case footnoteDefinition = "footnote_definition"
    case footnoteReference = "footnote_reference"
    case blockQuote = "block_quote"
    case list
    case item
    case codeBlock = "code_block"
    case linkReferenceDefinition = "link_reference_definition"
    case mathBlock = "math_block"
    case inlineMath = "math_inline"
    case htmlBlock = "html_block"
    case definitionList = "definition_list"
    case definitionTerm = "definition_term"
    case definitionDescription = "definition_description"
    case customBlock = "custom_block"
    case paragraph
    case heading
    case thematicBreak = "thematic_break"
    case text
    case softBreak = "softbreak"
    case lineBreak = "linebreak"
    case code
    case html = "html_inline"
    case customInline = "custom_inline"
    case emphasis = "emph"
    case strong
    case link
    case image
    case inlineAttributes = "attribute"
    case none = "NONE"
    case unknown = "<unknown>"

    // Extensions

    case strikethrough
    case highlight

    case table
    case tableHead = "table_header"
    case tableRow = "table_row"
    case tableCell = "table_cell"

    case taskListItem = "tasklist"
}

/// Represents the result of a cmark conversion: the current `MarkupConverterState` and the resulting converted node.
fileprivate struct MarkupConversion<Result> {
    let state: MarkupConverterState
    let result: Result
}

/// Represents the current state of cmark -> `Markup` conversion.
fileprivate struct MarkupConverterState {
    fileprivate struct PendingTableBody {
        var range: SourceRange?
    }
    /// The original source whose conversion created this state.
    let source: URL?

    /// An opaque pointer to a `cmark_iter` used during parsing.
    let iterator: UnsafeMutablePointer<aimarkdown_cmark_iter>?

    /// The last `cmark_event_type` during parsing.
    let event: aimarkdown_cmark_event_type

    /// An opaque pointer to the last parsed `cmark_node`.
    let node: UnsafeMutablePointer<aimarkdown_cmark_node>?

    /// Options to consider when converting to `Markup` elements.
    let options: ParseOptions

    private(set) var headerSeen: Bool
    private(set) var pendingTableBody: PendingTableBody?

    init(source: URL?, iterator: UnsafeMutablePointer<aimarkdown_cmark_iter>?, event: aimarkdown_cmark_event_type, node: UnsafeMutablePointer<aimarkdown_cmark_node>?, options: ParseOptions, headerSeen: Bool, pendingTableBody: PendingTableBody?) {
        self.source = source
        self.iterator = iterator
        self.event = event
        self.node = node
        self.options = options
        self.headerSeen = headerSeen
        self.pendingTableBody = pendingTableBody

        switch (event, nodeType) {
        case (AIMARKDOWN_CMARK_EVENT_EXIT, .tableHead):
            self.headerSeen = true
        case (AIMARKDOWN_CMARK_EVENT_ENTER, .tableRow) where headerSeen:
            if self.pendingTableBody == nil {
                self.pendingTableBody = PendingTableBody(range: self.range(self.node))
                precondition(self.pendingTableBody != nil)
            }
        case (AIMARKDOWN_CMARK_EVENT_EXIT, .table):
            if let endOfTable = self.range(self.node)?.upperBound,
               let pendingTableRange = self.pendingTableBody?.range {
                self.pendingTableBody?.range = pendingTableRange.lowerBound..<endOfTable
            }
        default:
            break
        }
    }

    /// Get the next cmark iterator and node, returning a new state.
    func next(clearPendingTableBody: Bool = false) -> MarkupConverterState {
        let newEvent = aimarkdown_cmark_iter_next(iterator)
        let newNode = aimarkdown_cmark_iter_get_node(iterator)
        return MarkupConverterState(source: source, iterator: iterator, event: newEvent, node: newNode, options: options, headerSeen: clearPendingTableBody ? false : headerSeen, pendingTableBody: clearPendingTableBody ? nil : pendingTableBody)
    }

    /// The type of the last parsed cmark node.
    var nodeType: CommonMarkNodeType {
        // cmark 0.9.0 exposes footnote enum constants, but its type-string getter
        // returns "<unknown>" for them. Only these built-in types use enum dispatch.
        switch aimarkdown_cmark_node_get_type(node) {
        case AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION:
            return .footnoteDefinition
        case AIMARKDOWN_CMARK_NODE_FOOTNOTE_REFERENCE:
            return .footnoteReference
        default:
            break
        }
        let typeString = String(cString: aimarkdown_cmark_node_get_type_string(node))
        guard let type = CommonMarkNodeType(rawValue: typeString) else {
            fatalError("Unknown cmark node type '\(typeString)' encountered during conversion")
        }
        return type
    }

    /// The source range where a node occurred, according to cmark.
    func range(_ node: UnsafeMutablePointer<aimarkdown_cmark_node>?) -> SourceRange? {
        let startLine = Int(aimarkdown_cmark_node_get_start_line(node))
        let startColumn = Int(aimarkdown_cmark_node_get_start_column(node))
        guard startLine > 0 && startColumn > 0 else {
            // cmark doesn't track the positions for this node.
            return nil
        }

        let endLine = Int(aimarkdown_cmark_node_get_end_line(node))
        let endColumn = Int(aimarkdown_cmark_node_get_end_column(node)) + 1

        guard endLine > 0 && endColumn > 0 else {
            // cmark doesn't track the positions for this node.
            return nil
        }

        // If this is a symbol link / code span, set the locations to include the ticks.
        let backtickCount = Int(aimarkdown_cmark_node_get_backtick_count(node))

        let start = SourceLocation(line: startLine, column: startColumn - backtickCount, source: source)
        let end = SourceLocation(line: endLine, column: endColumn + backtickCount, source: source)

        // Sometimes the cmark range is invalid (rdar://73376719)
        guard start <= end else { return nil }
        return start..<end
    }
}

/// Parses markup source and returns a `Markup` node representing the parsed source.
struct MarkupParser {
    /// One frame per actual container; only next advances the cmark iterator and table state.
    private struct ConversionFrame {
        let state: MarkupConverterState
        let type: CommonMarkNodeType
        let range: SourceRange?
        var children: [RawMarkup] = []

        init(_ state: MarkupConverterState) {
            self.state = state
            type = state.nodeType
            range = state.range(state.node)
        }
    }

    /// Explicit enter/exit postorder conversion; all containers share one loop without Swift recursion proportional to AST depth.
    private static func convertAnyElement(_ initialState: MarkupConverterState) -> MarkupConversion<RawMarkup> {
        precondition(initialState.event == AIMARKDOWN_CMARK_EVENT_ENTER)
        var state = initialState
        var frames: [ConversionFrame] = []
        while true {
            if state.event == AIMARKDOWN_CMARK_EVENT_ENTER {
                switch state.nodeType {
                case .document, .footnoteDefinition, .blockQuote, .list, .item, .customBlock,
                     .paragraph, .heading, .emphasis, .strong, .link, .image, .strikethrough,
                     .highlight, .definitionList, .definitionTerm, .definitionDescription,
                     .taskListItem, .table, .tableHead, .tableRow, .tableCell, .inlineAttributes:
                    frames.append(ConversionFrame(state))
                    state = state.next()
                default:
                    let conversion = convertLeaf(state)
                    guard !frames.isEmpty else { return conversion }
                    frames[frames.count - 1].children.append(conversion.result)
                    state = conversion.state
                }
            } else {
                precondition(state.event == AIMARKDOWN_CMARK_EVENT_EXIT)
                guard let frame = frames.popLast() else { preconditionFailure("Missing cmark container conversion frame") }
                precondition(state.node == frame.state.node)
                let result = finishContainer(frame, exit: state)
                state = state.next(clearPendingTableBody: frame.type == .table)
                guard !frames.isEmpty else { return MarkupConversion(state: state, result: result) }
                frames[frames.count - 1].children.append(result)
            }
        }
    }

    private static func finishContainer(_ frame: ConversionFrame, exit: MarkupConverterState) -> RawMarkup {
        let state = frame.state
        let range = frame.range
        let children = frame.children
        switch frame.type {
        case .document: return .document(parsedRange: range, children)
        case .footnoteDefinition: return .footnoteDefinition(label: getLiteralContent(node: state.node), parsedRange: range, children)
        case .blockQuote: return .blockQuote(parsedRange: range, children)
        case .list:
            for child in children {
                guard case .listItem = child.data else { fatalError("Converted cmark list had a node other than RawMarkup.listItem") }
            }
            switch aimarkdown_cmark_node_get_list_type(state.node) {
            case AIMARKDOWN_CMARK_BULLET_LIST: return .unorderedList(parsedRange: range, children)
            case AIMARKDOWN_CMARK_ORDERED_LIST: return .orderedList(parsedRange: range, children, startIndex: UInt(aimarkdown_cmark_node_get_list_start(state.node)))
            default: fatalError("cmark reported a list node but said its list type is CMARK_NO_LIST?")
            }
        case .item: return .listItem(checkbox: .none, parsedRange: range, children)
        case .taskListItem:
            let checkbox: Checkbox = aimarkdown_cmark_gfm_extensions_get_tasklist_item_checked(state.node) ? .checked : .unchecked
            return .listItem(checkbox: checkbox, parsedRange: range, children)
        case .customBlock: return .customBlock(parsedRange: range, children)
        case .paragraph: return .paragraph(parsedRange: range, children)
        case .heading: return .heading(level: Int(aimarkdown_cmark_node_get_heading_level(state.node)), parsedRange: range, children)
        case .emphasis: return .emphasis(parsedRange: range, children)
        case .strong: return .strong(parsedRange: range, children)
        case .strikethrough: return .strikethrough(parsedRange: range, children)
        case .highlight: return .highlight(parsedRange: range, children)
        case .definitionList: return .definitionList(parsedRange: range, children)
        case .definitionTerm: return .definitionTerm(parsedRange: range, children)
        case .definitionDescription: return .definitionDescription(spread: aimarkdown_cmark_node_get_definition_spread(state.node) != 0, parsedRange: range, children)
        case .link:
            let reference = referenceIdentity(state)
            let destination = String(cString: aimarkdown_cmark_node_get_url(state.node))
            let title = String(cString: aimarkdown_cmark_node_get_title(state.node))
            return .link(destination: reference == nil && !state.options.contains(.parseWebCharacterReferences) && destination.isEmpty ? nil : destination,
                         title: title.isEmpty ? nil : title, parsedRange: range, referenceIdentity: reference, children)
        case .image:
            let reference = referenceIdentity(state)
            let source = String(cString: aimarkdown_cmark_node_get_url(state.node))
            let title = String(cString: aimarkdown_cmark_node_get_title(state.node))
            return .image(source: reference == nil && !state.options.contains(.parseWebCharacterReferences) && source.isEmpty ? nil : source,
                          title: title.isEmpty ? nil : title, parsedRange: range, referenceIdentity: reference, children)
        case .table: return finishTable(state, range: range, children: children, exit: exit)
        case .tableHead: return .tableHead(parsedRange: range, columns: children)
        case .tableRow: return .tableRow(parsedRange: range, children)
        case .tableCell:
            return .tableCell(parsedRange: range, colspan: UInt(aimarkdown_cmark_gfm_extensions_get_table_cell_colspan(state.node)),
                              rowspan: UInt(aimarkdown_cmark_gfm_extensions_get_table_cell_rowspan(state.node)), children)
        case .inlineAttributes:
            return .inlineAttributes(attributes: String(cString: aimarkdown_cmark_node_get_attributes(state.node)), parsedRange: range, children)
        default: preconditionFailure("Expected a cmark container type")
        }
    }

    private static func finishTable(_ state: MarkupConverterState, range: SourceRange?, children input: [RawMarkup], exit: MarkupConverterState) -> RawMarkup {
        let columnCount = Int(aimarkdown_cmark_gfm_extensions_get_table_columns(state.node))
        let alignments = (0..<columnCount).map { column -> Table.ColumnAlignment? in
            let ascii = aimarkdown_cmark_gfm_extensions_get_table_alignments(state.node)[column]
            let character = Character(UnicodeScalar(ascii))
            switch character {
            case "l": return .left
            case "r": return .right
            case "c": return .center
            case "\0": return nil
            default: fatalError("Unexpected table column character for cmark table: \(character) (0x\(String(ascii, radix: 16)))")
            }
        }
        var rows = input
        let header: RawMarkup
        if let first = rows.first, case .tableHead = first.data {
            header = first
            rows.removeFirst()
        } else { header = .tableHead(parsedRange: nil, columns: []) }
        let body: RawMarkup
        if rows.isEmpty {
            precondition(exit.pendingTableBody == nil)
            body = .tableBody(parsedRange: nil, rows: [])
        } else {
            guard let pending = exit.pendingTableBody else { preconditionFailure("Table body is missing its actual iterator state") }
            body = .tableBody(parsedRange: pending.range, rows: rows)
        }
        return .table(columnAlignments: alignments, parsedRange: range, header: header, body: body)
    }

    private static func convertLeaf(_ state: MarkupConverterState) -> MarkupConversion<RawMarkup> {
        switch state.nodeType {
        case .footnoteReference:
            // cmark still emits ENTER/EXIT for this leaf; preserve the existing explicit skip.
            let label = getLiteralContent(node: state.node)
            let range = state.range(state.node)
            let exit = state.next()
            precondition(exit.event == AIMARKDOWN_CMARK_EVENT_EXIT && exit.node == state.node)
            return MarkupConversion(state: exit.next(), result: .footnoteReference(label: label, parsedRange: range))
        case .linkReferenceDefinition:
            let sourceLabel = String(cString: aimarkdown_cmark_node_get_reference_identifier_source(state.node))
            let label = String(cString: aimarkdown_cmark_node_get_reference_label(state.node))
            let destination = String(cString: aimarkdown_cmark_node_get_reference_destination(state.node))
            let title = aimarkdown_cmark_node_get_reference_title(state.node).map { String(cString: $0) }
            return MarkupConversion(state: state.next(), result: .linkReferenceDefinition(parsedRange: state.range(state.node), label: label,
                identifier: referenceIdentifier(sourceLabel), destination: destination, title: title, labelSource: sourceLabel))
        case .mathBlock:
            let value = getLiteralContent(node: state.node)
            let meta = String(cString: aimarkdown_cmark_node_get_fence_info(state.node))
            return MarkupConversion(state: state.next(), result: .mathBlock(parsedRange: state.range(state.node), value: value, meta: meta.isEmpty ? nil : meta))
        case .inlineMath: return MarkupConversion(state: state.next(), result: .inlineMath(parsedRange: state.range(state.node), value: getLiteralContent(node: state.node)))
        case .codeBlock: return convertCodeBlock(state)
        case .htmlBlock: return convertHTMLBlock(state)
        case .thematicBreak: return convertThematicBreak(state)
        case .text: return convertText(state)
        case .softBreak: return convertSoftBreak(state)
        case .lineBreak: return convertLineBreak(state)
        case .code: return convertInlineCode(state)
        case .html: return convertInlineHTML(state)
        case .customInline: return convertCustomInline(state)
        default: fatalError("Unknown cmark node type '\(state.nodeType.rawValue)' encountered during conversion")
        }
    }

    private static func getLiteralContent(node: UnsafeMutablePointer<aimarkdown_cmark_node>!) -> String {
        guard let rawText = aimarkdown_cmark_node_get_literal(node) else {
            fatalError("Expected literal content for cmark node but got null pointer")
        }
        return String(cString: rawText)
    }

    private static func referenceIdentifier(_ text: String) -> String {
        guard let identifier = aimarkdown_cmark_reference_identifier(text) else { return "" }
        defer { aimarkdown_cmark_reference_identifier_free(identifier) }
        return String(cString: identifier)
    }

    private static func referenceIdentity(_ state: MarkupConverterState) -> ReferenceIdentity? {
        let kind = aimarkdown_cmark_node_get_reference_usage_type(state.node)
        guard kind != 0 else { return nil }
        let source = String(cString: aimarkdown_cmark_node_get_reference_usage_source(state.node))
        let label = String(cString: aimarkdown_cmark_node_get_reference_usage_label(state.node))
        let type: ReferenceIdentity.ReferenceType = kind == 3 ? .full : kind == 2 ? .collapsed : .shortcut
        return ReferenceIdentity(identifier: referenceIdentifier(source), label: label, referenceType: type, labelSource: source)
    }

    private static func convertCodeBlock(_ state: MarkupConverterState) -> MarkupConversion<RawMarkup> {
        precondition(state.event == AIMARKDOWN_CMARK_EVENT_ENTER)
        precondition(state.nodeType == .codeBlock)
        let parsedRange = state.range(state.node)
        let language = String(cString: aimarkdown_cmark_node_get_fence_info(state.node))
        let code = getLiteralContent(node: state.node)

        return MarkupConversion(state: state.next(), result: .codeBlock(parsedRange: parsedRange, code: code, language: language.isEmpty ? nil : language, metadata: CodeSyntaxMetadata(node: state.node)))
    }

    private static func convertHTMLBlock(_ state: MarkupConverterState) -> MarkupConversion<RawMarkup> {
        precondition(state.event == AIMARKDOWN_CMARK_EVENT_ENTER)
        precondition(state.nodeType == .htmlBlock)
        let parsedRange = state.range(state.node)
        let html = getLiteralContent(node: state.node)
        return MarkupConversion(state: state.next(), result: .htmlBlock(parsedRange: parsedRange, html: html))
    }

    private static func convertThematicBreak(_ state: MarkupConverterState) -> MarkupConversion<RawMarkup> {
        precondition(state.event == AIMARKDOWN_CMARK_EVENT_ENTER)
        precondition(state.nodeType == .thematicBreak)
        let parsedRange = state.range(state.node)
        return MarkupConversion(state: state.next(), result: .thematicBreak(parsedRange: parsedRange))
    }

    private static func convertText(_ state: MarkupConverterState) -> MarkupConversion<RawMarkup> {
        precondition(state.event == AIMARKDOWN_CMARK_EVENT_ENTER)
        precondition(state.nodeType == .text)
        let parsedRange = state.range(state.node)
        let string = getLiteralContent(node: state.node)
        return MarkupConversion(state: state.next(), result: .text(parsedRange: parsedRange, string: string))
    }

    private static func convertSoftBreak(_ state: MarkupConverterState) -> MarkupConversion<RawMarkup> {
        precondition(state.event == AIMARKDOWN_CMARK_EVENT_ENTER)
        precondition(state.nodeType == .softBreak)
        let parsedRange = state.range(state.node)
        let ending: String?
        switch aimarkdown_cmark_node_get_softbreak_ending(state.node) {
        case 10: ending = "\n"
        case 13: ending = "\r"
        case 0x0d0a: ending = "\r\n"
        default: ending = nil
        }
        return MarkupConversion(state: state.next(), result: .softBreak(parsedRange: parsedRange, physicalLineEnding: ending))
    }

    private static func convertLineBreak(_ state: MarkupConverterState) -> MarkupConversion<RawMarkup> {
        precondition(state.event == AIMARKDOWN_CMARK_EVENT_ENTER)
        precondition(state.nodeType == .lineBreak)
        let parsedRange = state.range(state.node)
        return MarkupConversion(state: state.next(), result: .lineBreak(parsedRange: parsedRange))
    }

    private static func convertInlineCode(_ state: MarkupConverterState) -> MarkupConversion<RawMarkup> {
        precondition(state.event == AIMARKDOWN_CMARK_EVENT_ENTER)
        precondition(state.nodeType == .code)
        let parsedRange = state.range(state.node)
        let literalContent = getLiteralContent(node: state.node)
        if state.options.contains(.parseSymbolLinks),
           aimarkdown_cmark_node_get_backtick_count(state.node) > 1 {
            return MarkupConversion(state: state.next(), result: .symbolLink(parsedRange: parsedRange, destination: literalContent))
        } else {
            return MarkupConversion(state: state.next(), result: .inlineCode(parsedRange: parsedRange, code: literalContent, metadata: CodeSyntaxMetadata(node: state.node)))
        }
    }

    private static func convertInlineHTML(_ state: MarkupConverterState) -> MarkupConversion<RawMarkup> {
        precondition(state.event == AIMARKDOWN_CMARK_EVENT_ENTER)
        precondition(state.nodeType == .html)
        let parsedRange = state.range(state.node)
        let html = getLiteralContent(node: state.node)
        return MarkupConversion(state: state.next(), result: .inlineHTML(parsedRange: parsedRange, html: html))
    }

    private static func convertCustomInline(_ state: MarkupConverterState) -> MarkupConversion<RawMarkup> {
        precondition(state.event == AIMARKDOWN_CMARK_EVENT_ENTER)
        precondition(state.nodeType == .customInline)
        let parsedRange = state.range(state.node)
        let text = getLiteralContent(node: state.node)
        return MarkupConversion(state: state.next(), result: .customInline(parsedRange: parsedRange, text: text))
    }

    static func parseString(_ string: String, source: URL?, options: ParseOptions) -> Document {
        aimarkdown_cmark_gfm_core_extensions_ensure_registered()

        var cmarkOptions = AIMARKDOWN_CMARK_OPT_TABLE_SPANS
        if options.contains(.parseWebCodeSyntax) { cmarkOptions |= AIMARKDOWN_CMARK_OPT_WEB_CODE_SYNTAX }
        if options.contains(.parseSoftBreakMetadata) { cmarkOptions |= Int32(AIMARKDOWN_CMARK_OPT_SOFTBREAK_METADATA) }
        if options.contains(.parseCodeMetadata) { cmarkOptions |= AIMARKDOWN_CMARK_OPT_CODE_METADATA }
        if options.contains(.parseWebCharacterReferences) { cmarkOptions |= AIMARKDOWN_CMARK_OPT_WEB_CHARACTER_REFERENCES }
        if options.contains(.parseReferenceUsages) { cmarkOptions |= AIMARKDOWN_CMARK_OPT_REFERENCE_USAGES }
        if options.contains(.parseReferenceDefinitions) { cmarkOptions |= AIMARKDOWN_CMARK_OPT_REFERENCE_DEFINITIONS }
        if options.contains(.parseMath) { cmarkOptions |= AIMARKDOWN_CMARK_OPT_MATH }
        if options.contains(.parseWebEmphasis) {
            cmarkOptions |= AIMARKDOWN_CMARK_OPT_WEB_EMPHASIS
        }
        if options.contains(.parseDefinitionLists) { cmarkOptions |= AIMARKDOWN_CMARK_OPT_DEFINITION_LISTS }
        if options.contains(.parseFootnotes) {
            cmarkOptions |= AIMARKDOWN_CMARK_OPT_FOOTNOTES | AIMARKDOWN_CMARK_OPT_FOOTNOTES_LOSSLESS
        }
        if !options.contains(.disableSmartOpts) {
            cmarkOptions |= AIMARKDOWN_CMARK_OPT_SMART
        }
        if !options.contains(.disableSourcePosOpts) {
            cmarkOptions |= AIMARKDOWN_CMARK_OPT_SOURCEPOS
        }
        
        let parser = aimarkdown_cmark_parser_new(cmarkOptions)
        
        if !options.contains(.disableGFM) {
            aimarkdown_cmark_parser_attach_syntax_extension(parser, aimarkdown_cmark_find_syntax_extension("table"))
            aimarkdown_cmark_parser_attach_syntax_extension(parser, aimarkdown_cmark_find_syntax_extension("strikethrough"))
            aimarkdown_cmark_parser_attach_syntax_extension(parser, aimarkdown_cmark_find_syntax_extension("tasklist"))
        }
        if options.contains(.parseWebAutolinks) {
            aimarkdown_cmark_parser_attach_syntax_extension(parser, aimarkdown_cmark_find_syntax_extension("web_autolink"))
        }
        if options.contains(.parseHighlight) {
            aimarkdown_cmark_parser_attach_syntax_extension(parser, aimarkdown_cmark_find_syntax_extension("highlight"))
        }
        aimarkdown_cmark_parser_feed(parser, string, string.utf8.count)
        let rawDocument = aimarkdown_cmark_parser_finish(parser)
        let initialState = MarkupConverterState(source: source, iterator: aimarkdown_cmark_iter_new(rawDocument), event: AIMARKDOWN_CMARK_EVENT_NONE, node: nil, options: options, headerSeen: false, pendingTableBody: nil).next()
        precondition(initialState.event == AIMARKDOWN_CMARK_EVENT_ENTER)
        precondition(initialState.nodeType == .document)
        let conversion = convertAnyElement(initialState)
        guard case .document = conversion.result.data else {
            fatalError("cmark top-level conversion didn't produce a RawMarkup.document")
        }

        let finalState = conversion.state.next()
        precondition(finalState.event == AIMARKDOWN_CMARK_EVENT_DONE)
        precondition(finalState.node == nil)
        precondition(initialState.iterator == finalState.iterator)

        precondition(initialState.node != nil)

        aimarkdown_cmark_node_free(initialState.node)
        aimarkdown_cmark_iter_free(finalState.iterator)
        aimarkdown_cmark_parser_free(parser)

        let data = _MarkupData(AbsoluteRawMarkup(markup: conversion.result,
                                                metadata: MarkupMetadata(id: .newRoot(), indexInParent: 0)))
        return makeMarkup(data) as! Document
    }
}

