// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
// AI Markdown math leaf; see AIMarkdown/provenance.json and AIMarkdown/ThirdPartyLicenses for sources and licenses.
/// A raw TeX node with no math typesetting or recursively interpreted Markdown children.
public struct MathBlock: BlockMarkup {
    public var _data: _MarkupData
    init(_ data: _MarkupData) { _data = data }
    public init(value: String, meta: String? = nil) {
        let raw = RawMarkup.mathBlock(parsedRange: nil, value: value, meta: meta)
        _data = _MarkupData(AbsoluteRawMarkup(markup: raw, metadata: MarkupMetadata(id: .newRoot(), indexInParent: 0)))
    }
    public var value: String {
        guard case let .mathBlock(value, _) = _data.raw.markup.data else { preconditionFailure("Expected a math block node") }
        return value
    }
    public var meta: String? {
        guard case let .mathBlock(_, meta) = _data.raw.markup.data else { preconditionFailure("Expected a math block node") }
        return meta
    }
    public func accept<V: MarkupVisitor>(_ visitor: inout V) -> V.Result { visitor.visitMathBlock(self) }

}
