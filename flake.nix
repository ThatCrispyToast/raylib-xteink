{
  description = "raylib for the Xteink X4: native emulator and ESP32-C3 firmware";
  inputs.esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
  inputs.nixpkgs.follows = "esp-dev/nixpkgs";
  outputs = { self, nixpkgs, esp-dev }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; overlays = [ esp-dev.overlays.default ]; };
  in {
    devShells.${system} = { default = pkgs.mkShell {
      packages = with pkgs; [ cmake ninja pkg-config gcc SDL2 python3 git xorg.xorgserver xorg.xauth ];
    };
    firmware = pkgs.mkShell {
      inputsFrom = [ esp-dev.devShells.${system}.esp32c3-idf ];
      packages = [ pkgs.python3 ];
    };
    qemu = pkgs.mkShell {
      packages = [ (pkgs.qemu-esp32.overrideAttrs (old: {
        configureFlags = (builtins.filter (flag: !(pkgs.lib.hasPrefix "--target-list=" flag)) old.configureFlags)
          ++ [ "--target-list=riscv32-softmmu" ];
        postInstall = "";
      })) ];
    }; };
  };
}
