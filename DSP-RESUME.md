# mask::klimax — cœur DSP (étape 1 / Opus)

## Résumé (15 lignes)
1. Cœur DSP C++17 header-only, sans JUCE : `Source/Dsp/Engine.h` + `Source/Dsp/Presets.h` (classe `klx::Engine`).
2. API : `prepare(sr)` hors audio (seules allocations) ; `setPresetImmediate(i)` à l'init, `setPreset(i)` en lecture (fondu 0,3 s) ; `process(L, R, n, knob 0…1, Transport{playing, hasPpq, ppq, bpm})` en place, stéréo ; `latencySamples()` ; `isIdle()`.
3. Un seul paramètre KLIMAX 0→1 (lissé 25 ms, contrôle toutes les 32 éch., gains interpolés par échantillon). 8 presets (`presetName`, `presetDescription`) : LIFT, CLASSIC (défaut, index 1), EMO SWELL, PUNK RUSH, TECHNO RISE, ROLL, VACUUM, HYPERPOP.
4. Chaîne : PITCH granulaire (jusqu'à +12 dt) → ROLL (début de chaque temps répété 1/8→1/16→1/32) → DRIVE (tanh) → CRUSH → coupe-bas 24 dB résonant qui monte / coupe-haut qui se ferme → SOUFFLE (bruit filtré qui suit l'enveloppe) → LARGEUR (graves gardés mono, mono-compatible) + ASPIRATION mono → ÉCHOS ping-pong à la croche pointée → RÉVERB FDN 8 lignes (1,2→6 s) → PUMP (1/4→1/16) → GAIN MATCH.
5. Chaque étage = une courbe (début, fin, forme) sur le potard + une valeur au climax : tout part de son état neutre en continu, sans effet on/off.
6. À 0 % (queues éteintes) : sortie = entrée retardée de la latence, bit à bit (testé).
7. Latence fixe ≈ 25 ms (pitch granulaire) à déclarer à l'hôte ; grille rythmique alignée sur l'audio retardé (ppq − latence) ; horloge libre à l'arrêt ; queue 8 s.
8. Gain match interne et invisible : pondération K, plage −12/+9 dB, gelé quand l'entrée retombe, poids nul à 0 %.
9. Temps réel sûr : aucune allocation ni verrou dans `process`, étages inactifs sautés, réverb/échos s'éteignent seuls ; résultat indépendant de la taille de bloc (testé).
10. CPU : ≈ 0,04 % au repos, 0,4–0,7 % en action (x86, 48 kHz stéréo, 1 cœur).
11. Mono→mono : traiter en stéréo avec R = copie de L, sortir (L+R)/2 ; mono→stéréo : copier L dans R avant `process`.
12. Tests : `Tests/dsp_test.cpp` (transparence, retour au repos, entrée en jeu, taille de bloc, changement de preset, gain match, transport, CPU) — tout passe, ASan/UBSan propres.

## Prompt pour Sonnet (étape 2), à transmettre tel quel avec l'archive
Voici le résumé du cœur DSP de mask::klimax (ci-dessus) et ses fichiers (`Source/Dsp/Engine.h`, `Source/Dsp/Presets.h`, `Tests/dsp_test.cpp`). Écris le reste du plugin AU + VST3 pour Logic Pro avec JUCE 8.0.15 (CMake, FetchContent) :
- Interface : UNIQUEMENT un gros potard central KLIMAX (0–100 %, glisser vertical, double-clic = 0 %, molette) et une rangée de 8 boutons de presets avec la description du preset sélectionné. Aucun vumètre, aucun autre bouton, potard ou fader : le gain match reste interne (exception voulue à mes conventions habituelles pour ce plugin).
- Charte : logo « mask:: » en blanc #f4f6fb + « klimax » en jaune #ffe04a (couleur dominante), fond #08090c, panneaux #12141a / #1a1d25, police grasse ~24 px en haut à gauche.
- Câblage : paramètre `klimax` (float 0–100 %, automatisable) passé à `process` divisé par 100 ; paramètre `preset` (choix, non automatisable, défaut CLASSIC) → `setPreset` à chaque bloc ; presets aussi exposés comme programmes de l'hôte ; `setLatencySamples(engine.latencySamples())` dans `prepareToPlay` ; queue 8 s ; transport lu depuis `getPlayHead()` ; dispositions stéréo→stéréo, mono→stéréo, mono→mono (voir point 11) ; `ScopedNoDenormals` ; état via APVTS.
- Identité : PLUGIN_NAME `mask::klimax`, PRODUCT_NAME `mask-klimax`, COMPANY_NAME `mask`, codes AU `aufx` / `Mklx` / `Mask`. Logic coupe le nom au premier « : » : remplace `::` par « ∷ » dans l'Info.plist de l'AU seulement (à la configuration CMake sur MACOSX_BUNDLE_INFO_PLIST, puis après compilation, de façon idempotente).
- Workflow GitHub Actions `macos-15` (pas macos-14) : AU + VST3 universels, signature ad hoc, installateur `mask-klimax-<version>.pkg` (bundles non relocalisables, version lue dans CMakeLists.txt), `auval -v aufx Mklx Mask` non bloquant. Je ne vois pas le dossier caché `.github` sur GitHub : fournis aussi une copie visible du workflow et explique comment la créer depuis l'interface web.
Ne modifie pas le DSP sans me le signaler.
