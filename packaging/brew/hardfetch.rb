class Hardfetch < Formula
  desc "Compact system info tool, neofetch style, in C"
  homepage "https://github.com/00vez/hardfetch"
  url "https://github.com/00vez/hardfetch/releases/download/3.3/hardfetch"
  sha256 "64bb530cd1450a34aa0c532a4e9c283d01006032536ef7e7c57d098f374460dc"
  license "MIT"
  version "3.3"

  # Only the Linux build exists. macOS is still on the 0.2.4 collector set,
  # so there is nothing to install here yet.
  depends_on arch: :linux

  def install
    bin.install "hardfetch"
  end

  def caveats
    return unless OS.mac?
    <<~EOS
      hardfetch is published for Linux and Windows only at the moment.
      The macOS build still uses the 0.2.4 collector set and is not
      packaged. See https://github.com/00vez/hardfetch/releases
    EOS
  end

  test do
    assert_match "hardfetch v#{version}", shell_output("#{bin}/hardfetch --version")
  end
end
