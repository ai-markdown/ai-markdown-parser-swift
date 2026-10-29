// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
// AI Markdown math leaf; see AIMarkdown/provenance.json and AIMarkdown/ThirdPartyLicenses for sources and licenses.
/// A raw TeX node with no math typesetting or recursively interpreted Markdown children.
public struct InlineMath: RecurringInlineMarkup {
    public var _data: _MarkupData
    init(_ data: _MarkupData) { _data = data }
    public init(value: String) {
        let raw = RawMarkup.inlineMath(parsedRange: nil, value: value)
        _data = _MarkupData(AbsoluteRawMarkup(markup: raw, metadata: MarkupMetadata(id: .newRoot(), indexInParent: 0)))
    }
    public var value: String {
        guard case let .inlineMath(value) = _data.raw.markup.data else { preconditionFailure("Expected an inline math node") }
        return value
    }

    public func accept<V: MarkupVisitor>(_ visitor: inout V) -> V.Result { visitor.visitInlineMath(self) }
    public var plainText: String { value }
}
