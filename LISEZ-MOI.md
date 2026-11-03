# mask::klimax — du dépôt GitHub à Logic Pro

Un seul potard (0 → 100 %) + 8 presets. À 0 % : aucun effet (sortie identique à l'entrée). En montant : le build-up se construit tout seul.

## 1. Mettre les fichiers sur GitHub
1. Crée un dépôt (ex. `mask-klimax`) et envoie-y **tout le contenu** du dossier de l'archive (`CMakeLists.txt`, `Source/`, `cmake/`, `Tests/`…), pas le dossier lui-même.
2. Le dossier caché `.github` ne s'affiche pas dans le Finder, donc il ne part pas avec un glisser-déposer. **Crée-le à la main** (étape 2).

## 2. Créer le workflow (copie visible : `ci/build.yml`)
1. Sur la page du dépôt : **Add file ▸ Create new file**.
2. Dans le champ du nom, tape exactement `.github/workflows/build.yml` (les `/` créent les dossiers tout seuls).
3. Ouvre `ci/build.yml` (visible dans l'archive), copie tout son contenu, colle-le dans l'éditeur GitHub.
4. **Commit changes**. Le workflow se lance tout seul (onglet **Actions**).

## 3. Récupérer l'installateur
1. Onglet **Actions** ▸ dernier run ▸ en bas, **Artifacts** ▸ `mask-klimax-installateur` (zip).
2. Dézippe : tu obtiens `mask-klimax-1.0.1.pkg`. Clic droit ▸ **Ouvrir** (signature ad hoc, macOS peut demander confirmation dans Réglages ▸ Confidentialité et sécurité).
3. Installe : l'AU va dans `/Library/Audio/Plug-Ins/Components`, le VST3 dans `/Library/Audio/Plug-Ins/VST3`.

## 4. Dans Logic Pro
- Cherche **mask::klimax** : il s'affiche ainsi dans la liste et dans l'emplacement d'insert. En interne, les deux « : » du nom de l'Audio Unit sont deux caractères qui se lisent comme des deux-points (U+A789 « ꞉ »), parce que Logic coupe le nom aux vrais « : » et n'afficherait que « mask ». Le mot « mask » seul, devant, est le dossier « fabricant » du menu de plug-ins de Logic.
- Si l'ancien nom reste affiché : *Logic Pro ▸ Réglages ▸ Gestionnaire de plug-ins ▸ Réinitialisation complète des Audio Units*, puis relance Logic (Logic garde les noms en cache ; le numéro de version a été augmenté pour l'aider).
- Si les deux points de substitution s'affichent mal (carré, trou) : dans le workflow `build.yml`, ajoute `-DKLX_AU_COLON=∶` à la ligne « Configuration » (variante U+2236), ou, en dernier recours, double-clic sur la colonne **Custom Name** du plugin dans le Gestionnaire et saisis `mask::klimax` (de vrais deux-points).
- Les 8 presets sont aussi dans le menu de presets de Logic. Latence ≈ 25 ms, compensée automatiquement par Logic.
- Si le build échoue ou si `auval` signale quelque chose, envoie-moi le journal du run (Actions ▸ run ▸ étape en rouge).

## Mettre à jour plus tard
Change seulement le numéro dans `CMakeLists.txt` (`project(MASK_KLIMAX VERSION x.y.z …)`) : le workflow le lit lui-même, aucune modification du dossier caché n'est nécessaire.

## Contenu
- `Source/Dsp/` : cœur DSP (Opus) — **ne pas modifier sans nous le signaler**.
- `Source/PluginProcessor.*`, `Source/PluginEditor.*` : câblage, interface (potard + presets).
- `cmake/` : correctif du nom de l'AU (deux-points de substitution). `Tests/` : tests DSP et plugin (facultatifs).
