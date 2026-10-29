// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
/// Reference syntax identity; Link/Image separately store the resolved destination.
public struct ReferenceIdentity: Equatable, Sendable {
    public enum ReferenceType: String, Sendable { case full, collapsed, shortcut }
    public let identifier: String
    public let label: String
    public let referenceType: ReferenceType
    /// The original label before identifier normalization, including escapes, entities, and physical line endings.
    public let labelSource: String
    public static func == (lhs: Self, rhs: Self) -> Bool {
        return lhs.identifier.utf16.elementsEqual(rhs.identifier.utf16) &&
            lhs.label.utf16.elementsEqual(rhs.label.utf16) &&
            lhs.labelSource.utf16.elementsEqual(rhs.labelSource.utf16) && lhs.referenceType == rhs.referenceType
    }
}
