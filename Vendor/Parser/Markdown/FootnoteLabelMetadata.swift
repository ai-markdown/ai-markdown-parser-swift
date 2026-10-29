// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
import AIMarkdownCMark

/// Derive raw MDAST fields from actual footnote source labels; compute the identifier without first decoding the label.
public struct FootnoteLabelMetadata: Sendable {
    public let identifier: String
    public let label: String

    /// Parser source labels contain no NUL. Explicitly reject programmatic input outside the C label domain rather than truncating it.
    public init?(rawSource: String) {
        guard !rawSource.utf8.contains(0), rawSource.utf8.count <= Int32.max else { return nil }
        var count = 0
        guard let decoded = rawSource.withCString({ aimarkdown_cmark_decode_reference_label($0, rawSource.utf8.count, &count) }) else { return nil }
        defer { aimarkdown_cmark_decoded_reference_label_free(decoded) }
        label = String(decoding: UnsafeBufferPointer(start: decoded, count: count), as: UTF8.self)
        if let normalized = aimarkdown_cmark_reference_identifier(rawSource) {
            defer { aimarkdown_cmark_reference_identifier_free(normalized) }
            identifier = String(cString: normalized)
        } else { identifier = "" }
    }
}
