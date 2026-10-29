import Markdown
import AIMarkdownParser
let official = Markdown.Document(parsing: "**official**")
let custom = AIMarkdownParser.Document(parsing: "a\r\nb", options: [.parseSoftBreakMetadata])
precondition(official.childCount == 1 && custom.childCount == 1)
let paragraph = Array(custom.children).first!
let softBreak = paragraph.children.compactMap { $0 as? AIMarkdownParser.SoftBreak }.first!
precondition(softBreak.physicalLineEnding == "\r\n")
precondition(AIMarkdownParser.FootnoteLabelMetadata(rawSource: "A&amp;B")?.identifier == "a&amp;b")
print("Official Markdown and AIMarkdownParser coexistence passed with real Git dependencies")
