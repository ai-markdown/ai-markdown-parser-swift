// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
// Definition-list block node preserving Web document container semantics.
public struct DefinitionList: BlockContainer {
    public var _data: _MarkupData
    init(_ data: _MarkupData) { self._data = data }
    public init(_ children: some Sequence<BlockMarkup>) {
        let raw = RawMarkup.definitionList(parsedRange: nil, children.map { $0.raw.markup })
        self.init(_MarkupData(AbsoluteRawMarkup(markup: raw, metadata: MarkupMetadata(id: .newRoot(), indexInParent: 0))))
    }
    public func accept<V: MarkupVisitor>(_ visitor: inout V) -> V.Result {
        return visitor.visitDefinitionList(self)
    }
}
