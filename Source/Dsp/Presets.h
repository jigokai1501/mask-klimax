#pragma once
// mask::klimax — presets.
// Chaque preset décrit, pour chaque étage, une courbe d'entrée en jeu sur le potard KLIMAX
// (début, fin, forme) et la valeur atteinte au climax (100 %).
// Toutes les valeurs sont des float pour pouvoir fondre un preset dans un autre (changement
// de preset en cours de lecture sans à-coup).

#include <cmath>
#include <algorithm>

namespace klx
{

struct Curve
{
    float start = 0.0f, end = 1.0f, shape = 1.5f;

    // 0 sous « start », 1 à partir de « end », puissance « shape » entre les deux
    // (shape > 1 : pente nulle au départ → entrée en jeu sans à-coup).
    float operator() (float k) const noexcept
    {
        if (k <= start) return 0.0f;
        const float u = (k - start) / std::max (1.0e-4f, end - start);
        if (u >= 1.0f) return 1.0f;
        return std::pow (u, shape);
    }
};

inline Curve lerp (const Curve& a, const Curve& b, float t) noexcept
{
    return { a.start + (b.start - a.start) * t, a.end + (b.end - a.end) * t, a.shape + (b.shape - a.shape) * t };
}

struct Preset
{
    // PITCH : montée de hauteur granulaire (demi-tons au climax)
    Curve pitch;  float pitchSemi = 0.0f;
    // ROLL : répétition calée sur la grille (1/8 → 1/16 → 1/32). rollFinest : 3 = 1/8, 4 = 1/16, 5 = 1/32
    Curve roll;   float rollMix = 0.0f, rollFinest = 5.0f;
    // PUMP : pompage façon sidechain qui accélère. pumpFinest : 2 = 1/4, 3 = 1/8, 4 = 1/16
    Curve pump;   float pumpDepth = 0.0f, pumpFinest = 4.0f;
    // DRIVE : saturation douce (dB de gain d'attaque au climax)
    Curve drive;  float driveDb = 0.0f;
    // CRUSH : réduction de bits / d'échantillonnage
    Curve crush;  float crushAmt = 0.0f;
    // Coupe-bas qui monte (20 Hz → hpEnd) et coupe-haut qui se ferme (20 kHz → lpEnd)
    Curve hp;     float hpOn = 0.0f, hpEnd = 20.0f, hpReso = 0.0f;
    Curve lp;     float lpOn = 0.0f, lpEnd = 20000.0f, lpReso = 0.0f;
    // Largeur (graves gardés mono) et « aspiration » mono avant le drop
    Curve width;  float widthAmt = 0.0f;
    Curve vac;    float vacAmt = 0.0f;
    // Souffle (bruit filtré qui monte, suit l'enveloppe du signal)
    Curve noise;  float noiseGain = 0.0f;
    // Échos ping-pong (croche pointée) et réverbération
    Curve echo;   float echoSend = 0.0f, echoFb = 0.0f;
    Curve space;  float spaceSend = 0.0f, spaceDecay = 2.0f, dryDuck = 0.0f;
};

inline Preset lerp (const Preset& a, const Preset& b, float t) noexcept
{
    auto f = [t] (float x, float y) { return x + (y - x) * t; };
    Preset p;
    p.pitch = lerp (a.pitch, b.pitch, t);  p.pitchSemi = f (a.pitchSemi, b.pitchSemi);
    p.roll  = lerp (a.roll,  b.roll,  t);  p.rollMix   = f (a.rollMix, b.rollMix);     p.rollFinest = f (a.rollFinest, b.rollFinest);
    p.pump  = lerp (a.pump,  b.pump,  t);  p.pumpDepth = f (a.pumpDepth, b.pumpDepth); p.pumpFinest = f (a.pumpFinest, b.pumpFinest);
    p.drive = lerp (a.drive, b.drive, t);  p.driveDb   = f (a.driveDb, b.driveDb);
    p.crush = lerp (a.crush, b.crush, t);  p.crushAmt  = f (a.crushAmt, b.crushAmt);
    p.hp    = lerp (a.hp,    b.hp,    t);  p.hpOn = f (a.hpOn, b.hpOn); p.hpEnd = f (a.hpEnd, b.hpEnd); p.hpReso = f (a.hpReso, b.hpReso);
    p.lp    = lerp (a.lp,    b.lp,    t);  p.lpOn = f (a.lpOn, b.lpOn); p.lpEnd = f (a.lpEnd, b.lpEnd); p.lpReso = f (a.lpReso, b.lpReso);
    p.width = lerp (a.width, b.width, t);  p.widthAmt  = f (a.widthAmt, b.widthAmt);
    p.vac   = lerp (a.vac,   b.vac,   t);  p.vacAmt    = f (a.vacAmt, b.vacAmt);
    p.noise = lerp (a.noise, b.noise, t);  p.noiseGain = f (a.noiseGain, b.noiseGain);
    p.echo  = lerp (a.echo,  b.echo,  t);  p.echoSend  = f (a.echoSend, b.echoSend);   p.echoFb = f (a.echoFb, b.echoFb);
    p.space = lerp (a.space, b.space, t);  p.spaceSend = f (a.spaceSend, b.spaceSend); p.spaceDecay = f (a.spaceDecay, b.spaceDecay);
    p.dryDuck = f (a.dryDuck, b.dryDuck);
    return p;
}

inline float dbLin (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

constexpr int kNumPresets = 8;

inline const char* presetName (int i) noexcept
{
    static const char* names[kNumPresets] = { "LIFT", "CLASSIC", "EMO SWELL", "PUNK RUSH",
                                              "TECHNO RISE", "ROLL", "VACUUM", "HYPERPOP" };
    return names[std::clamp (i, 0, kNumPresets - 1)];
}

inline const char* presetDescription (int i) noexcept
{
    static const char* d[kNumPresets] = {
        "Montée douce (pré-refrain) : coupe-bas léger, largeur, un peu d'espace.",
        "Le build-up type : coupe-bas qui grimpe, roll qui accélère, souffle, réverb.",
        "Une vague de réverb et d'échos qui enfle ; le son s'ouvre et se dilue.",
        "Saturation qui pousse, roll 1/8 → 1/16, énergie brute.",
        "Coupe-bas résonant, pompe qui accélère (1/4 → 1/16), souffle blanc.",
        "Tout pour le roll : répétitions 1/8 → 1/32 et hauteur qui monte.",
        "Aspiration : le filtre se referme et l'image se resserre en mono.",
        "+12 demi-tons, bitcrush, roll 1/32, stéréo au maximum." };
    return d[std::clamp (i, 0, kNumPresets - 1)];
}

inline Preset makePreset (int i) noexcept
{
    Preset p;
    switch (std::clamp (i, 0, kNumPresets - 1))
    {
        case 0: // LIFT
            p.hp = { 0.00f, 1.0f, 1.6f };  p.hpOn = 1; p.hpEnd = 220.0f; p.hpReso = 0.10f;
            p.width = { 0.10f, 1.0f, 1.2f }; p.widthAmt = 0.60f;
            p.space = { 0.15f, 1.0f, 1.5f }; p.spaceSend = 0.22f; p.spaceDecay = 2.5f;
            p.noise = { 0.30f, 1.0f, 2.0f }; p.noiseGain = dbLin (-24.0f);
            p.drive = { 0.40f, 1.0f, 1.5f }; p.driveDb = 3.0f;
            break;

        case 1: // CLASSIC
            p.hp = { 0.00f, 1.0f, 2.0f };  p.hpOn = 1; p.hpEnd = 650.0f; p.hpReso = 0.35f;
            p.roll = { 0.30f, 1.0f, 1.3f }; p.rollMix = 0.70f; p.rollFinest = 5;
            p.noise = { 0.15f, 1.0f, 1.8f }; p.noiseGain = dbLin (-15.0f);
            p.space = { 0.25f, 1.0f, 1.4f }; p.spaceSend = 0.30f; p.spaceDecay = 3.5f;
            p.width = { 0.10f, 1.0f, 1.3f }; p.widthAmt = 0.80f;
            p.drive = { 0.50f, 1.0f, 2.0f }; p.driveDb = 6.0f;
            break;

        case 2: // EMO SWELL
            p.space = { 0.00f, 1.0f, 1.3f }; p.spaceSend = 0.60f; p.spaceDecay = 6.0f; p.dryDuck = 0.35f;
            p.echo  = { 0.10f, 1.0f, 1.4f }; p.echoSend = 0.35f; p.echoFb = 0.55f;
            p.hp = { 0.10f, 1.0f, 1.8f };  p.hpOn = 1; p.hpEnd = 300.0f; p.hpReso = 0.15f;
            p.width = { 0.00f, 1.0f, 1.2f }; p.widthAmt = 0.90f;
            p.roll = { 0.75f, 1.0f, 1.5f }; p.rollMix = 0.45f; p.rollFinest = 4;
            p.noise = { 0.40f, 1.0f, 2.0f }; p.noiseGain = dbLin (-22.0f);
            break;

        case 3: // PUNK RUSH
            p.drive = { 0.00f, 1.0f, 1.2f }; p.driveDb = 16.0f;
            p.hp = { 0.05f, 1.0f, 1.7f };  p.hpOn = 1; p.hpEnd = 300.0f; p.hpReso = 0.25f;
            p.roll = { 0.25f, 1.0f, 1.2f }; p.rollMix = 0.65f; p.rollFinest = 4;
            p.width = { 0.20f, 1.0f, 1.3f }; p.widthAmt = 0.70f;
            p.space = { 0.40f, 1.0f, 1.5f }; p.spaceSend = 0.18f; p.spaceDecay = 2.0f;
            p.noise = { 0.30f, 1.0f, 2.0f }; p.noiseGain = dbLin (-18.0f);
            break;

        case 4: // TECHNO RISE
            p.hp = { 0.00f, 1.0f, 1.8f };  p.hpOn = 1; p.hpEnd = 800.0f; p.hpReso = 0.60f;
            p.pump = { 0.10f, 1.0f, 1.2f }; p.pumpDepth = 0.65f; p.pumpFinest = 4;
            p.noise = { 0.05f, 1.0f, 1.5f }; p.noiseGain = dbLin (-11.0f);
            p.roll = { 0.70f, 1.0f, 1.4f }; p.rollMix = 0.60f; p.rollFinest = 5;
            p.space = { 0.20f, 1.0f, 1.4f }; p.spaceSend = 0.25f; p.spaceDecay = 3.0f;
            p.echo  = { 0.30f, 1.0f, 1.5f }; p.echoSend = 0.15f; p.echoFb = 0.45f;
            p.width = { 0.30f, 1.0f, 1.3f }; p.widthAmt = 0.60f;
            p.drive = { 0.30f, 1.0f, 1.5f }; p.driveDb = 6.0f;
            break;

        case 5: // ROLL
            p.roll = { 0.02f, 1.0f, 1.0f }; p.rollMix = 0.92f; p.rollFinest = 5;
            p.hp = { 0.10f, 1.0f, 1.8f };  p.hpOn = 1; p.hpEnd = 350.0f; p.hpReso = 0.30f;
            p.pitch = { 0.45f, 1.0f, 1.6f }; p.pitchSemi = 5.0f;
            p.space = { 0.30f, 1.0f, 1.5f }; p.spaceSend = 0.20f; p.spaceDecay = 2.5f;
            p.width = { 0.30f, 1.0f, 1.3f }; p.widthAmt = 0.50f;
            p.noise = { 0.50f, 1.0f, 2.0f }; p.noiseGain = dbLin (-20.0f);
            break;

        case 6: // VACUUM
            p.lp = { 0.00f, 1.0f, 1.4f };  p.lpOn = 1; p.lpEnd = 380.0f; p.lpReso = 0.45f;
            p.vac = { 0.55f, 1.0f, 1.6f };  p.vacAmt = 0.95f;
            p.roll = { 0.60f, 1.0f, 1.4f }; p.rollMix = 0.70f; p.rollFinest = 5;
            p.space = { 0.15f, 1.0f, 1.4f }; p.spaceSend = 0.35f; p.spaceDecay = 4.5f;
            p.echo  = { 0.20f, 1.0f, 1.5f }; p.echoSend = 0.20f; p.echoFb = 0.50f;
            p.noise = { 0.30f, 1.0f, 2.0f }; p.noiseGain = dbLin (-20.0f);
            p.pump = { 0.40f, 1.0f, 1.5f }; p.pumpDepth = 0.40f; p.pumpFinest = 3;
            break;

        case 7: // HYPERPOP
            p.pitch = { 0.15f, 1.0f, 1.5f }; p.pitchSemi = 12.0f;
            p.crush = { 0.40f, 1.0f, 1.5f }; p.crushAmt = 0.75f;
            p.roll = { 0.30f, 1.0f, 1.2f }; p.rollMix = 0.85f; p.rollFinest = 5;
            p.hp = { 0.05f, 1.0f, 1.7f };  p.hpOn = 1; p.hpEnd = 450.0f; p.hpReso = 0.50f;
            p.width = { 0.00f, 1.0f, 1.1f }; p.widthAmt = 1.00f;
            p.echo  = { 0.20f, 1.0f, 1.4f }; p.echoSend = 0.25f; p.echoFb = 0.50f;
            p.space = { 0.30f, 1.0f, 1.5f }; p.spaceSend = 0.30f; p.spaceDecay = 3.5f;
            p.noise = { 0.20f, 1.0f, 1.6f }; p.noiseGain = dbLin (-16.0f);
            p.drive = { 0.30f, 1.0f, 1.4f }; p.driveDb = 10.0f;
            break;
    }
    return p;
}

} // namespace klx
