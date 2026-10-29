// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2021 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
 See https://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

/// A footnote definition preserved in its original place in the document.
///
/// Labels are parser-provided source values, without normalization or display numbering.
/// Duplicate and unreferenced definitions are retained. Resolving references is a consumer task.
public struct FootnoteDefinition: BlockContainer {
    public var _data: _MarkupData

    init(_ data: _MarkupData) {
        self._data = data
    }

    /// Create a definition with block content. Programmatic nodes have no source range.
    public init(label: String, _ children: some Sequence<BlockMarkup>) {
        let raw = RawMarkup.footnoteDefinition(label: label, parsedRange: nil, children.map { $0.raw.markup })
        self.init(_MarkupData(AbsoluteRawMarkup(markup: raw, metadata: MarkupMetadata(id: .newRoot(), indexInParent: 0))))
    }

    /// The raw label. Mutating it follows ordinary markup value semantics and clears its source range.
    public var label: String {
        get {
            guard case let .footnoteDefinition(label) = raw.markup.data else {
                preconditionFailure("Unexpected footnote definition data")
            }
            return label
        }
        set {
            _data = _data.replacingSelf(.footnoteDefinition(label: newValue, parsedRange: nil, raw.markup.copyChildren()))
        }
    }

    public func accept<V: MarkupVisitor>(_ visitor: inout V) -> V.Result {
        return visitor.visitFootnoteDefinition(self)
    }
}
