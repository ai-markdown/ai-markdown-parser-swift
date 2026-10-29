// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2021 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
 See https://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

/// An unordered list.
public struct UnorderedList: ListItemContainer {
    public var _data: _MarkupData
    init(_ raw: RawMarkup) throws {
        guard case .unorderedList = raw.data else {
            throw RawMarkup.Error.concreteConversionError(from: raw, to: UnorderedList.self)
        }
        let absoluteRaw = AbsoluteRawMarkup(markup: raw, metadata: MarkupMetadata(id: .newRoot(), indexInParent: 0))
        self.init(_MarkupData(absoluteRaw))
    }

    init(_ data: _MarkupData) {
        self._data = data
    }
}

// MARK: - Public API

public extension UnorderedList {
    // MARK: ListItemContainer

    init<Items: Sequence>(_ items: Items) where Items.Element == ListItem {
        try! self.init(.unorderedList(parsedRange: nil, items.map { $0.raw.markup }))
    }

    /// Web mdast `spread` derived from block-parser events when `parseListSpreadMetadata` is enabled.
    ///
    /// Nil when capture is disabled or unsupported, or when the node is constructed or rebuilt programmatically;
    /// it is parse evidence and is not recomputed after child edits.
    var parsedSpread: Bool? {
        guard case let .unorderedList(spread) = _data.raw.markup.data else { return nil }
        return spread
    }

    // MARK: Visitation

    func accept<V: MarkupVisitor>(_ visitor: inout V) -> V.Result {
        return visitor.visitUnorderedList(self)
    }
}
