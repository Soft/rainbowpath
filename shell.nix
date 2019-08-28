{ pkgs ? import <nixpkgs> {} }:

with pkgs;

mkShell {
  buildInputs = [ pkgconfig automake autoconf gcc ncurses ];
}
