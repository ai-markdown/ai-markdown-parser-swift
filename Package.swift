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
        .target(name: "AIMarkdownParser", dependencies: ["AIMarkdownCMark", "AIMarkdownCMarkExtensions", "AIMarkdownCAtomic"], path: "Vendor/Parser/Markdown"),
        .target(name: "AIMarkdownCMark", path: "Vendor/Parser/CMark"),
        .target(name: "AIMarkdownCMarkExtensions", dependencies: ["AIMarkdownCMark"], path: "Vendor/Parser/CMarkExtensions"),
        .target(name: "AIMarkdownCAtomic", path: "Vendor/Parser/CAtomic"),
        .testTarget(name: "UpstreamMarkdownTests", dependencies: ["AIMarkdownParser"], resources: [.process("Visitors/Everything.md")]),
        .testTarget(name: "ParserBoundaryTests", dependencies: ["AIMarkdownParser", "AIMarkdownCMark"]),
    ],
    swiftLanguageModes: [.v5]
)
