cask "quickqc" do
  version :latest
  sha256 :no_check

  arch arm: "macos-arm64", intel: "macos-amd64"

  url "https://github.com/MuyleangIng/quickqc/releases/latest/download/quickqc-#{arch}.tar.gz"
  name "QuickQC"
  desc "Fast offline clipboard manager for text and images"
  homepage "https://github.com/MuyleangIng/quickqc"

  app "quickqc.app"
end
