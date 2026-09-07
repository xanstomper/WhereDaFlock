// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "WhereDaFlock",
    defaultLocalization: "en",
    platforms: [
        .iOS(.v17),
        .watchOS(.v10),
        .macOS(.v14)
    ],
    products: [
        .library(
            name: "WhereDaFlock",
            targets: ["WhereDaFlock"]
        ),
    ],
    dependencies: [
        // Uncomment when using Google Maps SDK via Swift Package Manager
        // .package(url: "https://github.com/googlemaps/ios-maps-sdk", from: "9.0.0"),
        // .package(url: "https://github.com/googlemaps/ios-directions-sdk", from: "1.0.0"),
        // .package(url: "https://github.com/MapLibre/maplibre-gl-native-distribution", from: "6.0.0"),
    ],
    targets: [
        .target(
            name: "WhereDaFlock",
            dependencies: [],
            resources: [
                .process("Resources"),
                .process("AI/Models")
            ]
        ),
        .testTarget(
            name: "WhereDaFlockTests",
            dependencies: ["WhereDaFlock"]
        ),
    ]
)
