// Standalone emitter API microbenchmark. No game headers or game linkage.
// Fixture strings are emitted only into this executable by the gold-v2 generator.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#include "particle_emitter_bench_fixture.h"

using fixture::Name;
using fixture::Range;
constexpr int Rounds = 9;
constexpr double TargetSampleNs = 4'000'000.0;
constexpr DWORD_PTR RequestedAffinity = 0x4000;
volatile std::uint64_t gSink = 0;

struct Color {
    std::int32_t red{}, green{}, blue{}, alpha{};
    bool operator==(const Color&) const = default;
};
static_assert(sizeof(Color) == 16);
struct System {
    const fixture::Emitter* defs{};
    std::uint32_t count{}, begin{};
    std::array<std::int32_t, 113> first{};
};
struct Query {
    const char* text{}; // nullptr is the historical All selector.
    std::size_t prefixLength{};
    Name id{Name::unnamed};
    Range range{fixture::All};
};
using SelectKernel = std::uint32_t (*)(const System&, Color*, const Query&, const Color&);
using FindKernel = std::int32_t (*)(const System&, const Query&);

// Each comparison is a distinct noinline entry, called through an opaque pointer.
// Shared layout/traversal intentionally isolates API predicates from roster changes.
__declspec(noinline) std::uint32_t OldExact(const System& s, Color* out, const Query& q, const Color& c) {
    std::uint32_t hits = 0;
    for (std::uint32_t i = 0; i < s.count; ++i) {
        if (q.text == nullptr || _stricmp(q.text, s.defs[i].name) == 0) {
            out[i] = c;
            ++hits;
        }
    }
    return hits;
}
__declspec(noinline) std::uint32_t OldExactFast(const System& s, Color* out, const Query& q, const Color& c) {
    const bool all = q.text == nullptr;
    const bool unnamed = !all && q.text[0] == '\0';
    std::uint32_t hits = 0;
    for (std::uint32_t i = 0; i < s.count; ++i) {
        if (all || (unnamed ? s.defs[i].name[0] == '\0' : _stricmp(q.text, s.defs[i].name) == 0)) {
            out[i] = c;
            ++hits;
        }
    }
    return hits;
}
__declspec(noinline) std::uint32_t NumericExact(const System& s, Color* out, const Query& q, const Color& c) {
    std::uint32_t hits = 0;
    for (std::uint32_t i = 0; i < s.count; ++i) {
        if (s.defs[i].id == q.id) {
            out[i] = c;
            ++hits;
        }
    }
    return hits;
}
__declspec(noinline) std::uint32_t NumericRange(const System& s, Color* out, const Query& q, const Color& c) {
    std::uint32_t hits = 0;
    for (std::uint32_t i = 0; i < s.count; ++i) {
        if (q.range.Contains(s.defs[i].id)) {
            out[i] = c;
            ++hits;
        }
    }
    return hits;
}
__declspec(noinline) std::uint32_t OldPrefix(const System& s, Color* out, const Query& q, const Color& c) {
    // Same length guard + case-insensitive comparator as preserved prefix scans.
    const auto length = std::strlen(q.text);
    std::uint32_t hits = 0;
    for (std::uint32_t i = 0; i < s.count; ++i) {
        if (std::strlen(s.defs[i].name) >= length && _strnicmp(s.defs[i].name, q.text, length) == 0) {
            out[i] = c;
            ++hits;
        }
    }
    return hits;
}
__declspec(noinline) std::uint32_t OldPrefixCachedLength(const System& s, Color* out, const Query& q, const Color& c) {
    // Useful lower-overhead baseline; _strnicmp already stops at a null terminator.
    std::uint32_t hits = 0;
    for (std::uint32_t i = 0; i < s.count; ++i) {
        if (_strnicmp(s.defs[i].name, q.text, q.prefixLength) == 0) {
            out[i] = c;
            ++hits;
        }
    }
    return hits;
}
__declspec(noinline) std::int32_t FindString(const System& s, const Query& q) {
    for (std::uint32_t i = 0; i < s.count; ++i)
        if (_stricmp(q.text, s.defs[i].name) == 0) return static_cast<std::int32_t>(i);
    return -1;
}
__declspec(noinline) std::int32_t FindNumeric(const System& s, const Query& q) {
    for (std::uint32_t i = 0; i < s.count; ++i)
        if (s.defs[i].id == q.id) return static_cast<std::int32_t>(i);
    return -1;
}
__declspec(noinline) std::int32_t FindDirect(const System& s, const Query& q) {
    const auto id = static_cast<std::uint32_t>(q.id);
    return id < s.first.size() ? s.first[id] : -1;
}

struct Call {
    std::uint32_t system{};
    Query query{};
    std::uint64_t expectedMask{}; // Independent raw-string oracle from generator.
};
struct Work {
    std::string name, category, baseline, candidate;
    std::vector<Call> calls;
    SelectKernel oldSelect{}, newSelect{};
    FindKernel oldFind{}, newFind{};
    bool lookup{};
    std::uint64_t definitionsPerPass{}, hitsPerPass{}, lookupHits{};
    std::uint32_t minimumCount{std::numeric_limits<std::uint32_t>::max()}, maximumCount{};
};
struct Counters {
    std::uint64_t scans{}, selections{}, lookups{}, sequenceChecks{}, boundaryChecks{};
    std::uint64_t runtimeInstanceScans{}, rangeFirstDefinitionChecks{};
};
struct Sample {
    double nsPerScan{};
    std::int64_t ticks{};
    std::uint64_t scans{}, hits{}, checksum{};
};
struct Result {
    Work work;
    std::uint64_t sweeps{};
    std::array<Sample, Rounds> oldSamples{}, newSamples{};
    double oldMedian{}, newMedian{}, deltaPercent{}, pairedDeltaPercent{};
};

std::int64_t Qpc() {
    LARGE_INTEGER v{};
    if (!QueryPerformanceCounter(&v)) throw std::runtime_error("QueryPerformanceCounter failed");
    return v.QuadPart;
}
Color MakeColor(std::uint32_t phase) {
    // Changes for every selection scan, independently of the chosen kernel.
    const auto x = phase * 1664525u + 1013904223u;
    return {static_cast<std::int32_t>((x >> 0) & 255), static_cast<std::int32_t>((x >> 8) & 255),
            static_cast<std::int32_t>((x >> 16) & 255), static_cast<std::int32_t>(128u | (x >> 25))};
}
Color InitialColor(std::uint32_t i) {
    return {-1000 - static_cast<std::int32_t>(i), -2000, -3000, -4000};
}
std::vector<Color> InitialState() {
    std::vector<Color> out(fixture::Emitters.size());
    for (std::uint32_t i = 0; i < out.size(); ++i) out[i] = InitialColor(i);
    return out;
}
std::uint64_t Checksum(const std::vector<Color>& out) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto& c : out) {
        for (auto v : {c.red, c.green, c.blue, c.alpha}) {
            hash ^= static_cast<std::uint32_t>(v);
            hash *= 1099511628211ull;
        }
    }
    return hash;
}
std::int32_t First(std::uint64_t mask) {
    return mask ? static_cast<std::int32_t>(std::countr_zero(mask)) : -1;
}
void Need(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
std::vector<System> MakeSystems() {
    std::vector<System> systems;
    for (const auto& src : fixture::Systems) {
        System s{fixture::Emitters.data() + src.begin, src.count, src.begin, {}};
        s.first.fill(-1);
        for (std::uint32_t i = 0; i < s.count; ++i) {
            const auto id = static_cast<std::uint32_t>(s.defs[i].id);
            Need(id < s.first.size(), "Out-of-contract fixture ID");
            if (s.first[id] < 0) s.first[id] = static_cast<std::int32_t>(i);
        }
        systems.push_back(s);
    }
    return systems;
}
Query NamedQuery(std::uint32_t id) {
    return {fixture::Names[id].text, 0, static_cast<Name>(id), fixture::All};
}
Query FamilyQuery(std::uint32_t family) {
    const auto& f = fixture::Families[family];
    return {f.prefix, std::strlen(f.prefix), Name::unnamed, f.range};
}
Call NamedCall(std::uint32_t system, std::uint32_t id) {
    return {system, NamedQuery(id), fixture::ExactMasks[system * fixture::Names.size() + id]};
}
Call FamilyCall(std::uint32_t system, std::uint32_t family) {
    return {system, FamilyQuery(family), fixture::FamilyMasks[system * fixture::Families.size() + family]};
}
void FinishWork(Work& w, const std::vector<System>& systems) {
    Need(!w.calls.empty(), "Empty workload " + w.name);
    // Deterministic permutation avoids grouping hit/miss cases without reordering definitions.
    std::uint32_t rng = 0x7f4a7c15u;
    for (std::size_t i = w.calls.size(); i > 1; --i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        std::swap(w.calls[i - 1], w.calls[rng % i]);
    }
    for (const auto& c : w.calls) {
        const auto n = systems[c.system].count;
        w.definitionsPerPass += n;
        w.hitsPerPass += std::popcount(c.expectedMask);
        w.lookupHits += c.expectedMask != 0;
        w.minimumCount = std::min(w.minimumCount, n);
        w.maximumCount = std::max(w.maximumCount, n);
    }
}
Work SelectWork(std::string name, std::string baseline, SelectKernel oldKernel, SelectKernel newKernel) {
    Work w;
    w.name = std::move(name); w.category = "selection_color_write";
    w.baseline = std::move(baseline);
    w.candidate = newKernel == NumericExact ? "enum_exact" : "half_open_id_range";
    w.oldSelect = oldKernel; w.newSelect = newKernel;
    return w;
}
std::vector<Work> MakeWorkloads(const std::vector<System>& systems) {
    std::vector<Work> works;
    // Primary tiny systems are weighted once per call; never treat 222 emitters as one large system.
    for (bool tiny : {true, false}) {
        const std::string suffix = tiny ? "tiny_1_10" : "full_corpus";
        for (bool fast : {false, true}) {
            auto w = SelectWork("unnamed_" + (fast ? std::string("fast_") : std::string()) + suffix,
                                fast ? "empty_name_first_byte_fast_path" : "empty_name_stricmp",
                                fast ? OldExactFast : OldExact, NumericExact);
            for (std::uint32_t s = 0; s < systems.size(); ++s)
                if (!tiny || systems[s].count <= 10) w.calls.push_back(NamedCall(s, 0));
            works.push_back(std::move(w));
        }
        auto all = SelectWork("all_" + suffix, "nullptr_all", OldExact, NumericRange);
        auto named = SelectWork("named_exact_" + suffix, "stricmp_exact", OldExact, NumericExact);
        auto prefix = SelectWork("family_prefix_" + suffix, "strlen_guard_strnicmp", OldPrefix, NumericRange);
        for (std::uint32_t s = 0; s < systems.size(); ++s) {
            if (tiny && systems[s].count > 10) continue;
            all.calls.push_back({s, {nullptr, 0, Name::unnamed, fixture::All}, (1ull << systems[s].count) - 1});
            std::vector<std::uint32_t> present;
            for (std::uint32_t id = 1; id < fixture::Names.size(); ++id)
                if (NamedCall(s, id).expectedMask) present.push_back(id);
            if (present.empty()) {
                named.calls.push_back(NamedCall(s, 1)); // Named query on an unnamed-only system.
            } else {
                for (auto id : present) named.calls.push_back(NamedCall(s, id));
                std::uint32_t absent = 1;
                while (NamedCall(s, absent).expectedMask) ++absent;
                // Equal per-system hit/miss counts, retaining realistic definition sizes.
                for (std::size_t i = 0; i < present.size(); ++i) named.calls.push_back(NamedCall(s, absent));
            }
            std::vector<std::uint32_t> families;
            for (std::uint32_t f = 0; f < fixture::Families.size(); ++f)
                if (FamilyCall(s, f).expectedMask) families.push_back(f);
            if (families.empty()) {
                prefix.calls.push_back(FamilyCall(s, 0));
            } else {
                std::uint32_t absent = 0;
                while (FamilyCall(s, absent).expectedMask) ++absent;
                for (auto f : families) {
                    prefix.calls.push_back(FamilyCall(s, f));
                    prefix.calls.push_back(FamilyCall(s, absent));
                }
            }
        }
        works.push_back(std::move(all)); works.push_back(std::move(named)); works.push_back(std::move(prefix));
    }
    // Also report prefix comparison with cached query length and no redundant name strlen.
    auto cached = works[4];
    cached.name = "family_prefix_cached_tiny_1_10";
    cached.baseline = "cached_length_strnicmp"; cached.oldSelect = OldPrefixCachedLength;
    works.push_back(std::move(cached));
    auto findSystem = [](const char* file) {
        for (std::uint32_t i = 0; i < fixture::Systems.size(); ++i)
            if (std::strcmp(fixture::Systems[i].file, file) == 0) return i;
        throw std::runtime_error("Missing representative system");
    };
    for (const char* family : {"gloom_", "icetrap", "awardray", "doommushroom"}) {
        std::uint32_t f = 0;
        while (f < fixture::Families.size() && std::strcmp(fixture::Families[f].prefix, family)) ++f;
        Need(f < fixture::Families.size(), "Missing representative family");
        const char* file = std::strcmp(family, "gloom_") == 0 ? "GloomCloud.xml.v2.compiled" :
                           std::strcmp(family, "icetrap") == 0 ? "IceTrapZombie.xml.v2.compiled" :
                           std::strcmp(family, "awardray") == 0 ? "Award.xml.v2.compiled" : "Doom.xml.v2.compiled";
        auto w = SelectWork(std::string("family_") + family + "_own_system", "strlen_guard_strnicmp", OldPrefix, NumericRange);
        w.calls.push_back(FamilyCall(findSystem(file), f)); works.push_back(std::move(w));
    }
    for (bool tiny : {true, false}) {
        const std::string suffix = tiny ? "tiny_1_10" : "full_corpus";
        std::vector<Call> calls;
        for (std::uint32_t s = 0; s < systems.size(); ++s) {
            if (tiny && systems[s].count > 10) continue;
            std::vector<std::uint32_t> present;
            for (std::uint32_t id = 0; id < fixture::Names.size(); ++id)
                if (NamedCall(s, id).expectedMask) present.push_back(id);
            std::uint32_t absent = 1;
            while (NamedCall(s, absent).expectedMask) ++absent;
            for (auto id : present) {
                calls.push_back(NamedCall(s, id)); calls.push_back(NamedCall(s, absent));
            }
        }
        for (bool direct : {false, true}) {
            Work w;
            w.name = (direct ? "find_direct_" : "find_scan_") + suffix;
            w.category = "definition_find_only"; w.baseline = "stricmp_first_definition_scan";
            w.candidate = direct ? "prebuilt_id_first_definition_table" : "enum_first_definition_scan";
            w.lookup = true; w.oldFind = FindString; w.newFind = direct ? FindDirect : FindNumeric;
            w.calls = calls; works.push_back(std::move(w));
        }
    }
    for (bool hitsOnly : {true, false}) {
        Work w;
        w.name = hitsOnly ? "find_fadeout_present_systems" : "find_fadeout_full_corpus";
        w.category = "definition_find_only"; w.baseline = "stricmp_first_definition_scan";
        w.candidate = "prebuilt_id_first_definition_table";
        w.lookup = true; w.oldFind = FindString; w.newFind = FindDirect;
        for (std::uint32_t s = 0; s < systems.size(); ++s) {
            const auto call = NamedCall(s, static_cast<std::uint32_t>(Name::fadeout));
            if (!hitsOnly || call.expectedMask) w.calls.push_back(call);
        }
        works.push_back(std::move(w));
    }
    for (auto& w : works) FinishWork(w, systems);
    return works;
}

Counters Validate(const std::vector<System>& systems, const std::vector<Work>& works) {
    Counters count;
    // Exhaustive raw-name oracle checks, including every absent name/family in every system.
    for (std::uint32_t s = 0; s < systems.size(); ++s) {
        const auto& sys = systems[s];
        auto checkSelect = [&](const Call& call, SelectKernel kernel) {
            std::vector<Color> out(sys.count), expected(sys.count);
            for (std::uint32_t i = 0; i < sys.count; ++i) out[i] = expected[i] = InitialColor(i);
            const auto color = MakeColor(17);
            for (std::uint32_t i = 0; i < sys.count; ++i)
                if (call.expectedMask & (1ull << i)) expected[i] = color;
            Need(kernel(sys, out.data(), call.query, color) == static_cast<std::uint32_t>(std::popcount(call.expectedMask)), "Selection hit mismatch");
            Need(out == expected, "Selection output mismatch in " + std::string(fixture::Systems[s].file));
            ++count.scans; count.selections += std::popcount(call.expectedMask);
        };
        for (std::uint32_t id = 0; id < fixture::Names.size(); ++id) {
            const auto call = NamedCall(s, id);
            for (auto kernel : {OldExact, OldExactFast, NumericExact}) checkSelect(call, kernel);
            for (auto kernel : {FindString, FindNumeric, FindDirect}) {
                Need(kernel(sys, call.query) == First(call.expectedMask), "First-definition lookup mismatch");
                ++count.lookups;
            }
        }
        for (std::uint32_t f = 0; f < fixture::Families.size(); ++f) {
            const auto call = FamilyCall(s, f);
            for (auto kernel : {OldPrefix, OldPrefixCachedLength, NumericRange}) checkSelect(call, kernel);
        }
        Call all{s, {nullptr, 0, Name::unnamed, fixture::All}, (1ull << sys.count) - 1};
        for (auto kernel : {OldExact, OldExactFast, NumericRange}) checkSelect(all, kernel);
    }
    Need(fixture::All.Contains(Name::unnamed) && fixture::All.Contains(static_cast<Name>(112)) &&
         !fixture::All.Contains(Name::top), "All range endpoint mismatch");
    count.boundaryChecks += 3;
    for (const auto& f : fixture::Families) {
        Need(f.range.Contains(f.range.mBegin), "Family begin must be inclusive");
        Need(!f.range.Contains(f.range.mEnd), "Family end must be exclusive");
        Need(!f.range.Contains(Name::unnamed), "Families must exclude unnamed");
        count.boundaryChecks += 3;
    }
    // A runtime roster can contain multiple instances of a definition. All matching
    // instances must receive the write; a definition's first-index table cannot replace this scan.
    for (std::uint32_t s = 0; s < systems.size(); ++s) {
        const auto& definitions = systems[s];
        std::vector<fixture::Emitter> instances;
        for (std::uint32_t pass = 0; pass < 2; ++pass)
            for (std::uint32_t i = 0; i < definitions.count; ++i) instances.push_back(definitions.defs[i]);
        System roster{instances.data(), static_cast<std::uint32_t>(instances.size()), 0, {}};
        auto checkInstances = [&](const Call& call, SelectKernel oldKernel, SelectKernel numericKernel) {
            const auto mask = call.expectedMask | (call.expectedMask << definitions.count);
            std::vector<Color> a(instances.size()), b(instances.size()), expected(instances.size());
            for (std::uint32_t i = 0; i < instances.size(); ++i) a[i] = b[i] = expected[i] = InitialColor(i);
            const auto color = MakeColor(31);
            for (std::uint32_t i = 0; i < instances.size(); ++i)
                if (mask & (1ull << i)) expected[i] = color;
            const auto hits = static_cast<std::uint32_t>(std::popcount(mask));
            Need(oldKernel(roster, a.data(), call.query, color) == hits &&
                 numericKernel(roster, b.data(), call.query, color) == hits,
                 "Runtime-instance hit mismatch");
            Need(a == expected && b == expected, "Runtime-instance color output mismatch");
            ++count.runtimeInstanceScans;
        };
        checkInstances(NamedCall(s, 0), OldExact, NumericExact);
        checkInstances(NamedCall(s, 0), OldExactFast, NumericExact);
        for (std::uint32_t f = 0; f < fixture::Families.size(); ++f) {
            const auto call = FamilyCall(s, f);
            checkInstances(call, OldPrefix, NumericRange);
            // Definition range chooses the first declaration-order hit, not the smallest ID.
            std::int32_t numericFirst = -1;
            for (std::uint32_t i = 0; i < definitions.count; ++i)
                if (call.query.range.Contains(definitions.defs[i].id)) {
                    numericFirst = static_cast<std::int32_t>(i); break;
                }
            Need(numericFirst == First(call.expectedMask), "Range first-definition order mismatch");
            if (numericFirst >= 0) {
                auto exact = NamedQuery(static_cast<std::uint32_t>(definitions.defs[numericFirst].id));
                Need(FindDirect(definitions, exact) == numericFirst, "Range then direct-definition mismatch");
            }
            ++count.rangeFirstDefinitionChecks;
        }
    }
    // First matching definition wins even for duplicate unnamed emitters in the gold corpus.
    for (const auto& w : works) {
        if (w.lookup) continue;
        auto oldOut = InitialState(), newOut = InitialState(), expected = InitialState();
        std::uint32_t phase = 0;
        for (int pass = 0; pass < 3; ++pass) {
            for (const auto& call : w.calls) {
                const auto& sys = systems[call.system];
                const auto color = MakeColor(++phase);
                const auto n = static_cast<std::uint32_t>(std::popcount(call.expectedMask));
                Need(w.oldSelect(sys, oldOut.data() + sys.begin, call.query, color) == n, "Old sequence hits mismatch");
                Need(w.newSelect(sys, newOut.data() + sys.begin, call.query, color) == n, "Numeric sequence hits mismatch");
                for (std::uint32_t i = 0; i < sys.count; ++i)
                    if (call.expectedMask & (1ull << i)) expected[sys.begin + i] = color;
            }
            Need(oldOut == expected && newOut == expected, "Changing-color sequence output mismatch");
            Need(Checksum(oldOut) == Checksum(newOut), "Changing-color checksum mismatch");
            ++count.sequenceChecks;
        }
    }
    return count;
}

__declspec(noinline) std::uint64_t RunSelect(const Work& w, const std::vector<System>& systems,
                                          std::vector<Color>& out, SelectKernel kernel,
                                          std::uint64_t sweeps) {
    std::uint64_t hits = 0;
    std::uint32_t phase = 0;
    for (std::uint64_t pass = 0; pass < sweeps; ++pass) {
        for (const auto& call : w.calls) {
            const auto& sys = systems[call.system];
            const auto color = MakeColor(++phase);
            hits += kernel(sys, out.data() + sys.begin, call.query, color);
        }
    }
    return hits;
}
struct LookupBatch { std::uint64_t hits{}, checksum{}; };
__declspec(noinline) LookupBatch RunFind(const Work& w, const std::vector<System>& systems,
                                      FindKernel kernel, std::uint64_t sweeps) {
    LookupBatch result;
    std::uint64_t phase = 0;
    for (std::uint64_t pass = 0; pass < sweeps; ++pass) {
        for (const auto& call : w.calls) {
            const auto index = kernel(systems[call.system], call.query);
            result.hits += index >= 0;
            // Consume every result and change the contribution for every call.
            result.checksum += (static_cast<std::uint64_t>(index + 2) * ++phase) ^ 0x9e3779b97f4a7c15ull;
        }
    }
    return result;
}
Sample Measure(const Work& w, const std::vector<System>& systems, bool old, std::uint64_t sweeps,
               double nsPerTick) {
    Sample sample;
    sample.scans = sweeps * w.calls.size();
    auto out = InitialState(); // Outside timed region.
    std::uint64_t lookupChecksum = 0;
    _ReadWriteBarrier();
    const auto start = Qpc();
    if (w.lookup) {
        const auto result = RunFind(w, systems, old ? w.oldFind : w.newFind, sweeps);
        sample.hits = result.hits; lookupChecksum = result.checksum;
    } else {
        sample.hits = RunSelect(w, systems, out, old ? w.oldSelect : w.newSelect, sweeps);
    }
    _ReadWriteBarrier();
    const auto finish = Qpc();
    sample.ticks = finish - start;
    sample.nsPerScan = sample.ticks * nsPerTick / static_cast<double>(sample.scans);
    sample.checksum = w.lookup ? lookupChecksum : Checksum(out); // Outside timed region.
    gSink = gSink ^ sample.checksum ^ sample.hits;
    return sample;
}
std::uint64_t Calibrate(const Work& w, const std::vector<System>& systems, double nsPerTick) {
    std::uint64_t sweeps = 1;
    while (sweeps * w.calls.size() < 4096) sweeps *= 2;
    auto a = Measure(w, systems, true, sweeps, nsPerTick);
    auto b = Measure(w, systems, false, sweeps, nsPerTick);
    double fastest = std::min(a.nsPerScan, b.nsPerScan);
    Need(fastest > 0, "Calibration timer resolution too low");
    const double wanted = TargetSampleNs / (fastest * w.calls.size());
    // The fastest member receives about 4 ms, the slower at most about 100 ms.
    const double cap = 100'000'000.0 / (std::max(a.nsPerScan, b.nsPerScan) * w.calls.size());
    sweeps = static_cast<std::uint64_t>(std::max(1.0, std::min(wanted, cap)));
    return sweeps;
}
double Median(std::vector<double> values) {
    std::sort(values.begin(), values.end()); return values[values.size() / 2];
}
std::string JsonQuote(const std::string& value) {
    std::string out = "\"";
    for (unsigned char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: Need(c >= 32 && c < 127, "Non-ASCII JSON metadata"); out += static_cast<char>(c); break;
        }
    }
    return out + '"';
}
std::string CpuBrand() {
    std::array<int, 4> words{};
    __cpuid(words.data(), static_cast<int>(0x80000000u));
    if (static_cast<std::uint32_t>(words[0]) < 0x80000004u) return "unknown";
    std::array<char, 49> brand{};
    for (int leaf = 0; leaf < 3; ++leaf) {
        __cpuid(words.data(), static_cast<int>(0x80000002u) + leaf);
        std::memcpy(brand.data() + 16 * leaf, words.data(), 16);
    }
    return brand.data();
}
void WriteReports(const std::filesystem::path& directory, const std::vector<Result>& results,
                  const Counters& validation, std::int64_t frequency, double elapsed,
                  DWORD_PTR affinity, DWORD observedCpu, const std::string& cpu) {
    std::ofstream csv(directory / "samples.csv"), summary(directory / "summary.csv"), json(directory / "results.json");
    Need(csv && summary && json, "Cannot open result files");
    csv << "case,round,order,api,scans,total_qpc_ticks,ns_per_scan,hits,checksum\n" << std::fixed << std::setprecision(6);
    summary << "case,category,baseline,candidate,calls_per_pass,min_definitions,max_definitions,average_definitions,hits_per_pass,lookup_hits_per_pass,old_median_ns,new_median_ns,new_change_percent,paired_median_change_percent,speedup\n" << std::fixed << std::setprecision(6);
    json << std::fixed << std::setprecision(6)
         << "{\n  \"status\": \"passed\",\n  \"compiler\": \"MSVC " << _MSC_FULL_VER
         << "\",\n  \"language\": \"C++23\",\n  \"build_flags\": \"/O2 /Ob2 /DNDEBUG /GL- /fp:precise /std:c++23preview; /OPT:NOICF\",\n"
         << "  \"cpu\": " << JsonQuote(cpu) << ",\n  \"affinity_requested\": \"0x4000\",\n  \"affinity_effective\": " << affinity
         << ",\n  \"logical_processor_observed\": " << observedCpu
         << ",\n  \"qpc_frequency_hz\": " << frequency << ",\n  \"rounds\": " << Rounds
         << ",\n  \"round_order\": \"AB,BA,AB,BA,AB,BA,AB,BA,AB; same executable and fixtures\",\n"
         << "  \"target_fast_sample_ms\": 4,\n  \"benchmark_wall_seconds\": " << elapsed
         << ",\n  \"corpus_sha256\": " << JsonQuote(fixture::CorpusSha256)
         << ",\n  \"name_table_crc32\": " << fixture::NameTableCrc32
         << ",\n  \"corpus_systems\": " << fixture::Systems.size() << ",\n  \"corpus_definitions\": " << fixture::Emitters.size()
         << ",\n  \"validation\": {\"selection_scans\": " << validation.scans
         << ", \"selected_outputs_checked\": " << validation.selections
         << ", \"definition_lookups\": " << validation.lookups
         << ", \"changing_color_sequences\": " << validation.sequenceChecks
         << ", \"range_endpoint_checks\": " << validation.boundaryChecks
         << ", \"runtime_instance_scans\": " << validation.runtimeInstanceScans
         << ", \"range_first_definition_checks\": " << validation.rangeFirstDefinitionChecks << "},\n"
         << "  \"limitations\": [\n"
         << "    \"Hot-cache standalone predicate/scan and 16-byte color-write model, common compact layout and traversal; not the full game roster or allocation path.\",\n"
         << "    \"One selection scan means one query over one original corpus system, not one 222-definition global scan.\",\n"
         << "    \"Call mixtures are explicitly constructed corpus workloads, not measured gameplay frequency. Tiny exact/family cases include hit and absent-query calls.\",\n"
         << "    \"Includes common indirect call, changing-color generation, and returned hit-count accumulation overhead; output hashing is outside timing.\",\n"
         << "    \"Unnamed stricmp and first-byte fast baselines are separate; nullptr-All already avoids string comparison.\",\n"
         << "    \"Prefix baseline is preserved strlen guard plus strnicmp; cached-length strnicmp is reported separately for sensitivity.\",\n"
         << "    \"FIND cases measure first-definition lookup only, including hit and miss; no particle spawn, crossfade duration calculation or fade execution.\",\n"
         << "    \"Measures the same first-definition-table strategy used by exact CrossFade; allocation/execution excluded. Table is 113 ints per system definition (452 bytes), first declaration-order match; construction and invalidation are outside timing.\",\n"
         << "    \"Runtime-instance unnamed and family selections still scan every live instance; the definition table cannot replace those operations. Timed model has one instance per definition; duplicate-instance semantics are validated separately.\",\n"
         << "    \"FadeOut present-system case queries only the five 2-definition corpus systems containing FadeOut (first index 1); full-corpus FadeOut is a synthetic absence-heavy workload.\",\n"
         << "    \"Single pinned local run under normal Windows scheduling; small ns differences may reflect noise, CPU frequency and background activity. No FPS claim.\"\n"
         << "  ],\n  \"cases\": [\n";
    for (std::size_t k = 0; k < results.size(); ++k) {
        const auto& r = results[k]; const auto& w = r.work;
        const double avg = static_cast<double>(w.definitionsPerPass) / w.calls.size();
        summary << w.name << ',' << w.category << ',' << w.baseline << ',' << w.candidate << ','
                << w.calls.size() << ',' << w.minimumCount << ',' << w.maximumCount << ',' << avg << ','
                << w.hitsPerPass << ',' << w.lookupHits << ',' << r.oldMedian << ',' << r.newMedian << ','
                << r.deltaPercent << ',' << r.pairedDeltaPercent << ',' << r.oldMedian / r.newMedian << '\n';
        json << "    {\"case\": " << JsonQuote(w.name) << ", \"category\": " << JsonQuote(w.category)
             << ", \"baseline\": " << JsonQuote(w.baseline) << ", \"candidate\": " << JsonQuote(w.candidate)
             << ", \"calls_per_pass\": " << w.calls.size() << ", \"sweeps_per_sample\": " << r.sweeps
             << ", \"min_definitions\": " << w.minimumCount << ", \"max_definitions\": " << w.maximumCount
             << ", \"average_definitions\": " << avg << ", \"selection_hits_per_pass\": " << w.hitsPerPass
             << ", \"lookup_hits_per_pass\": " << w.lookupHits << ", \"old_median_ns_per_scan\": " << r.oldMedian
             << ", \"new_median_ns_per_scan\": " << r.newMedian << ", \"new_change_percent\": " << r.deltaPercent
             << ", \"paired_median_change_percent\": " << r.pairedDeltaPercent
             << ", \"speedup\": " << r.oldMedian / r.newMedian << ", \"samples\": [";
        for (int round = 0; round < Rounds; ++round) {
            const auto& a = r.oldSamples[round]; const auto& b = r.newSamples[round];
            const auto order = round % 2 ? "BA" : "AB";
            for (bool old : {true, false}) {
                const auto& v = old ? a : b;
                csv << w.name << ',' << round + 1 << ',' << order << ',' << (old ? "A_old" : "B_new") << ','
                    << v.scans << ',' << v.ticks << ',' << v.nsPerScan << ',' << v.hits << ',' << v.checksum << '\n';
            }
            if (round) json << ',';
            json << "{\"round\": " << round + 1 << ", \"order\": " << JsonQuote(order)
                 << ", \"scans\": " << a.scans << ", \"old_ns_per_scan\": " << a.nsPerScan
                 << ", \"new_ns_per_scan\": " << b.nsPerScan << ", \"old_hits\": " << a.hits
                 << ", \"new_hits\": " << b.hits << ", \"old_checksum\": " << a.checksum
                 << ", \"new_checksum\": " << b.checksum << '}';
        }
        json << "]}" << (k + 1 < results.size() ? ",\n" : "\n");
    }
    json << "  ],\n  \"volatile_sink\": " << gSink << "\n}\n";
}

int main(int argc, char** argv) try {
    Need(argc == 2 || (argc == 3 && std::strcmp(argv[2], "--fadeout-only") == 0),
         "Usage: bench_particle_emitter_names.exe <existing-output-directory> [--fadeout-only]");
    const bool fadeoutOnly = argc == 3;
    const std::filesystem::path output(argv[1]);
    Need(std::filesystem::is_directory(output), "Output directory does not exist");
    const auto wallStart = std::chrono::steady_clock::now();
    Need(SetProcessAffinityMask(GetCurrentProcess(), RequestedAffinity) != 0,
         "Cannot pin process to requested affinity mask 0x4000");
    Need(SetThreadAffinityMask(GetCurrentThread(), RequestedAffinity) != 0, "Cannot pin benchmark thread");
    DWORD_PTR processAffinity{}, systemAffinity{};
    Need(GetProcessAffinityMask(GetCurrentProcess(), &processAffinity, &systemAffinity) != 0 &&
         processAffinity == RequestedAffinity, "Effective process affinity differs from 0x4000");
    const auto observedCpu = GetCurrentProcessorNumber();
    Need(observedCpu == 14, "Benchmark is not running on LP14");
    LARGE_INTEGER frequency{};
    Need(QueryPerformanceFrequency(&frequency) != 0, "No QPC frequency");
    const double nsPerTick = 1e9 / frequency.QuadPart;
    const auto cpu = CpuBrand();
    const auto systems = MakeSystems();
    auto works = MakeWorkloads(systems);
    const auto validation = Validate(systems, works);
    if (fadeoutOnly) {
        std::erase_if(works, [](const Work& w) { return w.name.rfind("find_fadeout_", 0) != 0; });
        Need(works.size() == 2, "Expected two scoped FadeOut cases");
    }
    std::cout << "Validation passed: " << validation.scans << " color scans, " << validation.lookups
              << " definition lookups, " << validation.sequenceChecks << " changing-color sequences, "
              << validation.runtimeInstanceScans << " duplicate-instance scans and "
              << validation.rangeFirstDefinitionChecks << " declaration-order range lookups.\n"
              << "Pinned process and benchmark thread to 0x4000 (LP" << observedCpu << "). CPU: " << cpu << '\n';
    std::vector<Result> results;
    for (auto& work : works) {
        Result r; r.work = std::move(work);
        r.sweeps = Calibrate(r.work, systems, nsPerTick);
        // Warm both at the actual workload size before any reported timing.
        Measure(r.work, systems, true, r.sweeps, nsPerTick);
        Measure(r.work, systems, false, r.sweeps, nsPerTick);
        results.push_back(std::move(r));
    }
    // One executable, global rounds with counterbalanced A/B order.
    for (int round = 0; round < Rounds; ++round) {
        for (auto& r : results) {
            for (int order = 0; order < 2; ++order) {
                const bool old = (order == 0) == (round % 2 == 0);
                const auto sample = Measure(r.work, systems, old, r.sweeps, nsPerTick);
                (old ? r.oldSamples : r.newSamples)[round] = sample;
            }
            const auto& a = r.oldSamples[round]; const auto& b = r.newSamples[round];
            const auto expectedHits = r.sweeps * (r.work.lookup ? r.work.lookupHits : r.work.hitsPerPass);
            Need(a.hits == expectedHits && b.hits == expectedHits, "Timed sample hits disagree with gold oracle");
            Need(a.checksum == b.checksum, "Timed old/new output checksums differ");
            Need(GetCurrentProcessorNumber() == 14, "Affinity changed during timed rounds");
            Need(std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count() < 14.5,
                 "Timing budget reached before 9-round completion");
        }
    }
    std::cout << std::fixed << std::setprecision(3);
    for (auto& r : results) {
        std::vector<double> a, b, paired;
        for (int round = 0; round < Rounds; ++round) {
            a.push_back(r.oldSamples[round].nsPerScan); b.push_back(r.newSamples[round].nsPerScan);
            paired.push_back(100.0 * (b.back() / a.back() - 1.0));
        }
        r.oldMedian = Median(a); r.newMedian = Median(b);
        r.deltaPercent = 100.0 * (r.newMedian / r.oldMedian - 1.0);
        r.pairedDeltaPercent = Median(paired);
        std::cout << std::setw(40) << std::left << r.work.name << std::right
                  << " A " << std::setw(9) << r.oldMedian << " ns  B " << std::setw(9) << r.newMedian
                  << " ns  change " << std::setw(8) << r.deltaPercent << "%\n";
    }
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count();
    WriteReports(output, results, validation, frequency.QuadPart, elapsed, processAffinity, observedCpu, cpu);
    std::cout << "Benchmark wall time " << elapsed << " seconds; " << Rounds << " rounds. Results: "
              << output.string() << '\n';
    return 0;
} catch (const std::exception& e) {
    std::cerr << "Benchmark failed: " << e.what() << '\n'; return 1;
}
