// Offline test of the whole audio path, without a host. Prints what it
// measures, and fails when the unit stops behaving as its front panel says.
#include "dsp/Opto.h"
#include "TestSignal.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <vector>

namespace
{
constexpr double kSr = 48000.0;
constexpr int kBlock = 256;

int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    failures += ok ? 0 : 1;
}

double db (double x) { return 20.0 * std::log10 (std::max (x, 1.0e-12)); }

/** A sine at a level in dB from 0 VU (its peaks at -18 dBFS). */
std::vector<float> tone (double seconds, double hz, double vu, double rate = kSr)
{
    return mdxtest::makeTone (seconds, rate, hz, (float) (obx::kZeroVu * std::pow (10.0, vu / 20.0)));
}

/** Plays a signal through, on two channels, block by block; calls back after each block. */
std::vector<float> play (obx::Compressor& comp, const std::vector<float>& x,
                         const std::function<void (size_t end)>& afterBlock = {},
                         const std::vector<float>* key = nullptr)
{
    std::vector<float> left (x), right (x);
    std::vector<float> keyL, keyR;

    if (key != nullptr)
        keyL = keyR = *key;

    for (size_t at = 0; at < x.size(); at += kBlock)
    {
        const auto n = (int) std::min<size_t> (kBlock, x.size() - at);
        float* ch[2] = { left.data() + at, right.data() + at };
        const float* sc[2] = { key != nullptr ? keyL.data() + at : nullptr, key != nullptr ? keyR.data() + at : nullptr };

        comp.process (ch, ch, n, sc, key != nullptr ? 2 : 0);

        if (afterBlock)
            afterBlock (at + (size_t) n);
    }

    return left;
}

obx::Compressor make (const obx::Settings& s, double rate = kSr)
{
    obx::Compressor comp;
    comp.setSettings (s);
    comp.prepare (rate, 2);
    return comp;
}

/** The reduction once a steady tone has settled. */
float settledReduction (obx::Settings s, double vu, double hz = 1000.0, double rate = kSr)
{
    auto comp = make (s, rate);
    play (comp, tone (3.0, hz, vu, rate));
    return comp.reductionDb();
}

/** The level of each harmonic of a 1 kHz tone, relative to the fundamental. */
void harmonics (const std::vector<float>& y, double& second, double& third)
{
    const auto n = (size_t) kSr;
    const auto* tail = y.data() + y.size() - n;
    const auto f = mdxtest::toneLevel (tail, n, 1000.0, kSr);
    second = mdxtest::toneLevel (tail, n, 2000.0, kSr) / f;
    third = mdxtest::toneLevel (tail, n, 3000.0, kSr) / f;
}

obx::Settings clean()
{
    obx::Settings s;
    s.thresholdDb = obx::kThresholdOffDb;
    return s;
}
}

int main()
{
    using obx::Generation;
    using obx::Timing;

    std::printf ("Halfband\n");
    {
        // Up then down: a tone in the passband comes back at its level.
        obx::Halfband band;
        const auto x = mdxtest::makeTone (0.5, kSr, 1000.0, 0.5f);
        std::vector<float> y (x.size()), high (x.size() * 2);

        for (size_t i = 0; i < x.size(); ++i)
        {
            band.up (x[i], high[2 * i], high[2 * i + 1]);
            y[i] = band.down (high[2 * i], high[2 * i + 1]);
        }

        const auto through = mdxtest::toneLevel (y.data() + 4800, 19200, 1000.0, kSr) / 0.5;
        std::printf ("    1 kHz up and down: %+.4f dB\n", db (through));
        check (std::abs (db (through)) < 0.01, "the passband comes back as it went in");

        // The image of an upsampled 15 kHz tone, at 33 kHz.
        obx::Halfband up;
        const auto t = mdxtest::makeTone (0.5, kSr, 15000.0, 0.5f);
        std::vector<float> u (t.size() * 2);

        for (size_t i = 0; i < t.size(); ++i)
            up.up (t[i], u[2 * i], u[2 * i + 1]);

        const auto image = mdxtest::toneLevel (u.data() + 9600, 38400, 33000.0, 2 * kSr)
                         / mdxtest::toneLevel (u.data() + 9600, 38400, 15000.0, 2 * kSr);
        std::printf ("    image of 15 kHz going up: %.1f dB\n", db (image));
        check (db (image) < -90.0, "upsampling leaves no image");

        // A 30 kHz tone at the high rate does not fold back to 18 kHz.
        obx::Halfband down;
        const auto h = mdxtest::makeTone (0.5, 2 * kSr, 30000.0, 0.5f);
        std::vector<float> d (h.size() / 2);

        for (size_t i = 0; i < d.size(); ++i)
            d[i] = down.down (h[2 * i], h[2 * i + 1]);

        const auto alias = mdxtest::toneLevel (d.data() + 4800, 19200, 18000.0, kSr) / 0.5;
        std::printf ("    30 kHz folded to 18 kHz going down: %.1f dB\n", db (alias));
        check (db (alias) < -90.0, "downsampling lets nothing fold back");
    }

    std::printf ("\nThe threshold and the ratio\n");
    {
        obx::Settings s;   // threshold 0, ratio 4, manual, mk I

        for (auto generation : { Generation::vintage, Generation::modern })
        {
            s.generation = generation;
            const auto atThreshold = settledReduction (s, 0.0);
            std::printf ("    %s, a tone at the threshold: %.2f dB of reduction\n",
                         generation == Generation::vintage ? "mk I " : "mk II", atThreshold);
            check (std::abs (atThreshold - 1.0f) < 0.25f, "a tone at the threshold is brought down by 1 dB");
        }

        s.generation = Generation::vintage;

        for (float ratio : { 2.0f, 4.0f, 10.0f })
        {
            s.ratio = ratio;
            s.thresholdDb = -30.0f;
            const auto low = settledReduction (s, -10.0), high = settledReduction (s, 0.0);
            const auto expected = 10.0f * (1.0f - 1.0f / ratio);
            std::printf ("    ratio %.0f:1, 10 dB more in: %.2f dB more reduction (%.2f expected)\n", ratio, high - low, expected);
            check (std::abs ((high - low) - expected) < 0.35f, "far above the knee, the ratio is the ratio");
        }

        s = obx::Settings();
        s.thresholdDb = -20.0f;
        const auto deep = settledReduction (s, 0.0);
        s.thresholdDb = -40.0f;
        const auto deeper = settledReduction (s, 0.0);
        std::printf ("    0 VU, threshold -20: %.1f dB; -40: %.1f dB\n", deep, deeper);
        check (deep > 10.0f && deeper > deep + 10.0f, "lowering the threshold compresses more");

        check (settledReduction (clean(), 18.0) == 0.0f, "threshold on off: no reduction even at full scale");

        // The same at every rate the plugin can be run at.
        bool same = true;

        for (double rate : { 44100.0, 96000.0, 192000.0 })
        {
            const auto r = settledReduction (obx::Settings(), 0.0, 1000.0, rate);
            std::printf ("    at %.0f Hz: %.2f dB\n", rate, r);
            same = same && std::abs (r - 1.0f) < 0.25f;
        }

        check (same, "and at 44.1, 96 and 192 kHz");
    }

    std::printf ("\nAttack and release\n");
    {
        // How long the reduction takes to reach 63 % of where it settles.
        auto riseMs = [] (obx::Settings s)
        {
            const auto settled = settledReduction (s, 15.0);
            auto comp = make (s);
            const auto x = tone (1.0, 1000.0, 15.0);
            double found = -1.0;

            for (size_t at = 0; at < x.size() && found < 0.0; at += 48)
            {
                std::vector<float> piece (x.begin() + (long) at, x.begin() + (long) std::min (at + 48, x.size()));
                play (comp, piece);

                if (comp.reductionDb() >= 0.63f * settled)
                    found = (double) (at + 48) / kSr * 1000.0;
            }

            return found;
        };

        obx::Settings s;
        s.thresholdDb = 0.0f;
        s.attackMs = 10.0f;
        const auto fast = riseMs (s);
        s.attackMs = 100.0f;
        const auto slow = riseMs (s);
        s.timing = Timing::fixed;
        const auto fixed = riseMs (s);
        std::printf ("    to 63 %%: attack 10 ms -> %.1f ms, 100 ms -> %.1f ms, fixed -> %.1f ms\n", fast, slow, fixed);
        check (fast > 5.0 && fast < 20.0, "a 10 ms attack takes about 10 ms");
        check (slow > 70.0 && slow < 140.0, "a 100 ms attack takes about 100 ms");
        check (fixed > 0.0 && fixed < 6.0, "the fixed attack is a few ms at most");

        // The cell remembers: after long, heavy compression it lets go more
        // slowly than after a short burst, and the old cell more than the new.
        auto residual = [] (Generation generation, double seconds)
        {
            obx::Settings s;
            s.generation = generation;
            s.timing = Timing::fixed;
            s.thresholdDb = -10.0f;
            s.ratio = 10.0f;
            auto comp = make (s);
            auto x = tone (seconds, 1000.0, 6.0);
            x.resize (x.size() + (size_t) (0.4 * kSr), 0.0f);
            const auto peakAt = (size_t) (seconds * kSr);
            float peak = 0.0f, after = 0.0f;
            play (comp, x, [&] (size_t end)
            {
                if (end <= peakAt)
                    peak = comp.reductionDb();
                if (end <= peakAt + (size_t) (0.4 * kSr))
                    after = comp.reductionDb();
            });
            return after / peak;
        };

        const auto shortI = residual (Generation::vintage, 0.1), longI = residual (Generation::vintage, 5.0);
        const auto shortII = residual (Generation::modern, 0.1), longII = residual (Generation::modern, 5.0);
        std::printf ("    left 400 ms after the end, fixed: mk I %.0f %% (burst) / %.0f %% (5 s); mk II %.0f %% / %.0f %%\n",
                     shortI * 100.0f, longI * 100.0f, shortII * 100.0f, longII * 100.0f);
        check (longI > shortI * 2.0f && longII > shortII * 2.0f, "a long compression is let go of more slowly than a burst");
        check (longI > longII, "and more slowly by the mk I than by the mk II");
        check (shortII < 0.08f, "a burst is let go of quickly");

        // FIX./MAN.: a short peak is caught and released fast; a long one
        // is held as on MANUAL.
        auto afterPeak = [] (Timing timing, double seconds, float& peak)
        {
            obx::Settings s;
            s.timing = timing;
            s.attackMs = 150.0f;
            s.releaseMs = 4000.0f;
            s.thresholdDb = -10.0f;
            s.ratio = 10.0f;
            auto comp = make (s);
            auto x = tone (seconds, 1000.0, 6.0);
            const auto end = x.size();
            x.resize (end + (size_t) (0.3 * kSr), 0.0f);
            float after = 0.0f;
            peak = 0.0f;
            play (comp, x, [&] (size_t at)
            {
                if (at <= end)
                    peak = std::max (peak, comp.reductionDb());
                if (at <= end + (size_t) (0.3 * kSr))
                    after = comp.reductionDb();
            });
            return after;
        };

        float peakShortFm, peakShortMan, peakLongFm, peakLongMan;
        const auto shortFm = afterPeak (Timing::fixedManual, 0.01, peakShortFm);
        afterPeak (Timing::manual, 0.01, peakShortMan);
        const auto longFm = afterPeak (Timing::fixedManual, 1.5, peakLongFm);
        const auto longMan = afterPeak (Timing::manual, 1.5, peakLongMan);
        std::printf ("    a 10 ms peak: fix./man. catches %.1f dB, manual %.1f dB; 300 ms later fix./man. holds %.1f dB\n",
                     peakShortFm, peakShortMan, shortFm);
        std::printf ("    a 1.5 s note: 300 ms after it, fix./man. %.1f dB, manual %.1f dB\n", longFm, longMan);
        check (peakShortFm > 3.0f * peakShortMan, "fix./man. catches a peak the manual attack lets through");
        check (shortFm < 0.3f * peakShortFm, "and lets go of it quickly");
        check (std::abs (longFm - longMan) < 0.15f * longMan && longMan > 0.7f * peakLongMan,
               "a long note is held as on manual");
    }

    std::printf ("\nThe valve and the transformers\n");
    {
        double secondI, thirdI, secondII, thirdII;
        auto s = clean();
        s.generation = Generation::vintage;
        auto comp = make (s);
        harmonics (play (comp, tone (2.0, 1000.0, 0.0)), secondI, thirdI);
        s.generation = Generation::modern;
        comp = make (s);
        harmonics (play (comp, tone (2.0, 1000.0, 0.0)), secondII, thirdII);
        std::printf ("    0 VU: mk I 2nd %.1f dB 3rd %.1f dB; mk II 2nd %.1f dB 3rd %.1f dB\n",
                     db (secondI), db (thirdI), db (secondII), db (thirdII));
        check (secondI > secondII * 2.0, "the mk I's valve has more second harmonic");
        check (secondI < 0.005 && thirdI < 0.005, "and both stay clean at 0 VU");

        double hotSecond, hotThird;
        s.generation = Generation::vintage;
        s.outputDb = 15.0f;
        comp = make (s);
        harmonics (play (comp, tone (2.0, 1000.0, 0.0)), hotSecond, hotThird);
        std::printf ("    mk I driven 15 dB over: 2nd %.1f dB, 3rd %.1f dB\n", db (hotSecond), db (hotThird));
        check (hotSecond > secondI * 3.0 && hotThird > thirdI * 3.0, "driving the output stage brings the harmonics up");

        // The response, quiet and uncompressed.
        auto response = [] (Generation generation, double hz)
        {
            auto s = clean();
            s.generation = generation;
            auto comp = make (s);
            const auto y = play (comp, tone (2.0, hz, -10.0));
            const auto n = (size_t) kSr;
            return db (mdxtest::toneLevel (y.data() + y.size() - n, n, hz, kSr) / (obx::kZeroVu * std::pow (10.0, -0.5)));
        };

        for (auto generation : { Generation::vintage, Generation::modern })
        {
            const double r20 = response (generation, 20.0), r60 = response (generation, 60.0), r1k = response (generation, 1000.0),
                         r10k = response (generation, 10000.0), r20k = response (generation, 20000.0);
            std::printf ("    %s response: 20 Hz %+.2f, 60 Hz %+.2f, 1 kHz %+.2f, 10 kHz %+.2f, 20 kHz %+.2f dB\n",
                         generation == Generation::vintage ? "mk I " : "mk II", r20, r60, r1k, r10k, r20k);
            check (std::abs (r1k) < 0.1 && r20 > -1.5 && r20k > -1.6 && r60 < 0.8, "flat within the band, give or take its colour");
        }

        check (response (Generation::vintage, 20000.0) < response (Generation::modern, 20000.0) - 0.3,
               "the mk I is softer at the top");
    }

    std::printf ("\nAround it\n");
    {
        // All dry: no compression, whatever the reduction.
        obx::Settings s;
        s.thresholdDb = -40.0f;
        s.parallel = 0.0f;
        auto comp = make (s);
        const auto y = play (comp, tone (1.0, 1000.0, 0.0));
        const auto n = (size_t) kSr / 2;
        const auto dry = db (mdxtest::toneLevel (y.data() + y.size() - n, n, 1000.0, kSr) / obx::kZeroVu);
        std::printf ("    all dry with 40 dB of threshold: %+.3f dB\n", dry);
        check (std::abs (dry) < 0.02 && comp.reductionDb() > 20.0f, "parallel at dry lets the input through as it is");

        // Bypassed, what comes out is exactly what went in.
        s = obx::Settings();
        s.thresholdDb = -30.0f;
        s.outputDb = 12.0f;
        s.bypass = true;
        comp = make (s);
        const auto x = mdxtest::makeVoice (kSr, 1.0, 180.0);
        const auto b = play (comp, x);
        double worst = 0.0;

        for (size_t i = 0; i < x.size(); ++i)
            worst = std::max (worst, (double) std::abs (b[i] - x[i]));

        check (worst < 1.0e-6, "bypassed, the input comes out untouched");

        // The external sidechain.
        s = obx::Settings();
        s.thresholdDb = -10.0f;
        s.externalSidechain = true;
        comp = make (s);
        play (comp, tone (1.0, 1000.0, 6.0));
        const auto nothing = comp.reductionDb();
        const auto key = tone (1.0, 1000.0, 6.0);
        comp = make (s);
        play (comp, tone (1.0, 1000.0, -30.0), {}, &key);
        const auto keyed = comp.reductionDb();
        std::printf ("    external: nothing plugged in %.2f dB, a loud key on a quiet input %.1f dB\n", nothing, keyed);
        check (nothing == 0.0f && keyed > 5.0f, "on external, only the key compresses");

        // The low cut keeps the lows out of the detector, not out of the sound.
        s = obx::Settings();
        s.thresholdDb = -10.0f;
        const auto open = settledReduction (s, 6.0, 50.0);
        s.lowCut = 1;
        const auto at80 = settledReduction (s, 6.0, 50.0);
        s.lowCut = 2;
        const auto at220 = settledReduction (s, 6.0, 50.0);
        std::printf ("    a 50 Hz tone: %.1f dB of reduction, %.1f with the 80 Hz cut, %.1f with 220 Hz\n", open, at80, at220);
        check (open > at80 + 2.0f && at80 > at220 + 4.0f, "the low cut makes the lows count less");

        // Output off.
        s = obx::Settings();
        s.outputDb = obx::kOutputOffDb;
        comp = make (s);
        const auto quiet = play (comp, tone (0.5, 1000.0, 0.0));
        check (mdxtest::rmsDb (quiet.data() + quiet.size() / 2, quiet.size() / 2) < -120.0, "output on off is silence");
    }

    std::printf ("\nThe meter\n");
    {
        auto s = clean();
        s.meter = obx::Meter::input;
        auto comp = make (s);
        float highest = 0.0f, at300 = 0.0f;
        play (comp, tone (1.5, 1000.0, 0.0), [&] (size_t end)
        {
            highest = std::max (highest, comp.needle());
            if (end <= (size_t) (0.3 * kSr))
                at300 = comp.needle();
        });
        std::printf ("    0 VU sine: %.3f after 300 ms, %.3f at most, %.3f settled\n", at300, highest, comp.needle());
        check (std::abs (comp.needle() - 1.0f) < 0.02f, "a 0 VU sine reads 0 VU");
        check (at300 > 0.95f && highest < 1.03f, "with a VU's rise and overshoot");

        s = obx::Settings();
        s.thresholdDb = -10.0f;
        s.meter = obx::Meter::compression;
        comp = make (s);
        play (comp, tone (3.0, 1000.0, 0.0));
        const auto shownDb = db (comp.needle());
        std::printf ("    compression: the needle shows %.1f dB, the reduction is %.1f dB\n", shownDb, comp.reductionDb());
        check (std::abs (-shownDb - comp.reductionDb()) < 0.3, "on compression the needle falls by the reduction");
    }

    std::printf ("\nNothing blows up\n");
    {
        const auto loud = mdxtest::makeNoise (1.0, kSr, 1.0f);
        double highest = 0.0;
        bool finite = true;

        for (auto timing : { Timing::fixed, Timing::fixedManual, Timing::manual })
            for (auto generation : { Generation::vintage, Generation::modern })
                for (float out : { 0.0f, 30.0f })
                {
                    obx::Settings s;
                    s.timing = timing;
                    s.generation = generation;
                    s.outputDb = out;
                    s.attackMs = 0.5f;
                    s.releaseMs = 50.0f;
                    auto comp = make (s);
                    const auto y = play (comp, loud);

                    for (auto v : y)
                    {
                        finite = finite && std::isfinite (v);
                        highest = std::max (highest, (double) std::abs (v));
                    }
                }

        std::printf ("    full-scale noise, every mode, up to +30 dB of gain: peaks at %.1f dBFS\n", db (highest));
        check (finite && highest < 16.0, "everything stays finite and bounded");
    }

    std::printf (failures == 0 ? "\nall passed\n" : "\n%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
