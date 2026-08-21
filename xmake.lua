set_xmakever("3.0.8")

if is_plat("windows") then
    local project_root = os.projectdir()
    add_cxflags(
        "/Brepro",
        "/experimental:deterministic",
        '/d1trimfile:"' .. project_root .. '"',
        '/pathmap:"' .. project_root .. '"=.',
        {
            tools = "cl",
            force = true
        })
    add_shflags("/Brepro", "/PDBALTPATH:%_PDB%", {
        force = true
    })
end

includes("lib/commonlibsf")

set_project("FavoritesToggle")
set_version("0.1.0")
set_license("GPL-3.0-or-later")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg", "mode.release")
add_rules("plugin.vsxmake.autoupdate")

target("FavoritesToggle")
    add_defines("_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING")
    add_rules("commonlibsf.plugin", {
        name = "FavoritesToggle",
        author = "Quantumyilmaz",
        description = "Toggles equipped Starfield favorites from keyboard, mouse, or gamepad",
        options = {
            address_library = true,
            no_struct_use = false,
            layout_dependent = true
        }
    })

    add_files("src/plugin.cpp", "src/Vanilla.cpp")
    add_headerfiles("include/PCH.h", "include/Hooks.h")
    add_includedirs("include")
    set_pcxxheader("include/PCH.h")
    set_installdir("build/install/Data")
