# Corresponding Source

The distributed `FavoritesToggle.dll` statically incorporates code from these
exact revisions:

- [QTR CommonLibSF](https://github.com/QTR-Modding/commonlibsf) commit
  `765219c66f58f729f854771a95cdd59bbd76b84b`
- [commonlib-shared](https://github.com/libxse/commonlib-shared) commit
  `5470284e964d5510aa001dca3e0bb5548b6356a4`
- [spdlog](https://github.com/gabime/spdlog) v1.16.0, commit
  `486b55554f11c9cccc913e11a87085b2a91f706f`

CommonLibSF and its nested commonlib-shared dependency are build submodules.
Xmake resolves spdlog v1.16.0 for the build; `external/spdlog` pins a matching
source snapshot for corresponding-source delivery.

A version-matched `FavoritesToggle-SFSE-v0.1.0-source.zip` is attached alongside
every distributed binary, including on Nexus. It contains the complete project
tree corresponding to the release commit and expanded CommonLibSF,
commonlib-shared, and matching spdlog source trees. GitHub's automatic source
archives are not a substitute because they do not expand submodules.

Build instructions are in [README.md](README.md). Complete license and exception
texts are in `COPYING`, `EXCEPTIONS`, and `LICENSES/`.
