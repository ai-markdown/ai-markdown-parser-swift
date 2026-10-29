// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
import AIMarkdownCMark

/// Raw Web code fields produced only by actual parser tokens, not reconstructed from existing display literals.
public struct CodeSyntaxMetadata: Sendable, Equatable {
    public enum Kind: Int, Sendable { case fenced = 1, indented = 2, inline = 3 }
    public let kind: Kind
    public let value: String
    public let language: String?
    public let meta: String?
    /// The raw opening-fence info string, retaining ASCII whitespace before decoding; nil for other kinds.
    public let rawInfo: String?
    /// Actual parser capture after removing container/code indentation; neither the complete original source nor an exact inverse source-position mapping.
    /// Fenced content retains its final EOL; indented content may include trailing blank lines to exclude; inline content includes padding.
    public let sourceCapture: String

    init?(node: UnsafeMutablePointer<aimarkdown_cmark_node>?) {
        guard let node else { return nil }
        guard let kind = Kind(rawValue: Int(aimarkdown_cmark_node_get_code_metadata_kind(node))),
              let value = aimarkdown_cmark_node_get_code_metadata_value(node),
              let source = aimarkdown_cmark_node_get_code_metadata_source(node) else { return nil }
        self.kind = kind
        self.value = String(cString: value)
        sourceCapture = String(cString: source)
        language = aimarkdown_cmark_node_get_code_metadata_lang(node).map { String(cString: $0) }
        meta = aimarkdown_cmark_node_get_code_metadata_meta(node).map { String(cString: $0) }
        rawInfo = aimarkdown_cmark_node_get_code_metadata_info(node).map { String(cString: $0) }
    }

    public static func == (lhs: Self, rhs: Self) -> Bool {
        func same(_ left: String?, _ right: String?) -> Bool {
            switch (left, right) {
            case let (left?, right?): return left.utf16.elementsEqual(right.utf16)
            case (nil, nil): return true
            default: return false
            }
        }
        return lhs.kind == rhs.kind && same(lhs.value, rhs.value) && same(lhs.language, rhs.language) &&
            same(lhs.meta, rhs.meta) && same(lhs.rawInfo, rhs.rawInfo) && same(lhs.sourceCapture, rhs.sourceCapture)
    }
}
