set_xmakever("3.0.9")
set_policy("package.requires_lock", true)
set_config("commonlib_ini", true)

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

local commonlibsf = path.join(os.projectdir(), "lib", "commonlibsf")
includes(commonlibsf)

local plugin_name = "FavoritesToggle"
local plugin_version = "1.0.0"
local plugin_author = "Quantumyilmaz"

set_project(plugin_name)
set_version(plugin_version)
set_license("GPL-3.0-or-later")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg", "mode.release")
add_rules("plugin.vsxmake.autoupdate")

target(plugin_name)
    set_pcxxheader("src/PCH.h")
    add_defines("_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING")
    add_rules("commonlibsf.plugin", {
        name = plugin_name,
        author = plugin_author,
        options = {
            sig_scanning = false,
            address_library = true,
            no_struct_use = false,
            layout_dependent = true
        }
    })

    add_files(
        "src/plugin.cpp",
        "src/Settings.cpp",
        "src/Vanilla.cpp",
        "src/FME.cpp"
    )
    add_headerfiles("src/PCH.h", "src/Hooks.h")
    add_includedirs("src")
    add_installfiles("config/FavoritesToggle.ini", {
        prefixdir = "SFSE/Plugins"
    })
    set_installdir("build/install/Data")
