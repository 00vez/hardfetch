class Hardfetch < Formula
  desc "Compact system info tool, neofetch style, in C"
  homepage "https://github.com/00vez/hardfetch"
  url "https://github.com/00vez/hardfetch/releases/download/3.5/hardfetch"
  sha256 "3ba7bc2d1742a628b8a3b8d43847e6c4a437f316f136fb0d22d65f00130e2cc1"
  license "MIT"
  version "3.5"

  # The Linux build. macOS has its own binaries in the same release, see
  # hardfetch-macos-arm64 and hardfetch-macos-x64.
  depends_on arch: :linux

  def install
    bin.install "hardfetch"
  end

  test do
    assert_match "hardfetch v#{version}", shell_output("#{bin}/hardfetch --version")
  end
end
