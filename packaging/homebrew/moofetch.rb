# Template for a Homebrew formula.
#
# TODO before submitting:
#   1. Create a tagged release (vX.Y.Z) on the real repository and update `url`.
#   2. Replace the `sha256` placeholder: `brew fetch --build-from-source moofetch` prints it.
#   3. Update `homepage`.
class Moofetch < Formula
  desc "System information tool with animated ASCII logos (fastfetch fork)"
  homepage "https://github.com/moofetch/moofetch"
  url "https://github.com/moofetch/moofetch/archive/refs/tags/v0.1.0.tar.gz"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"
  license "MIT"
  head "https://github.com/moofetch/moofetch.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "python@3.12" => :build

  def install
    system "cmake", "-B", "build", "-DCMAKE_BUILD_TYPE=Release", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_match "moofetch", shell_output("#{bin}/moofetch --version")
    assert_match "arch", shell_output("#{bin}/moofetch --list-animations")
  end
end
