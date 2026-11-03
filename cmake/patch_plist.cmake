# cmake -DPLIST=<chemin> [-DKLX_AU_COLON=<caractère>] -P patch_plist.cmake
# Remplace « mask:: » par « mask<c><c> » dans l'Info.plist de l'AU, où <c> est un caractère qui SE LIT comme
# un deux-points mais n'en est pas un pour Logic (par défaut U+A789 « ꞉ », « MODIFIER LETTER COLON »).
# Idempotent. Remplace aussi l'ancien « mask∷ » (U+2237) des versions précédentes.
if(NOT DEFINED KLX_AU_COLON OR KLX_AU_COLON STREQUAL "")
    set(KLX_AU_COLON "꞉")
endif()

if(EXISTS "${PLIST}")
    file(READ "${PLIST}" before)
    set(after "${before}")
    string(REPLACE "mask::" "mask${KLX_AU_COLON}${KLX_AU_COLON}" after "${after}")
    string(REPLACE "mask∷"  "mask${KLX_AU_COLON}${KLX_AU_COLON}" after "${after}")
    if(NOT after STREQUAL before)
        file(WRITE "${PLIST}" "${after}")
    endif()
endif()
