# ══════════════════════════════════════════════════════════════════════════════
# Conan recipe for QtNetworkRequest (QtMultiThreadNetwork)
#
# Supports:    Qt 5.9 ~ Qt 6.x, Visual Studio 2017 / 2019 / 2022
# Binary ABI changes per Qt version — package_id encodes the Qt version used.
# ══════════════════════════════════════════════════════════════════════════════
from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy, rmdir
import os


class QtNetworkRequestConan(ConanFile):
    name = "qtnetworkrequest"
    version = "2.2.1"
    license = "MIT"
    author = "Your Name <your.email@example.com>"
    url = "https://github.com/your-org/qt-network-request"
    description = "Multi-threaded HTTP(S)/FTP request library based on Qt5/6 Network"
    topics = ("qt", "qt5", "qt6", "network", "http", "https", "ftp", "download", "upload")

    # ── Build settings ─────────────────────────────────────────────────────────
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC":   [True, False],
        # Qt major version: 5 or 6
        "qt":     [5, 6],
    }
    default_options = {
        "shared": True,
        "fPIC":   True,
        "qt":     5,
    }

    exports_sources = [
        "CMakeLists.txt",
        "cmake/*",
        "source/*",
        "include/*",
        "LICENSE",
    ]

    # ── Supported compilers map ────────────────────────────────────────────────
    # Minimum MSVC/Clang/GCC versions for C++17 + Qt compatibility
    _minimum_compiler = {
        "msvc":       "191",   # VS 2017 (MSVC 15.0)
        "gcc":        "8",
        "clang":      "7",
        "apple-clang": "10",
    }

    # ── Qt-to-OpenSSL mapping for minimum version bumps ────────────────────────
    # Qt 5.15+ requires OpenSSL 1.1.1; older Qt can use 1.0.2 or 1.1.1
    _openssl_min = {
        5: "1.1.1",
        6: "1.1.1",
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        self.settings.compiler.cppstd = 17

    def validate(self):
        # ── Check minimum compiler ─────────────────────────────────────────────
        compiler = str(self.settings.compiler)
        if compiler in self._minimum_compiler:
            min_ver = self._minimum_compiler[compiler]
            if self.settings.compiler.version < min_ver:
                raise ConanInvalidConfiguration(
                    f"{self.name}/{self.version} requires {compiler} >= {min_ver}"
                )

        # ── Qt 6 requires VS 2019+ on Windows ──────────────────────────────────
        if self.options.qt == 6 and self.settings.os == "Windows":
            if compiler == "msvc" and self.settings.compiler.version < "192":
                raise ConanInvalidConfiguration(
                    f"Qt 6 requires Visual Studio 2019 (MSVC 192) or later"
                )

    def requirements(self):
        qt_ver = str(self.options.qt)

        if self.options.qt == 5:
            self.requires("qt/5.15.8")
            # Qt 5.15.8 bundles Core + Network in base package
            self.requires("openssl/1.1.1w")
        else:
            self.requires("qt/6.5.3")
            # Qt 6: modules are separate packages
            self.requires("openssl/1.1.1w")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_TESTS"]   = "OFF"
        tc.cache_variables["BUILD_SAMPLES"] = "OFF"

        # MSVC runtime: /MD or /MT — align with the Qt build type
        # Qt prebuilt binaries always use /MD (dynamic CRT)
        if self.settings.os == "Windows" and self.settings.compiler == "msvc":
            # Let Conan's runtime setting drive the CRT linkage
            # (default: dynamic=True → /MD, static=False → /MT)
            pass

        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        # Keep cmake config files for find_package support
        copy(self, "LICENSE",
             dst=os.path.join(self.package_folder, "licenses"),
             src=self.source_folder)

    def package_id(self):
        # ── Qt version affects binary ABI — encode it in package id ────────────
        # All Qt version #if branches are centralized in source/qtcompat.h:
        #   5.9 → QThreadPool::tryTake, 5.12 → TLS 1.3, 5.13 → Http2AllowedAttribute,
        #   5.14 → QRecursiveMutex / Qt::SkipEmptyParts, 5.15 → setTransferTimeout / errorOccurred
        del self.info.options.qt
        self.info.requires["qt"].full_version_mode()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "QtNetworkRequest")
        self.cpp_info.set_property("cmake_target_name",
                                   "QtNetworkRequest::QNetworkRequest")

        self.cpp_info.libs = ["QNetworkRequest"]

        if self.settings.os == "Windows" and not self.options.shared:
            self.cpp_info.defines = ["QT_MTNETWORK_STATIC"]

        # Consumers must also link Qt Core + Network
        if self.options.qt == 5:
            self.cpp_info.requires = ["qt::qtCore", "qt::qtNetwork"]
        else:
            self.cpp_info.requires = [
                "qt::qtCore", "qt::qtNetwork", "openssl::openssl"
            ]
