// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
// AI Markdown link reference definition leaf; preserve duplicate and unused definitions in source order.
/// Raw parser metadata producing no visible text; the parser independently handles reference resolution and duplicate selection.
public struct LinkReferenceDefinition: BlockMarkup {
    public var _data: _MarkupData
    init(_ data: _MarkupData) { _data = data }
    public init(label: String, identifier: String, destination: String, title: String? = nil) {
        let raw = RawMarkup.linkReferenceDefinition(parsedRange: nil, label: label, identifier: identifier, destination: destination, title: title, labelSource: nil)
        _data = _MarkupData(AbsoluteRawMarkup(markup: raw, metadata: MarkupMetadata(id: .newRoot(), indexInParent: 0)))
    }
    public var label: String {
        guard case let .linkReferenceDefinition(label, _, _, _, _) = _data.raw.markup.data else { preconditionFailure("Expected a link reference definition node") }
        return label
    }
    public var identifier: String {
        guard case let .linkReferenceDefinition(_, identifier, _, _, _) = _data.raw.markup.data else { preconditionFailure("Expected a link reference definition node") }
        return identifier
    }
    public var destination: String {
        guard case let .linkReferenceDefinition(_, _, destination, _, _) = _data.raw.markup.data else { preconditionFailure("Expected a link reference definition node") }
        return destination
    }
    public var title: String? {
        guard case let .linkReferenceDefinition(_, _, _, title, _) = _data.raw.markup.data else { preconditionFailure("Expected a link reference definition node") }
        return title
    }
    /// Original escape and entity spellings in the label; programmatically created nodes have no parsed source value.
    public var labelSource: String? {
        guard case let .linkReferenceDefinition(_, _, _, _, source) = _data.raw.markup.data else { preconditionFailure("Expected a link reference definition node") }
        return source
    }
    public func accept<V: MarkupVisitor>(_ visitor: inout V) -> V.Result { visitor.visitLinkReferenceDefinition(self) }
}
