// swift-tools-version: 6.2
import PackageDescription

let package = Package(
    name: "AIMarkdownParser",
    platforms: [.iOS(.v26), .macOS(.v26)],
    products: [
        .library(name: "AIMarkdownParser", targets: ["AIMarkdownParser"]),
        .library(name: "AIMarkdownCMark", targets: ["AIMarkdownCMark"]),
        .library(name: "AIMarkdownCMarkExtensions", targets: ["AIMarkdownCMarkExtensions"]),
    ],
    targets: [
        .target(name: "AIMarkdownParser", dependencies: ["AIMarkdownCMark", "AIMarkdownCMarkExtensions", "AIMarkdownCAtomic"]),
        .target(name: "AIMarkdownCMark"),
        .target(name: "AIMarkdownCMarkExtensions", dependencies: ["AIMarkdownCMark"]),
        .target(name: "AIMarkdownCAtomic"),
        .testTarget(name: "UpstreamMarkdownTests", dependencies: ["AIMarkdownParser"], resources: [.process("Visitors/Everything.md")]),
        .testTarget(name: "ParserBoundaryTests", dependencies: ["AIMarkdownParser", "AIMarkdownCMark"]),
    ],
    swiftLanguageModes: [.v5]
)
