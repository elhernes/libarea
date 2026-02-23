// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "LibArea",
    products: [
        .library(name: "LibArea", targets: ["LibArea"]),
    ],
    targets: [
        // C++ library: all libarea sources in src/ plus the C shim.
        // Only src/include/area_c.h is exposed as a public header (Swift-visible).
        .target(
            name: "CLibArea",
            path: "src",
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("."),
            ],
            cxxSettings: [
                .headerSearchPath("."),
            ]
        ),

        // Swift wrapper over the C shim.
        .target(
            name: "LibArea",
            dependencies: ["CLibArea"]
        ),

        .testTarget(
            name: "LibAreaTests",
            dependencies: ["LibArea"]
        ),
    ],
    cxxLanguageStandard: .cxx17
)
