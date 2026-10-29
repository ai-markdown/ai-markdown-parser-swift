// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2021 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
 See https://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

/// An inline image reference.
public struct Image: InlineMarkup, InlineContainer {
    public var _data: _MarkupData

    init(_ data: _MarkupData) {
        self._data = data
    }

    init(_ raw: RawMarkup) throws {
        guard case .image = raw.data else {
            throw RawMarkup.Error.concreteConversionError(from: raw, to: Image.self)
        }
        let absoluteRaw = AbsoluteRawMarkup(markup: raw, metadata: MarkupMetadata(id: .newRoot(), indexInParent: 0))
        self.init(_MarkupData(absoluteRaw))
    }
}

// MARK: - Public API

public extension Image {
    /// Create an image from a source and zero or more child inline elements.
    init<Children: Sequence>(source: String? = nil, title: String? = nil, _ children: Children) where Children.Element == RecurringInlineMarkup {
        let titleToUse: String?
        if let t = title, t.isEmpty {
            titleToUse = nil
        } else {
            titleToUse = title
        }

        let sourceToUse: String?
        if let s = source, s.isEmpty {
            sourceToUse = nil
        } else {
            sourceToUse = source
        }

        try! self.init(.image(source: sourceToUse, title: titleToUse, parsedRange: nil, children.map { $0.raw.markup }))
    }

    /// Create an image from a source and zero or more child inline elements.
    init(source: String? = nil, title: String? = nil, _ children: RecurringInlineMarkup...) {
        self.init(source: source, title: title, children)
    }

    /// The image's source.
    var source: String? {
      get {
        guard case let .image(source, _, _) = _data.raw.markup.data else {
            fatalError("\(self) markup wrapped unexpected \(_data.raw)")
        }
        return source
      }
      set {
        guard newValue != source || referenceIdentity != nil else {
            return
        }
        if let s = newValue, s.isEmpty {
            _data = _data.replacingSelf(.image(source: nil, title: title, parsedRange: nil, _data.raw.markup.copyChildren()))
        } else {
            _data = _data.replacingSelf(.image(source: newValue, title: title, parsedRange: nil, _data.raw.markup.copyChildren()))
        }
      }
    }

    /// The image's title.
    var title: String? {
      get {
        guard case let .image(_, title, _) = _data.raw.markup.data else {
            fatalError("\(self) markup wrapped unexpected \(_data.raw)")
        }
        return title
      }
      set {
        guard newValue != title || referenceIdentity != nil else {
            return
        }

        if let t = newValue, t.isEmpty {
                _data = _data.replacingSelf(.image(source: source, title: nil, parsedRange: nil, _data.raw.markup.copyChildren()))
        } else {
            _data = _data.replacingSelf(.image(source: source, title: newValue, parsedRange: nil, _data.raw.markup.copyChildren()))
        }
      }
    }

    /// Only parsed reference syntax has an identity; direct links and programmatically created nodes have nil.
    /// Setting the destination or title clears the identity; changing children/sourceOrigin preserves it.
    var referenceIdentity: ReferenceIdentity? {
        guard case let .image(_, _, identity) = _data.raw.markup.data else { preconditionFailure("Expected a reference container node") }
        return identity
    }

    // MARK: Visitation

    func accept<V: MarkupVisitor>(_ visitor: inout V) -> V.Result {
        return visitor.visitImage(self)
    }
}
