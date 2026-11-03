// Tests du moteur mask::klimax (sans JUCE).
#include "../Source/Dsp/Engine.h"
#include <cstdio>
#include <random>
#include <chrono>
#include <vector>
#include <cmath>
#if defined(__x86_64__)
 #include <xmmintrin.h>
#endif

using namespace klx;

static int failures = 0;
#define CHECK(c, ...) do { if (! (c)) { ++failures; std::printf ("  ÉCHEC : " __VA_ARGS__); std::printf ("\n"); } } while (0)

struct Sig { std::vector<float> L, R; };

static Sig makeMusic (int n, float fs, unsigned seed = 1)
{
    // mélange « musical » : basse 55 Hz, accord, kick à 120 BPM, bruit (charley)
    Sig s; s.L.resize ((size_t) n); s.R.resize ((size_t) n);
    std::mt19937 rng (seed); std::uniform_real_distribution<float> u (-1.f, 1.f);
    const int beat = (int) (fs * 0.5f);
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / fs;
        const int pb = i % beat; const float tb = (float) pb / fs;
        const float kick = std::exp (-tb * 18.f) * std::sin (2.f * 3.14159265f * (50.f + 120.f * std::exp (-tb * 30.f)) * tb);
        const float bass = 0.25f * std::sin (2.f * 3.14159265f * 55.f * t);
        const float chord = 0.08f * (std::sin (2.f * 3.14159265f * 440.f * t) + std::sin (2.f * 3.14159265f * 554.4f * t + 1.f) + std::sin (2.f * 3.14159265f * 659.3f * t + 2.f));
        const float hat = ((i % (beat / 2)) < 2000 ? 0.05f : 0.0f) * u (rng);
        s.L[(size_t) i] = 0.45f * kick + bass + chord + hat + 0.01f * u (rng);
        s.R[(size_t) i] = 0.45f * kick + bass + 0.9f * chord + hat + 0.01f * u (rng);
    }
    return s;
}

static void run (Engine& e, Sig& s, int block, const std::vector<float>& knobPerSample, Engine::Transport tr = {})
{
    const int n = (int) s.L.size();
    for (int off = 0; off < n; off += block)
    {
        const int m = std::min (block, n - off);
        e.process (s.L.data() + off, s.R.data() + off, m, knobPerSample[(size_t) off], tr);
    }
}

int main()
{
#if defined(__x86_64__)
    _mm_setcsr (_mm_getcsr() | 0x8040); // FTZ / DAZ, comme ScopedNoDenormals
#endif
    const float fs = 48000.f;

    // 1. Transparence bit à bit à 0 %
    std::printf ("[1] transparence à 0 %% (8 presets)\n");
    for (int p = 0; p < kNumPresets; ++p)
    {
        Engine e; e.prepare (fs); e.setPresetImmediate (p);
        Sig in = makeMusic ((int) fs * 2, fs, 3), s = in;
        run (e, s, 512, std::vector<float> (s.L.size(), 0.0f));
        const int lat = e.latencySamples(); int bad = 0;
        for (size_t i = (size_t) lat; i < s.L.size(); ++i)
            if (s.L[i] != in.L[i - (size_t) lat] || s.R[i] != in.R[i - (size_t) lat]) ++bad;
        CHECK (bad == 0, "preset %s : %d échantillons différents", presetName (p), bad);
    }

    // 2. Montée 0 → 100 %, retour à 0 : retour à la transparence exacte après les queues
    std::printf ("[2] montée puis retour à 0 : transparence retrouvée\n");
    for (int p = 0; p < kNumPresets; ++p)
    {
        Engine e; e.prepare (fs); e.setPresetImmediate (p);
        const int n = (int) fs * 20;
        Sig in = makeMusic (n, fs, 5), s = in;
        std::vector<float> k ((size_t) n, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / fs;
            k[(size_t) i] = t < 6.f ? t / 6.f : (t < 7.f ? 1.f : 0.f);
        }
        run (e, s, 256, k);
        bool finite = true; float peak = 0;
        for (size_t i = 0; i < s.L.size(); ++i) { finite &= std::isfinite (s.L[i]) && std::isfinite (s.R[i]); peak = std::max (peak, std::max (std::abs (s.L[i]), std::abs (s.R[i]))); }
        CHECK (finite, "preset %s : NaN/inf", presetName (p));
        CHECK (peak < 4.0f, "preset %s : crête %.2f", presetName (p), peak);
        CHECK (e.isIdle(), "preset %s : pas revenu au repos", presetName (p));
        const int lat = e.latencySamples(); int bad = 0;
        for (size_t i = (size_t) (n - (int) fs); i < (size_t) n; ++i)
            if (s.L[i] != in.L[i - (size_t) lat] || s.R[i] != in.R[i - (size_t) lat]) ++bad;
        CHECK (bad == 0, "preset %s : %d échantillons ≠ entrée dans la dernière seconde", presetName (p), bad);
        std::printf ("    %-12s crête %.2f\n", presetName (p), peak);
    }

    // 3. Entrée en jeu progressive : de 0 à 3 % en 3 s, l'écart avec le signal sec reste minuscule
    std::printf ("[3] entrée en jeu sans à-coup\n");
    for (int p = 0; p < kNumPresets; ++p)
    {
        Engine e; e.prepare (fs); e.setPresetImmediate (p);
        const int n = (int) fs * 3;
        Sig in = makeMusic (n, fs, 7), s = in;
        std::vector<float> k ((size_t) n);
        for (int i = 0; i < n; ++i) k[(size_t) i] = 0.03f * (float) i / (float) n;
        run (e, s, 128, k);
        const int lat = e.latencySamples();
        float maxDiff = 0, maxJump = 0, prev = 0, diff05 = 0;
        for (int i = lat; i < n; ++i)
        {
            const float d = s.L[(size_t) i] - in.L[(size_t) (i - lat)];
            maxDiff = std::max (maxDiff, std::abs (d));
            if (i > lat) maxJump = std::max (maxJump, std::abs (d - prev));
            prev = d;
            if (i < (int) (fs * 0.5f)) diff05 = std::max (diff05, std::abs (d));
        }
        std::printf ("    %-12s écart max %.4f (0,5 s : %.5f), saut max de l'écart %.4f\n", presetName (p), maxDiff, diff05, maxJump);
        CHECK (diff05 < 0.01f, "preset %s : écart déjà %.4f après 0,5 s", presetName (p), diff05);
        CHECK (maxJump < 0.05f, "preset %s : saut %.4f", presetName (p), maxJump);
    }

    // 4. Indépendance vis-à-vis de la taille des blocs
    std::printf ("[4] indépendance de la taille de bloc\n");
    for (int p = 0; p < kNumPresets; ++p)
    {
        const int n = (int) fs * 4;
        Sig a = makeMusic (n, fs, 9), b = a;
        Engine e1, e2; e1.prepare (fs); e2.prepare (fs); e1.setPresetImmediate (p); e2.setPresetImmediate (p);
        std::vector<float> k ((size_t) n, 0.72f);
        run (e1, a, 512, k);
        std::mt19937 rng (p); std::uniform_int_distribution<int> bs (1, 700);
        for (int off = 0; off < n;)
        {
            const int m = std::min (bs (rng), n - off);
            e2.process (b.L.data() + off, b.R.data() + off, m, 0.72f, {});
            off += m;
        }
        int bad = 0;
        for (int i = 0; i < n; ++i) if (a.L[(size_t) i] != b.L[(size_t) i] || a.R[(size_t) i] != b.R[(size_t) i]) ++bad;
        CHECK (bad == 0, "preset %s : %d échantillons différents", presetName (p), bad);
    }

    // 5. Changement de preset en pleine montée : pas de saut
    std::printf ("[5] changement de preset pendant la lecture\n");
    {
        Engine e; e.prepare (fs); e.setPresetImmediate (0);
        const int n = (int) fs * 8;
        Sig s; s.L.resize ((size_t) n); s.R.resize ((size_t) n);
        for (int i = 0; i < n; ++i) s.L[(size_t) i] = s.R[(size_t) i] = 0.3f * std::sin (2.f * 3.14159265f * 220.f * (float) i / fs);
        float maxStep = 0, prevL = 0;
        for (int off = 0, blk = 0; off < n; off += 512, ++blk)
        {
            if (blk % 47 == 0) e.setPreset ((blk / 47) % kNumPresets);
            e.process (s.L.data() + off, s.R.data() + off, 512, 0.6f, {});
        }
        bool finite = true;
        for (int i = 1; i < n; ++i) { finite &= std::isfinite (s.L[(size_t) i]); maxStep = std::max (maxStep, std::abs (s.L[(size_t) i] - prevL)); prevL = s.L[(size_t) i]; }
        CHECK (finite, "NaN pendant les changements de preset");
        std::printf ("    variation max d'un échantillon à l'autre : %.3f\n", maxStep);
    }

    // 6. Gain match : à 100 %, sonie de sortie ≈ sonie d'entrée
    std::printf ("[6] gain match (écart de sonie pondérée K, sortie − entrée, à 100 %%)\n");
    for (int p = 0; p < kNumPresets; ++p)
    {
        Engine e; e.prepare (fs); e.setPresetImmediate (p);
        const int n = (int) fs * 10;
        Sig in = makeMusic (n, fs, 11), s = in;
        run (e, s, 512, std::vector<float> ((size_t) n, 1.0f));
        Biquad a0, a1, b0, b1, c0, c1, d0, d1; makeKWeighting (fs, a0, a1); b0 = c0 = d0 = a0; b1 = c1 = d1 = a1;
        double pi = 0, po = 0;
        for (int i = 0; i < n; ++i)
        {
            const float x = a1.tick (a0.tick (in.L[(size_t) i])), y = b1.tick (b0.tick (in.R[(size_t) i]));
            const float u = c1.tick (c0.tick (s.L[(size_t) i])), v = d1.tick (d0.tick (s.R[(size_t) i]));
            if (i > n / 2) { pi += x * x + y * y; po += u * u + v * v; }
        }
        const double diff = 10.0 * std::log10 (po / pi);
        std::printf ("    %-12s %+5.1f dB (gain appliqué %+5.1f dB)\n", presetName (p), diff, e.currentGmGainDb());
        CHECK (std::abs (diff) < 2.5, "preset %s : écart %.1f dB", presetName (p), diff);
    }

    // 7. Transport en lecture (grille de l'hôte, boucle)
    std::printf ("[7] grille de l'hôte, saut de boucle\n");
    {
        Engine e; e.prepare (fs); e.setPresetImmediate (5);
        const int n = (int) fs * 6;
        Sig s = makeMusic (n, fs, 13);
        double ppq = 0; bool finite = true;
        for (int off = 0; off < n; off += 512)
        {
            Engine::Transport tr; tr.playing = true; tr.hasPpq = true; tr.bpm = 140; tr.ppq = ppq;
            e.process (s.L.data() + off, s.R.data() + off, std::min (512, n - off), 0.9f, tr);
            ppq += 512.0 / fs * 140.0 / 60.0;
            if (ppq > 8.0) ppq -= 8.0; // boucle de 2 mesures
        }
        for (float x : s.L) finite &= std::isfinite (x);
        CHECK (finite, "NaN avec transport");
    }

    // 8. Charge processeur
    std::printf ("[8] charge processeur (48 kHz stéréo, blocs de 512, 1 cœur)\n");
    {
        const int n = (int) fs * 20;
        Sig base = makeMusic (n, fs, 17);
        for (int p = 0; p < kNumPresets; ++p)
        {
            std::printf ("    %-12s", presetName (p));
            for (float kv : { 0.0f, 0.5f, 1.0f })
            {
                Engine e; e.prepare (fs); e.setPresetImmediate (p);
                Sig s = base;
                auto t0 = std::chrono::high_resolution_clock::now();
                run (e, s, 512, std::vector<float> ((size_t) n, kv));
                auto t1 = std::chrono::high_resolution_clock::now();
                const double sec = std::chrono::duration<double> (t1 - t0).count();
                std::printf ("  %3.0f %% : %5.2f %%", kv * 100.f, 100.0 * sec / 20.0);
            }
            std::printf ("\n");
        }
    }

    std::printf (failures ? "\n%d ÉCHEC(S)\n" : "\nTOUT EST OK\n", failures);
    return failures ? 1 : 0;
}
