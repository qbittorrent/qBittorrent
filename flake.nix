{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem ( system:
      let pkgs = nixpkgs.legacyPackages.${system};
          patched_qbt = nixpkgs.legacyPackages.${system}.qbittorrent-nox.overrideAttrs (o: {
            patches = (o.patches or [ ]) ++ [
              #"${self}/api-consistency.patch"
              #"${self}/api-hash-support.patch"
              #"${self}/api-debug.patch"
		"${self}/hash-1.patch"
		"${self}/hash-2.patch"
            ];
            src = nixpkgs.legacyPackages.x86_64-linux.fetchgit {
              url = "https://github.com/qbittorrent/qbittorrent";
              rev = "c3936cd4b6e7be34c1a13e4aff63dcd906d762c5";
              sha256 = "z0T/KebfFuJr9OJkRprvcQ0B5kIQi9ZAH1uStKHLIMY=";
              # Use fakeSha256 to find the new hash and update it
              #sha256 = nixpkgs.lib.fakeSha256;
            };
          });


      in {
        #packages.qbittorrent = patched_qbt;
	packages.qbittorrent-nox = patched_qbt;
        packages.default = patched_qbt;

        devShell = pkgs.mkShell { buildInputs = [ pkgs.qbittorrent-nox ]; };
      });



#      packages.x86_64-linux.default = nixpkgs.legacyPackages.x86_64-linux.qbittorrent-nox.overrideAttrs (o: {
#        patches = (o.patches or [ ]) ++ [
#          "${self}/api-consistency.patch"
#          "${self}/api-hash-support.patch"
#        ];
#        src = nixpkgs.legacyPackages.x86_64-linux.fetchgit {
#          url = "https://github.com/qbittorrent/qbittorrent";
#          rev = "c3936cd4b6e7be34c1a13e4aff63dcd906d762c5";
#          sha256 = "z0T/KebfFuJr9OJkRprvcQ0B5kIQi9ZAH1uStKHLIMY=";
#          # Use fakeSha256 to find the new hash and update it
#          #sha256 = nixpkgs.lib.fakeSha256;
#        };
#      });
  #};
}
