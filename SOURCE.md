# Corresponding Source

The distributed `FavoritesToggle.dll` statically incorporates code from these
exact revisions:

- [CommonLibSF](https://github.com/libxse/commonlibsf) commit
  `84f8589b15a6d588d28da16baa7459a1fc414ea7`
- [commonlib-shared](https://github.com/libxse/commonlib-shared) commit
  `40bdbcaf8fa691ee6daadc7b5cea8d43f794bef4`
- [spdlog](https://github.com/gabime/spdlog) v1.16.0, commit
  `486b55554f11c9cccc913e11a87085b2a91f706f`
- [SimpleIni](https://github.com/brofield/simpleini) v4.25, commit
  `18fd1db7fcfee664c174719f062b2598772c8be6`

CommonLibSF and its nested commonlib-shared dependency are build submodules.
Xmake resolves spdlog v1.16.0 and SimpleIni v4.25 for the build;
`external/spdlog` and `external/simpleini` pin matching source snapshots for
corresponding-source delivery.

A version-matched `FavoritesToggle-SFSE-v0.1.0-source.zip` is attached alongside
every distributed binary, including on Nexus. It contains the complete project
tree corresponding to the release commit and expanded CommonLibSF,
commonlib-shared, spdlog, and SimpleIni source trees. GitHub's automatic source
archives are not a substitute because they do not expand submodules.

Build instructions are in [README.md](README.md). Complete license and exception
texts are in `COPYING`, `EXCEPTIONS`, and `LICENSES/`.
