#pragma once
// mask::klimax — moteur DSP (header-only, sans JUCE).
//
// Un seul réglage : KLIMAX (0 → 1). Chaque étage a sa propre courbe d'entrée en jeu (Presets.h).
// Chaîne : PITCH → ROLL → DRIVE → CRUSH → COUPE-BAS / COUPE-HAUT → SOUFFLE → LARGEUR
//          → ÉCHOS → RÉVERB → PUMP → GAIN MATCH (invisible)
//
// Garanties :
//  - KLIMAX = 0 et queues éteintes → sortie = entrée retardée de la latence, bit à bit.
//  - Tout part de sa valeur neutre de façon continue (pas d'effet on/off), réglages lissés.
//  - Résultat indépendant de la taille des blocs de l'hôte (contrôle toutes les 32 éch.).
//  - Les étages inactifs ne coûtent rien (sautés), réverb / échos s'éteignent tout seuls.

#include "Presets.h"
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace klx
{

constexpr int   kCtl = 32;                  // période de contrôle (échantillons)
constexpr float kPi  = 3.14159265358979f;

inline float clamp01 (float x) noexcept  { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float smooth01 (float x) noexcept { x = clamp01 (x); return x * x * (3.0f - 2.0f * x); }
inline float tanhF (float x) noexcept
{
    x = std::clamp (x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
inline uint32_t nextPow2 (uint32_t v) noexcept { uint32_t p = 1; while (p < v) p <<= 1; return p; }

// Valeur interpolée linéairement sur une période de contrôle.
struct Ramp
{
    float a = 0.0f, b = 0.0f;
    void set (float t) noexcept  { a = b; b = t; }
    void snap (float t) noexcept { a = b = t; }
    float at (int j) const noexcept { return a + (b - a) * (float) (j + 1) * (1.0f / (float) kCtl); }
    bool nonZero() const noexcept { return a != 0.0f || b != 0.0f; }
    bool is (float v) const noexcept { return a == v && b == v; }
};

struct SvfCoef
{
    float a1 = 0, a2 = 0, a3 = 0, k = 1.41421356f;
    void set (float fc, float q, float fs) noexcept
    {
        const float g = std::tan (kPi * std::clamp (fc, 5.0f, 0.45f * fs) / fs);
        k  = 1.0f / std::max (0.3f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

struct Svf
{
    float ic1 = 0, ic2 = 0;
    void reset() noexcept { ic1 = ic2 = 0; }
    inline float hp (const SvfCoef& c, float x) noexcept
    {
        const float v3 = x - ic2, v1 = c.a1 * ic1 + c.a2 * v3, v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return x - c.k * v1 - v2;
    }
    inline float lp (const SvfCoef& c, float x) noexcept
    {
        const float v3 = x - ic2, v1 = c.a1 * ic1 + c.a2 * v3, v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return v2;
    }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float tick (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

// Pondération K (ITU-R BS.1770) pour le gain match.
inline void makeKWeighting (double fs, Biquad& shelf, Biquad& hp)
{
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (3.141592653589793 * f0 / fs), Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416), a0 = 1.0 + K / Q + K * K;
        shelf.b0 = (float) ((Vh + Vb * K / Q + K * K) / a0);
        shelf.b1 = (float) (2.0 * (K * K - Vh) / a0);
        shelf.b2 = (float) ((Vh - Vb * K / Q + K * K) / a0);
        shelf.a1 = (float) (2.0 * (K * K - 1.0) / a0);
        shelf.a2 = (float) ((1.0 - K / Q + K * K) / a0);
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (3.141592653589793 * f0 / fs), a0 = 1.0 + K / Q + K * K;
        hp.b0 = 1.0f; hp.b1 = -2.0f; hp.b2 = 1.0f;
        hp.a1 = (float) (2.0 * (K * K - 1.0) / a0);
        hp.a2 = (float) ((1.0 - K / Q + K * K) / a0);
    }
}

// Allpass de Schroeder (diffusion / décorrélation).
struct Allpass
{
    std::vector<float> buf; uint32_t mask = 0, w = 0, d = 1; float g = 0.5f;
    void init (int delay, float gain) { d = (uint32_t) std::max (1, delay); buf.assign (nextPow2 (d + 1), 0.0f); mask = (uint32_t) buf.size() - 1; w = 0; g = gain; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline float tick (float x) noexcept
    {
        const float z = buf[(w - d) & mask];
        const float y = -g * x + z;
        buf[w] = x + g * y;
        w = (w + 1) & mask;
        return y;
    }
};

class Engine
{
public:
    struct Transport { bool playing = false, hasPpq = false; double ppq = 0.0, bpm = 120.0; };

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        lat   = std::max (64, (int) std::lround (0.025 * sampleRate));
        xfLen = std::max (16.0f, std::round (0.005f * fs));

        auto mkBuf = [] (std::vector<float>& b, uint32_t& mask, double len)
        {
            b.assign (nextPow2 ((uint32_t) std::ceil (len) + 8), 0.0f);
            mask = (uint32_t) b.size() - 1;
        };
        mkBuf (pbL, pMask, lat + 8); mkBuf (pbR, pMask, lat + 8);
        mkBuf (rbL, rMask, 2.6 * sampleRate); mkBuf (rbR, rMask, 2.6 * sampleRate);
        mkBuf (ebL, eMask, 1.7 * sampleRate); mkBuf (ebR, eMask, 1.7 * sampleRate);

        static const float lineMs[8] = { 31.7f, 37.3f, 41.9f, 47.3f, 53.1f, 59.3f, 67.1f, 73.9f };
        revMaxLen = 0;
        for (int k = 0; k < 8; ++k)
        {
            revLen[k] = (uint32_t) std::lround (lineMs[k] * 0.001 * sampleRate);
            line[k].assign (nextPow2 (revLen[k] + 2), 0.0f);
            lineMask[k] = (uint32_t) line[k].size() - 1;
            revMaxLen = std::max (revMaxLen, revLen[k]);
        }
        preAp[0].init ((int) std::lround (0.0053 * sampleRate), 0.6f);
        preAp[1].init ((int) std::lround (0.0079 * sampleRate), 0.6f);
        decAp[0].init ((int) std::lround (0.0043 * sampleRate), 0.5f);
        decAp[1].init ((int) std::lround (0.0097 * sampleRate), 0.5f);

        makeKWeighting (sampleRate, kwProto[0], kwProto[1]);
        finalizeKWeighting();

        auto onePole = [this] (float hz) { return 1.0f - std::exp (-2.0f * kPi * hz / fs); };
        cKnob   = 1.0f - std::exp (-(float) kCtl / (0.025f * fs));
        morphStep = (float) kCtl / (0.30f * fs);
        cGmAvg  = 1.0f - std::exp (-(float) kCtl / (0.40f * fs));
        cGmGain = 1.0f - std::exp (-(float) kCtl / (0.30f * fs));
        cGmShort = 1.0f - std::exp (-(float) kCtl / (0.05f * fs));
        gmPeakDecay = std::exp (-(float) kCtl / (2.0f * fs));
        cEnvAtk = 1.0f - std::exp (-1.0f / (0.005f * fs));
        cEnvRel = 1.0f - std::exp (-1.0f / (0.300f * fs));
        cSideLo = onePole (200.0f);
        cMidLo  = onePole (300.0f);
        cFbHp   = onePole (250.0f);
        cFbLp   = onePole (7000.0f);
        cDamp   = onePole (6500.0f);
        cDly    = 1.0f - std::exp (-(float) kCtl / (0.05f * fs));
        pumpAtk = 1.0f / (0.004f * fs);
        reset();
    }

    void reset()
    {
        std::fill (pbL.begin(), pbL.end(), 0.0f); std::fill (pbR.begin(), pbR.end(), 0.0f);
        std::fill (rbL.begin(), rbL.end(), 0.0f); std::fill (rbR.begin(), rbR.end(), 0.0f);
        clearEcho(); clearReverb();
        pw = rw = 0; dA = dB = lat; xfading = false; xfT = 0;
        ctlPos = 0; kS = kTarget; morphT = 1.0f; from = to = makePreset (toIdx);
        posPpq = 0.0; lastBeat = INT64_MIN; haveCell = false; rp = 0; rLen = 1024; rFade = 64;
        pumpLvl = 2; rollLvl = 3;
        for (auto* r : allRamps()) r->snap (0.0f);
        ratio.snap (1.0f); dry.snap (1.0f); gmGain.snap (1.0f); driveGain.snap (1.0f); driveNorm.snap (1.0f);
        for (auto& s : hpS) s.reset();
        for (auto& s : lpS) s.reset();
        for (auto& s : nzS) s.reset();
        hpOn = lpOn = widthOn = crushOn = false;
        holdPh = 0; hL = hR = 0; env = 0;
        sLo = mLo = 0; decAp[0].reset(); decAp[1].reset();
        resetGm();
        rng[0] = 0x9E3779B9u; rng[1] = 0x7F4A7C15u;
        dlyTarget = 0.75f * 0.5f * fs; dly.snap (dlyTarget);
    }

    int latencySamples() const noexcept { return lat; }

    void setPreset (int idx) noexcept
    {
        idx = std::clamp (idx, 0, kNumPresets - 1);
        if (idx == toIdx) return;
        from = morphT >= 1.0f ? to : lerp (from, to, morphT);
        to = makePreset (idx);
        toIdx = idx;
        morphT = 0.0f;
    }
    // Pour l'initialisation (pas de fondu).
    void setPresetImmediate (int idx) noexcept
    {
        toIdx = std::clamp (idx, 0, kNumPresets - 1);
        from = to = makePreset (toIdx);
        morphT = 1.0f;
    }

    // knob : 0 … 1. Traitement en place, stéréo.
    void process (float* L, float* R, int n, float knob, const Transport& tr) noexcept
    {
        kTarget = clamp01 (knob);
        bpm = (tr.bpm > 1.0) ? std::clamp (tr.bpm, 30.0, 300.0) : 120.0;
        spb = 60.0 * fs / bpm;
        if (tr.playing && tr.hasPpq)
            posPpq = tr.ppq - (double) lat / spb;   // grille alignée sur l'audio retardé

        int off = 0;
        while (off < n)
        {
            if (ctlPos == 0) updateControl();
            const int m = std::min (n - off, kCtl - ctlPos);
            processChunk (L + off, R + off, m, ctlPos);
            ctlPos = (ctlPos + m) % kCtl;
            off += m;
        }
    }

    bool isIdle() const noexcept
    {
        return kS == 0.0f && ! xfading && dA == (double) lat && echoIdle && revIdle
            && gmGain.is (1.0f) && dry.is (1.0f) && ! rollMix.nonZero() && ! pumpDepth.nonZero()
            && ! driveMix.nonZero() && ! crushMix.nonZero() && ! hpMix.nonZero() && ! lpMix.nonZero()
            && ! widen.nonZero() && ! vac.nonZero() && ! noiseGain.nonZero();
    }
    float smoothedKnob() const noexcept { return kS; }
    float currentGmGainDb() const noexcept { return 20.0f * std::log10 (std::max (1e-6f, gmGain.b)); }

private:
    // ---------------------------------------------------------------- contrôle
    Ramp ratio, rollMix, pumpDepth, driveMix, driveGain, driveNorm, crushMix, hpMix, lpMix,
         widen, vac, noiseGain, echoSend, spaceSend, dry, gmGain;
    std::vector<Ramp*> allRamps()
    {
        return { &ratio, &rollMix, &pumpDepth, &driveMix, &driveGain, &driveNorm, &crushMix, &hpMix, &lpMix,
                 &widen, &vac, &noiseGain, &echoSend, &spaceSend, &dry, &gmGain };
    }

    void updateControl() noexcept
    {
        // lissage du potard
        kS += (kTarget - kS) * cKnob;
        if (std::abs (kTarget - kS) < 1.0e-5f) kS = kTarget;

        if (morphT < 1.0f) { morphT = std::min (1.0f, morphT + morphStep); if (morphT >= 1.0f) from = to; }
        const Preset P = morphT >= 1.0f ? to : lerp (from, to, morphT);
        const float k = kS;

        // PITCH
        const float semi = P.pitchSemi * P.pitch (k);
        ratio.set (semi > 1.0e-4f ? std::exp2 (semi / 12.0f) : 1.0f);

        // ROLL
        const float aR = P.roll (k);
        rollMix.set (P.rollMix * aR);
        {
            const int finest = std::clamp ((int) std::lround (P.rollFinest), 3, 5), nl = finest - 2;
            rollLvlTarget = 3 + std::min ((int) (aR * (float) nl), nl - 1);
        }
        // PUMP
        const float aU = P.pump (k);
        pumpDepth.set (std::min (0.95f, P.pumpDepth * aU));
        {
            const int finest = std::clamp ((int) std::lround (P.pumpFinest), 2, 4), nl = finest - 1;
            pumpLvlTarget = 2 + std::min ((int) (aU * (float) nl), nl - 1);
        }
        // DRIVE
        const float aD = P.drive (k);
        const float dg = dbLin (P.driveDb * aD);
        driveGain.set (dg);
        driveNorm.set (1.0f / std::sqrt (dg));
        driveMix.set (P.driveDb > 0.05f ? smooth01 (aD * 4.0f) : 0.0f);
        // CRUSH
        const float aC = P.crush (k) * P.crushAmt;
        crushMix.set (smooth01 (aC * 3.0f));
        crushQ = std::exp2 (15.0f - 11.0f * aC);   // 16 → 5 bits
        crushHold = 1.0f + 7.0f * aC;
        // FILTRES
        const float aH = P.hp (k) * P.hpOn;
        hpMix.set (smooth01 (aH * 12.0f));
        if (hpMix.nonZero())
        {
            const float fc = 20.0f * std::pow (std::max (P.hpEnd, 20.0f) / 20.0f, aH);
            hpC[0].set (fc, 0.541f, fs);
            hpC[1].set (fc, 1.307f + P.hpReso * 4.0f * aH, fs);
        }
        const float aL = P.lp (k) * P.lpOn;
        lpMix.set (smooth01 (aL * 12.0f));
        if (lpMix.nonZero())
        {
            const float fc = 20000.0f * std::pow (std::min (P.lpEnd, 20000.0f) / 20000.0f, aL);
            lpC[0].set (fc, 0.541f, fs);
            lpC[1].set (fc, 1.307f + P.lpReso * 4.0f * aL, fs);
        }
        // LARGEUR / ASPIRATION
        widen.set (P.widthAmt * P.width (k));
        vac.set (std::min (1.0f, P.vacAmt * P.vac (k)));
        // SOUFFLE
        const float aN = P.noise (k);
        noiseGain.set (P.noiseGain * aN);
        if (noiseGain.nonZero()) nzC.set (600.0f * std::pow (20.0f, aN), 0.9f, fs);
        // ÉCHOS
        const float aE = P.echo (k);
        echoSend.set (P.echoSend * aE);
        echoFb = P.echoFb * std::sqrt (aE);
        dlyTarget = std::min ((float) (0.75 * spb), (float) eMask - 8.0f);
        if (echoIdle && ! echoSend.nonZero()) dly.snap (dlyTarget);
        else dly.set (dly.b + (dlyTarget - dly.b) * cDly);
        // RÉVERB
        const float aS = P.space (k);
        spaceSend.set (P.spaceSend * aS);
        dry.set (1.0f - P.dryDuck * std::pow (aS, 1.5f));
        const float decay = 1.2f + (P.spaceDecay - 1.2f) * aS;
        if (std::abs (decay - revDecay) > 1.0e-3f)
        {
            revDecay = decay;
            for (int i = 0; i < 8; ++i)
                revG[i] = std::pow (10.0f, -3.0f * (float) revLen[i] / (decay * fs));
        }

        // GAIN MATCH (invisible) : sortie = niveau d'entrée, pondération K, gelé quand l'entrée retombe
        const float w = smooth01 (k * 20.0f);
        if (w > 0.0f)
        {
            if (gmCount > 0)
            {
                const float pIn = gmSumIn / (float) gmCount, pOut = gmSumOut / (float) gmCount;
                gmShort += (pIn - gmShort) * cGmShort;
                gmPeak = std::max (gmShort, gmPeak * gmPeakDecay);
                if (gmShort > 1.0e-7f && gmShort > gmPeak * 0.01f)
                {
                    gmAvgIn  += (pIn  - gmAvgIn)  * cGmAvg;
                    gmAvgOut += (pOut - gmAvgOut) * cGmAvg;
                    gmWarm += gmCount;
                }
            }
            gmSumIn = gmSumOut = 0.0f; gmCount = 0;
            float gt = gmTarget;
            if (gmWarm > (int) (0.15f * fs) && gmAvgOut > 1.0e-12f && gmAvgIn > 1.0e-12f)
                gt = std::clamp (std::sqrt (gmAvgIn / gmAvgOut), 0.25f, 2.8184f);   // −12 … +9 dB
            gmTarget = gt;
            gmS += (gmTarget - gmS) * cGmGain;
            gmRun = true;
        }
        else
        {
            resetGm();
        }
        gmGain.set (w > 0.0f ? 1.0f + w * (gmS - 1.0f) : 1.0f);
    }

    void resetGm() noexcept
    {
        gmRun = false; gmSumIn = gmSumOut = 0; gmCount = 0; gmShort = gmPeak = 0;
        gmAvgIn = gmAvgOut = 0; gmWarm = 0; gmTarget = 1.0f; gmS = 1.0f;
        for (auto& b : kwIn)  b.reset();
        for (auto& b : kwOut) b.reset();
    }

    // ---------------------------------------------------------------- audio
    void processChunk (float* L, float* R, int m, int j0) noexcept
    {
        // horloge (grille de l'hôte, ou horloge libre à l'arrêt)
        const double inc = 1.0 / spb;
        for (int i = 0; i < m; ++i)
        {
            const double fl = std::floor (posPpq);
            const int64_t b = (int64_t) fl;
            newBeat[i] = (b != lastBeat);
            lastBeat = b;
            beatFrac[i] = (float) (posPpq - fl);
            posPpq += inc;
        }

        if (gmRun) { std::memcpy (inL, L, sizeof (float) * (size_t) m); std::memcpy (inR, R, sizeof (float) * (size_t) m); }

        stagePitch (L, R, m, j0);
        stageRoll  (L, R, m, j0);
        if (driveMix.nonZero()) stageDrive (L, R, m, j0);
        stageCrush (L, R, m, j0);
        stageFilters (L, R, m, j0);
        if (noiseGain.nonZero()) stageNoise (L, R, m, j0); else env = 0.0f;
        stageWidth (L, R, m, j0);
        stageEcho  (L, R, m, j0);
        stageSpace (L, R, m, j0);
        stagePump  (L, R, m, j0);
        stageGain  (L, R, m, j0);
    }

    // PITCH : décalage granulaire à deux têtes de lecture ; au repos = retard pur de « lat » échantillons.
    static inline float readH (const float* b, uint32_t mask, uint32_t w, double d) noexcept
    {
        const int di = (int) d;
        const float f = (float) (d - di);
        const uint32_t i0 = (w - (uint32_t) di) & mask;
        const float y0 = b[i0];
        if (f == 0.0f) return y0;
        const float ym1 = b[(i0 + 1) & mask], y1 = b[(i0 - 1) & mask], y2 = b[(i0 - 2) & mask];
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + y0;
    }

    void stagePitch (float* L, float* R, int m, int j0) noexcept
    {
        const double latD = (double) lat;
        const bool still = ratio.is (1.0f) && ! xfading && dA == latD;
        for (int i = 0; i < m; ++i)
        {
            pbL[pw] = L[i]; pbR[pw] = R[i];
            if (still)
            {
                const uint32_t ri = (pw - (uint32_t) lat) & pMask;
                L[i] = pbL[ri]; R[i] = pbR[ri];
            }
            else
            {
                const double step = (double) ratio.at (j0 + i) - 1.0;
                if (! xfading)
                {
                    if (step > 0.0)
                    {
                        dA -= step;
                        if (dA <= 2.0 + step * xfLen) startXf();
                    }
                    else if (dA != latD) startXf();      // retour au repos : recentrage en un fondu
                }
                else
                {
                    if (step > 0.0) { dA -= step; dB -= step; }
                    xfT += 1.0f / xfLen;
                    if (xfT >= 1.0f) xfading = false;
                }
                dA = std::max (dA, 2.0); dB = std::max (dB, 2.0);

                if (! xfading)
                {
                    L[i] = readH (pbL.data(), pMask, pw, dA);
                    R[i] = readH (pbR.data(), pMask, pw, dA);
                }
                else
                {
                    const float gA = 0.5f - 0.5f * std::cos (kPi * xfT), gB = 1.0f - gA;
                    L[i] = gA * readH (pbL.data(), pMask, pw, dA) + gB * readH (pbL.data(), pMask, pw, dB);
                    R[i] = gA * readH (pbR.data(), pMask, pw, dA) + gB * readH (pbR.data(), pMask, pw, dB);
                }
            }
            pw = (pw + 1) & pMask;
        }
    }
    void startXf() noexcept { dB = dA; dA = (double) lat; xfading = true; xfT = 0.0f; }

    // ROLL : à chaque temps, le début du temps est répété en 1/8, 1/16 puis 1/32.
    void stageRoll (float* L, float* R, int m, int j0) noexcept
    {
        const bool on = rollMix.nonZero();
        for (int i = 0; i < m; ++i)
        {
            rbL[rw] = L[i]; rbR[rw] = R[i];
            if (newBeat[i])
            {
                capStart = rw; rp = 0; haveCell = true;
                rollLvl = rollLvlTarget; pumpLvl = pumpLvlTarget;
                rLen  = std::max (32, (int) (spb / (double) (1 << (rollLvl - 2))));
                rFade = std::max (8, std::min (64, rLen / 4));
            }
            if (on && haveCell)
            {
                const float mix = rollMix.at (j0 + i);
                const uint32_t idx = (capStart + (uint32_t) rp) & rMask;
                float e = 1.0f;
                if (rp < rFade) e = (float) rp / (float) rFade;
                else if (rLen - rp < rFade) e = (float) (rLen - rp) / (float) rFade;
                const float g = mix * e;
                L[i] += g * (rbL[idx] - L[i]);
                R[i] += g * (rbR[idx] - R[i]);
            }
            if (++rp >= rLen) rp = 0;
            rw = (rw + 1) & rMask;
        }
    }

    void stageDrive (float* L, float* R, int m, int j0) noexcept
    {
        for (int i = 0; i < m; ++i)
        {
            const float mx = driveMix.at (j0 + i), g = driveGain.at (j0 + i), nm = driveNorm.at (j0 + i);
            L[i] += mx * (tanhF (g * L[i]) * nm - L[i]);
            R[i] += mx * (tanhF (g * R[i]) * nm - R[i]);
        }
    }

    void stageCrush (float* L, float* R, int m, int j0) noexcept
    {
        if (! crushMix.nonZero()) { crushOn = false; return; }
        if (! crushOn) { crushOn = true; holdPh = crushHold; }
        const float q = crushQ, iq = 1.0f / crushQ;
        for (int i = 0; i < m; ++i)
        {
            holdPh += 1.0f;
            if (holdPh >= crushHold)
            {
                holdPh -= crushHold;
                if (holdPh >= crushHold) holdPh = 0.0f;
                hL = std::floor (L[i] * q + 0.5f) * iq;
                hR = std::floor (R[i] * q + 0.5f) * iq;
            }
            const float mx = crushMix.at (j0 + i);
            L[i] += mx * (hL - L[i]);
            R[i] += mx * (hR - R[i]);
        }
    }

    void stageFilters (float* L, float* R, int m, int j0) noexcept
    {
        if (hpMix.nonZero())
        {
            hpOn = true;
            for (int i = 0; i < m; ++i)
            {
                const float mx = hpMix.at (j0 + i);
                const float yl = hpS[1].hp (hpC[1], hpS[0].hp (hpC[0], L[i]));
                const float yr = hpS[3].hp (hpC[1], hpS[2].hp (hpC[0], R[i]));
                L[i] += mx * (yl - L[i]);
                R[i] += mx * (yr - R[i]);
            }
        }
        else if (hpOn) { hpOn = false; for (auto& s : hpS) s.reset(); }

        if (lpMix.nonZero())
        {
            lpOn = true;
            for (int i = 0; i < m; ++i)
            {
                const float mx = lpMix.at (j0 + i);
                const float yl = lpS[1].lp (lpC[1], lpS[0].lp (lpC[0], L[i]));
                const float yr = lpS[3].lp (lpC[1], lpS[2].lp (lpC[0], R[i]));
                L[i] += mx * (yl - L[i]);
                R[i] += mx * (yr - R[i]);
            }
        }
        else if (lpOn) { lpOn = false; for (auto& s : lpS) s.reset(); }
    }

    inline float rnd (int c) noexcept
    {
        uint32_t x = rng[c];
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        rng[c] = x;
        return (float) (int32_t) x * 4.6566129e-10f;
    }

    // SOUFFLE : bruit filtré (coupe-bas qui monte 600 Hz → 12 kHz), suit l'enveloppe du signal.
    void stageNoise (float* L, float* R, int m, int j0) noexcept
    {
        for (int i = 0; i < m; ++i)
        {
            const float a = std::max (std::abs (L[i]), std::abs (R[i]));
            env += (a - env) * (a > env ? cEnvAtk : cEnvRel);
            const float g = noiseGain.at (j0 + i) * env * 1.7f;
            L[i] += g * nzS[0].hp (nzC, rnd (0));
            R[i] += g * nzS[1].hp (nzC, rnd (1));
        }
    }

    // LARGEUR : côté renforcé dans les aigus + décorrélation (mono-compatible : rien n'est ajouté au mid),
    // graves ramenés vers le mono ; ASPIRATION : tout le côté se referme.
    void stageWidth (float* L, float* R, int m, int j0) noexcept
    {
        if (! (widen.nonZero() || vac.nonZero()))
        {
            if (widthOn) { widthOn = false; sLo = mLo = 0; decAp[0].reset(); decAp[1].reset(); }
            return;
        }
        widthOn = true;
        for (int i = 0; i < m; ++i)
        {
            const float w = widen.at (j0 + i), v = vac.at (j0 + i);
            const float M = 0.5f * (L[i] + R[i]), S = 0.5f * (L[i] - R[i]);
            sLo += (S - sLo) * cSideLo;
            const float sHi = S - sLo;
            mLo += (M - mLo) * cMidLo;
            const float d = decAp[1].tick (decAp[0].tick (M - mLo));
            const float S2 = (sLo * (1.0f - 0.8f * w) + sHi * (1.0f + 0.8f * w) + 0.6f * w * d) * (1.0f - v);
            L[i] = M + S2;
            R[i] = M - S2;
        }
    }

    void clearEcho() noexcept
    {
        std::fill (ebL.begin(), ebL.end(), 0.0f); std::fill (ebR.begin(), ebR.end(), 0.0f);
        fbHp = fbLp = 0; echoIdle = true; echoQuiet = 0; ew = 0;
    }

    // ÉCHOS ping-pong à la croche pointée, boucle filtrée.
    void stageEcho (float* L, float* R, int m, int j0) noexcept
    {
        const bool sending = echoSend.nonZero();
        if (! sending && echoIdle) return;
        echoIdle = false;
        float peak = 0.0f;
        for (int i = 0; i < m; ++i)
        {
            const float d = dly.at (j0 + i);
            const int di = (int) d; const float f = d - (float) di;
            const uint32_t a = (ew - (uint32_t) di) & eMask, b = (a - 1) & eMask;
            const float lO = ebL[a] + f * (ebL[b] - ebL[a]);
            const float rO = ebR[a] + f * (ebR[b] - ebR[a]);
            fbHp += (rO - fbHp) * cFbHp;
            const float hp = rO - fbHp;
            fbLp += (hp - fbLp) * cFbLp;
            const float in = 0.5f * (L[i] + R[i]) * echoSend.at (j0 + i);
            ebL[ew] = in + echoFb * fbLp;
            ebR[ew] = lO;
            ew = (ew + 1) & eMask;
            L[i] += 0.8f * lO; R[i] += 0.8f * rO;
            peak = std::max (peak, std::max (std::abs (lO), std::abs (rO)));
        }
        if (! sending)
        {
            echoQuiet = peak < 1.0e-7f ? echoQuiet + m : 0;
            if (echoQuiet > (int) (2.0f * dly.b) + kCtl) clearEcho();
        }
        else echoQuiet = 0;
    }

    void clearReverb() noexcept
    {
        for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
        for (auto& v : damp) v = 0.0f;
        for (auto& w : lw) w = 0;
        preAp[0].reset(); preAp[1].reset();
        revIdle = true; revQuiet = 0;
    }

    // RÉVERB : FDN 8 lignes (Hadamard), amortissement des aigus, décroissance 1,2 → 6 s.
    void stageSpace (float* L, float* R, int m, int j0) noexcept
    {
        const bool sending = spaceSend.nonZero();
        if (! sending && revIdle)
        {
            if (! dry.is (1.0f))
                for (int i = 0; i < m; ++i) { const float d = dry.at (j0 + i); L[i] *= d; R[i] *= d; }
            return;
        }
        revIdle = false;
        float peak = 0.0f;
        constexpr float hs = 0.35355339f; // 1/sqrt(8)
        for (int i = 0; i < m; ++i)
        {
            const float x = preAp[1].tick (preAp[0].tick (0.5f * (L[i] + R[i]) * spaceSend.at (j0 + i)));
            float o[8];
            for (int k = 0; k < 8; ++k)
            {
                const float r = line[k][(lw[k] - revLen[k]) & lineMask[k]];
                damp[k] += (r - damp[k]) * cDamp;
                o[k] = damp[k] * revG[k];
            }
            float h[8] = { o[0], o[1], o[2], o[3], o[4], o[5], o[6], o[7] };
            for (int s = 1; s < 8; s <<= 1)
                for (int b = 0; b < 8; b += 2 * s)
                    for (int j = b; j < b + s; ++j)
                    {
                        const float u = h[j], v = h[j + s];
                        h[j] = u + v; h[j + s] = u - v;
                    }
            for (int k = 0; k < 8; ++k)
            {
                line[k][lw[k]] = h[k] * hs + ((k & 1) ? -x : x);
                lw[k] = (lw[k] + 1) & lineMask[k];
            }
            const float outL = 0.35f * (o[0] + o[2] + o[4] + o[6]);
            const float outR = 0.35f * (o[1] + o[3] + o[5] + o[7]);
            const float d = dry.at (j0 + i);
            L[i] = L[i] * d + outL;
            R[i] = R[i] * d + outR;
            peak = std::max (peak, std::max (std::abs (outL), std::abs (outR)));
        }
        if (! sending)
        {
            revQuiet = peak < 1.0e-7f ? revQuiet + m : 0;
            if (revQuiet > (int) revMaxLen + kCtl) clearReverb();
        }
        else revQuiet = 0;
    }

    // PUMP : creux façon sidechain au début de chaque division (1/4 → 1/8 → 1/16), calé sur la grille.
    void stagePump (float* L, float* R, int m, int j0) noexcept
    {
        if (! pumpDepth.nonZero()) return;
        for (int i = 0; i < m; ++i)
        {
            const float depth = pumpDepth.at (j0 + i);
            const float mult = (float) (1 << (pumpLvl - 2));
            float p = beatFrac[i] * mult; p -= std::floor (p);
            const float tSince = p * (float) spb / mult;
            const float envA = std::min (1.0f, tSince * pumpAtk);
            const float dec = (1.0f - p) * (1.0f - p);
            const float g = 1.0f - depth * envA * dec;
            L[i] *= g; R[i] *= g;
        }
    }

    void stageGain (float* L, float* R, int m, int j0) noexcept
    {
        if (gmRun)
        {
            // accumulation échantillon par échantillon : même ordre d'arrondi quelle que soit la taille de bloc
            for (int i = 0; i < m; ++i)
            {
                const float a = kwIn[1].tick (kwIn[0].tick (inL[i])), b = kwIn[3].tick (kwIn[2].tick (inR[i]));
                const float c = kwOut[1].tick (kwOut[0].tick (L[i])), d = kwOut[3].tick (kwOut[2].tick (R[i]));
                gmSumIn += 0.5f * (a * a + b * b); gmSumOut += 0.5f * (c * c + d * d);
            }
            gmCount += m;
        }
        if (! gmGain.is (1.0f))
            for (int i = 0; i < m; ++i) { const float g = gmGain.at (j0 + i); L[i] *= g; R[i] *= g; }
    }

    // ---------------------------------------------------------------- état
    float fs = 48000.0f;
    int lat = 1200; float xfLen = 240.0f;
    double bpm = 120.0, spb = 24000.0, posPpq = 0.0; int64_t lastBeat = INT64_MIN;

    float kTarget = 0.0f, kS = 0.0f, cKnob = 0.1f;
    int ctlPos = 0;
    Preset from, to; int toIdx = 1; float morphT = 1.0f, morphStep = 0.01f;

    bool newBeat[kCtl] {}; float beatFrac[kCtl] {};
    float inL[kCtl] {}, inR[kCtl] {};

    // pitch
    std::vector<float> pbL, pbR; uint32_t pMask = 0, pw = 0;
    double dA = 0, dB = 0; bool xfading = false; float xfT = 0;
    // roll
    std::vector<float> rbL, rbR; uint32_t rMask = 0, rw = 0, capStart = 0;
    int rp = 0, rLen = 1024, rFade = 64, rollLvl = 3, rollLvlTarget = 3; bool haveCell = false;
    // pump
    int pumpLvl = 2, pumpLvlTarget = 2; float pumpAtk = 0.005f;
    // crush
    float crushQ = 32768.0f, crushHold = 1.0f, holdPh = 0, hL = 0, hR = 0; bool crushOn = false;
    // filtres
    SvfCoef hpC[2], lpC[2], nzC; Svf hpS[4], lpS[4], nzS[2]; bool hpOn = false, lpOn = false;
    // souffle
    float env = 0, cEnvAtk = 0.01f, cEnvRel = 0.001f; uint32_t rng[2] { 1u, 2u };
    // largeur
    float sLo = 0, mLo = 0, cSideLo = 0.02f, cMidLo = 0.04f; Allpass decAp[2]; bool widthOn = false;
    // échos
    std::vector<float> ebL, ebR; uint32_t eMask = 0, ew = 0;
    Ramp dly; float dlyTarget = 0, cDly = 0.05f, echoFb = 0, fbHp = 0, fbLp = 0, cFbHp = 0.03f, cFbLp = 0.6f;
    bool echoIdle = true; int echoQuiet = 0;
    // réverb
    std::vector<float> line[8]; uint32_t lineMask[8] {}, lw[8] {}, revLen[8] {}, revMaxLen = 0;
    float damp[8] {}, revG[8] {}, revDecay = -1.0f, cDamp = 0.5f; Allpass preAp[2];
    bool revIdle = true; int revQuiet = 0;
    // gain match
    Biquad kwProto[2], kwIn[4], kwOut[4];
    bool gmRun = false; float gmSumIn = 0, gmSumOut = 0; int gmCount = 0, gmWarm = 0;
    float gmShort = 0, gmPeak = 0, gmAvgIn = 0, gmAvgOut = 0, gmTarget = 1, gmS = 1;
    float cGmAvg = 0.01f, cGmGain = 0.01f, cGmShort = 0.1f, gmPeakDecay = 0.999f;

    // les filtres K des deux mesures partagent les coefficients calculés dans prepare()
    void finalizeKWeighting() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            kwIn[2 * c] = kwProto[0]; kwIn[2 * c + 1] = kwProto[1];
            kwOut[2 * c] = kwProto[0]; kwOut[2 * c + 1] = kwProto[1];
        }
    }
};

} // namespace klx
