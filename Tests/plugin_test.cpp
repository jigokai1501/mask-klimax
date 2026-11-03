// Test du plugin (processeur JUCE + éditeur rendu hors écran).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(c, ...) do { if (! (c)) { ++failures; std::printf ("  ÉCHEC : " __VA_ARGS__); std::printf ("\n"); } } while (0)

static void setParam (KlimaxProcessor& p, const char* id, float plainValue)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());

    // 1. Dispositions de bus
    {
        KlimaxProcessor p;
        using CS = juce::AudioChannelSet;
        auto ok = [&p] (CS i, CS o) { juce::AudioProcessor::BusesLayout l; l.inputBuses.add (i); l.outputBuses.add (o); return p.checkBusesLayoutSupported (l); };
        CHECK (ok (CS::stereo(), CS::stereo()), "stéréo→stéréo refusé");
        CHECK (ok (CS::mono(), CS::stereo()),   "mono→stéréo refusé");
        CHECK (ok (CS::mono(), CS::mono()),     "mono→mono refusé");
        CHECK (! ok (CS::stereo(), CS::mono()), "stéréo→mono accepté");
        std::printf ("[1] dispositions OK\n");
    }

    // 2. Traitement stéréo et mono, transparence à 0 %, programmes, état
    {
        KlimaxProcessor p;
        p.setPlayConfigDetails (2, 2, 44100.0, 512);
        p.prepareToPlay (44100.0, 512);
        const int lat = p.getLatencySamples();
        std::printf ("[2] latence : %d échantillons (%.1f ms à 44,1 kHz)\n", lat, 1000.0 * lat / 44100.0);
        CHECK (lat > 0 && lat < 2048, "latence %d", lat);

        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
        std::vector<float> in; juce::Random r (1); int bad = 0, total = 0;
        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < 512; ++i) { const float x = r.nextFloat() - 0.5f; in.push_back (x); buf.setSample (0, i, x); buf.setSample (1, i, x * 0.5f); }
            p.processBlock (buf, midi);
            for (int i = 0; i < 512; ++i)
            {
                const int gi = b * 512 + i - lat;
                if (gi >= 0) { ++total; if (std::memcmp (&in[(size_t) gi], &(const float&) buf.getReadPointer (0)[i], sizeof (float)) != 0 || std::abs (buf.getSample (1, i) - in[(size_t) gi] * 0.5f) > 0.0f) ++bad; }
            }
        }
        CHECK (bad == 0, "%d / %d échantillons ≠ à 0 %%", bad, total);

        // montée à 100 % sur tous les presets via les programmes
        float peak = 0; bool finite = true;
        for (int prog = 0; prog < p.getNumPrograms(); ++prog)
        {
            p.setCurrentProgram (prog);
            CHECK (p.getCurrentProgram() == prog, "programme %d non sélectionné", prog);
            for (int b = 0; b < 120; ++b)
            {
                setParam (p, "klimax", (float) b / 119.0f * 100.0f);
                for (int i = 0; i < 512; ++i) { const float x = 0.3f * std::sin ((float) (b * 512 + i) * 0.03f) + 0.1f * (r.nextFloat() - 0.5f); buf.setSample (0, i, x); buf.setSample (1, i, x); }
                p.processBlock (buf, midi);
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) { const float y = buf.getSample (c, i); finite &= std::isfinite (y); peak = std::max (peak, std::abs (y)); }
            }
        }
        CHECK (finite, "NaN/inf");
        std::printf ("    8 programmes parcourus, crête %.2f\n", peak);

        // état
        setParam (p, "klimax", 63.0f); p.setCurrentProgram (6);
        juce::MemoryBlock st; p.getStateInformation (st);
        KlimaxProcessor q; q.setStateInformation (st.getData(), (int) st.getSize());
        CHECK (std::abs (q.apvts.getRawParameterValue ("klimax")->load() - 63.0f) < 0.01f, "état : klimax non restauré");
        CHECK (q.getCurrentProgram() == 6, "état : preset non restauré");
        std::printf ("    état sauvegardé / restauré, programme %s\n", q.getProgramName (q.getCurrentProgram()).toRawUTF8());

        // mono → mono
        KlimaxProcessor m;
        juce::AudioProcessor::BusesLayout l; l.inputBuses.add (juce::AudioChannelSet::mono()); l.outputBuses.add (juce::AudioChannelSet::mono());
        CHECK (m.setBusesLayout (l), "mono non appliqué");
        m.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> mb (1, 1024);
        setParam (m, "klimax", 80.0f);
        bool mf = true;
        for (int b = 0; b < 100; ++b) { for (int i = 0; i < 1024; ++i) mb.setSample (0, i, 0.3f * std::sin ((float) (b * 1024 + i) * 0.02f)); m.processBlock (mb, midi); for (int i = 0; i < 1024; ++i) mf &= std::isfinite (mb.getSample (0, i)); }
        CHECK (mf, "mono : NaN");
        std::printf ("    mono→mono OK (blocs plus grands que prévu)\n");
    }

    // 3. Éditeur : captures
    {
        KlimaxProcessor p;
        p.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        std::printf ("[3] éditeur %d × %d\n", ed->getWidth(), ed->getHeight());
        struct Shot { float v; int preset; const char* name; };
        for (auto s : { Shot { 0.0f, 1, "klimax_0" }, Shot { 42.0f, 1, "klimax_42" }, Shot { 100.0f, 7, "klimax_100" } })
        {
            setParam (p, "klimax", s.v); p.setCurrentProgram (s.preset);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            juce::File f = outDir.getChildFile (juce::String (s.name) + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            std::printf ("    capture %s\n", f.getFullPathName().toRawUTF8());
        }

        // interaction : clic sur un preset
        auto* pill = dynamic_cast<PresetPill*> (ed->getChildComponent (2 + 4));
        if (pill != nullptr) { pill->triggerClick(); juce::MessageManager::getInstance()->runDispatchLoopUntil (50); }
        CHECK (pill != nullptr && p.getCurrentProgram() == pill->index, "clic sur un preset sans effet");
        ed.reset();
    }

    std::printf (failures ? "\n%d ÉCHEC(S)\n" : "\nTOUT EST OK\n", failures);
    return failures ? 1 : 0;
}
