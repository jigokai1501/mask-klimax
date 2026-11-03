# Logic Pro lit le nom d'une Audio Unit dans l'Info.plist sous la forme « fabricant: nom » et coupe aux « : ».
# Avec « mask::klimax », il n'afficherait que « mask ».
# On remplace donc les deux « : » du nom par deux caractères qui se lisent comme des deux-points
# (par défaut U+A789 « ꞉ ») dans l'Info.plist de l'AU uniquement :
#   1. à la configuration, dans le plist généré par JUCE (MACOSX_BUNDLE_INFO_PLIST) ;
#   2. après la compilation, dans le bundle (idempotent, par sécurité).
# Le VST3, getName() et le logo gardent « mask::klimax » avec de vrais deux-points.
#
# Si le caractère s'affichait mal dans Logic : -DKLX_AU_COLON=∶  (U+2236) ou  -DKLX_AU_COLON=ː  (U+02D0).

set(KLX_AU_COLON "꞉" CACHE STRING "Caractère qui remplace chaque « : » dans le nom de l'Audio Unit")
set(KLX_PATCH_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/patch_plist.cmake")

function(klx_patch_au_name target)
    if(NOT TARGET ${target})
        return()
    endif()
    get_target_property(plist ${target} MACOSX_BUNDLE_INFO_PLIST)
    if(plist AND EXISTS "${plist}")
        execute_process(COMMAND ${CMAKE_COMMAND} -DPLIST=${plist} "-DKLX_AU_COLON=${KLX_AU_COLON}" -P ${KLX_PATCH_SCRIPT})
        file(READ "${plist}" content)
        string(REGEX MATCH "<key>name</key>[ \t\r\n]*<string>[^<]*</string>" found "${content}")
        string(REGEX REPLACE ".*<string>([^<]*)</string>" "\\1" found "${found}")
        message(STATUS "nom de l'Audio Unit (Info.plist) : ${found}")
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} "-DPLIST=$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Info.plist"
                "-DKLX_AU_COLON=${KLX_AU_COLON}" -P ${KLX_PATCH_SCRIPT}
        VERBATIM)
endfunction()
