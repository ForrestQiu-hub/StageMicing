#include "Model.h"
#include "Catalog.h"
#include "Draw.h"
#include "L10n.h"

#include <cmath>
#include <map>

namespace {

bool IsMicId(const std::wstring& id) {
    for (const auto& m : Mics()) if (id == m.id) return true;
    return false;
}

int Add(Project& pr, const wchar_t* def, double x, double y,
        const std::wstring& label, double angle = 0.0) {
    Item it;
    it.id = pr.nextId++;
    it.defId = def;
    it.kind = IsMicId(def) ? ItemKind::Mic : ItemKind::Instrument;
    it.x = x;
    it.y = y;
    it.angle = angle;
    it.label = label;
    if (const CatItem* c = FindItem(def)) { it.w = c->w * CatScale(def); it.h = c->h * CatScale(def); }
    pr.items.push_back(it);
    return pr.items.back().id;
}

} // namespace

// ---------------------------------------------------------------------------
// Merge OLD intensity-difference pairs (two mic items in a kind-2 MicGroup) into
// the ONE stage object the current model uses.  Called by the project loader, so
// every path that parses a .smproj — 打开项目, an undo snapshot, a preset — sees
// the same single object.
// ---------------------------------------------------------------------------
void MergeIntensityPairs(Project& pr) {
    for (size_t gi = 0; gi < pr.micGroups.size(); ++gi) {
        const MicGroup g = pr.micGroups[gi];
        if (g.kind != 2 || !IsIntensityTech(g.tech)) continue;
        if (g.ids.size() != 2) continue;
        Item* a = pr.Find(g.ids[0]);
        Item* b = pr.Find(g.ids[1]);
        if (!a || !b) continue;
        const int keepId = a->id;
        const int dropId = b->id;
        Item m = *a;
        m.defId = g.tech;                    // the technique IS the object
        m.kind = ItemKind::Mic;
        // position = the midpoint of the two members, angle = their mean angle
        const double ang = (a->angle + b->angle) * 0.5;
        m.x = (a->x + b->x) * 0.5;
        m.y = (a->y + b->y) * 0.5;
        m.angle = ang;
        // the two 型号 texts become the L / R (or M / S) model columns
        m.modelL = a->model.empty() ? a->modelL : a->model;
        m.modelR = b->model.empty() ? b->modelR : b->model;
        m.model.clear();
        if (m.spotType.empty()) m.spotType = b->spotType;
        if (m.height <= 0.0) m.height = b->height;
        if (const CatItem* c = FindItem(g.tech)) {
            m.w = c->w * CatScale(g.tech);
            m.h = c->h * CatScale(g.tech);
        }
        // strip the old 「… L / … R」 suffix the split used to write
        std::wstring lab = m.label;
        for (const wchar_t* suf : {L" L", L" R", L" M", L" S"}) {
            const std::wstring sfx(suf);
            const size_t n = sfx.size();
            if (lab.size() > n && lab.compare(lab.size() - n, n, sfx) == 0) {
                lab.resize(lab.size() - n);
                break;
            }
        }
        if (!lab.empty()) m.label = lab;
        *a = m;
        // re-point a cable that fed the second member at the merged object, then
        // drop the duplicate pair a merge can create (same two objects, twice)
        for (auto& ln : pr.links) {
            if (ln.aKind == 1 && ln.a == dropId) ln.a = keepId;
            if (ln.bKind == 1 && ln.b == dropId) ln.b = keepId;
        }
        {
            std::vector<CableLink> uniq;
            for (const auto& ln : pr.links) {
                bool dup = false;
                for (const auto& o : uniq) {
                    const bool sameEnds =
                        (o.aKind == ln.aKind && o.a == ln.a && o.bKind == ln.bKind &&
                         o.b == ln.b) ||
                        (o.aKind == ln.bKind && o.a == ln.b && o.bKind == ln.aKind &&
                         o.b == ln.a);
                    if (sameEnds) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) uniq.push_back(ln);
            }
            pr.links.swap(uniq);
        }
        pr.items.erase(std::remove_if(pr.items.begin(), pr.items.end(),
                                      [&](const Item& q) { return q.id == dropId; }),
                       pr.items.end());
        pr.selIds.erase(std::remove(pr.selIds.begin(), pr.selIds.end(), dropId),
                        pr.selIds.end());
        if (pr.selected == dropId) pr.selected = keepId;
        pr.micGroups.erase(pr.micGroups.begin() + (long)gi);
        --gi;
    }
}

// ---------------------------------------------------------------------------
// The built-in demo scene (test fixture).
//
// Everything else was removed on purpose: the demo holds exactly one piano
// with a soprano (and her lavalier) in front of it, the main microphone on the
// downstage edge, a linked stereo spot pair in the centre of the stage and the
// cabling devices that only layer 03 draws.  See docs/CABLING.md.
// ---------------------------------------------------------------------------
// Startup state: an EMPTY project — a blank 18 x 12 m stage and nothing else.
// The demo content is still available through BuildDemoProject for fixtures.
void BuildEmptyProject(Project& pr) {
    pr.items.clear();
    pr.selected = -1;
    pr.selIds.clear();
    pr.micGroups.clear();
    pr.mainMics.clear();
    pr.nextId = 1;
    pr.devices.clear();
    pr.nextDeviceId = 1;
    pr.links.clear();
    pr.topo = RouteTopology{};
    pr.piece.clear();
    pr.venue.clear();
    pr.conductor.clear();
    pr.notes.clear();
    pr.channelOrder.clear();
    pr.stage.w = 18.0;
    pr.stage.d = 12.0;
}

void BuildDemoProject(Project& pr, bool zh) {
    pr.items.clear();
    pr.selected = -1;
    pr.selIds.clear();
    pr.micGroups.clear();
    pr.mainMics.clear();
    pr.nextId = 1;
    pr.devices.clear();
    pr.nextDeviceId = 1;
    pr.links.clear();
    pr.topo = RouteTopology{};

    if (zh) {
        pr.piece = L"勃拉姆斯 · 第四交响曲";
        pr.venue = L"维也纳 · 金色大厅";
        pr.conductor = L"——";
    } else {
        pr.piece = L"Brahms · Symphony No. 4";
        pr.venue = L"Musikverein, Vienna";
        pr.conductor = L"—";
    }

    // stage centre, the anchor of the whole fixture
    const double cx = pr.stage.w * 0.5;   // 7.0 m
    const double cy = pr.stage.d * 0.5;   // 5.0 m

    // 1. grand piano, horizontal, exactly in the middle of the stage
    Add(pr, L"grand_piano", cx, cy, zh ? L"三角钢琴" : L"Grand piano", 270);

    // 2. the solo singer 0.9 m downstage of the piano centre
    Add(pr, L"solo_singer", cx, cy + 0.9, zh ? L"独唱歌手" : L"Solo singer", 0);

    // 2b. lavalier on the soprano (5 cm away so both symbols stay readable)
    Add(pr, L"lavalier", cx, cy + 0.95, zh ? L"领夹话筒 1" : L"Lavalier 1", 0);

    // 3. main microphone on the middle of the downstage edge
    int mainId = Add(pr, L"mainmic", cx, pr.stage.d - 0.40,
                     zh ? L"主话筒" : L"Main mic", 0);
    if (Item* mm = pr.Find(mainId)) {
        mm->mainTech = 0;      // AB
        mm->pattern = 0;       // omni
        mm->spacing = 0.40;
        mm->center = 0.0;
        mm->w = 0.48;
        mm->h = 0.42;
    }
    pr.mainMics = {mainId};

    // 4. a stereo spot pair in the centre of the stage, bound as one 2-mic group
    {
        int a = Add(pr, L"mic", cx - 0.25, cy, zh ? L"点话筒 1" : L"Spot 1", 0);
        int b = Add(pr, L"mic", cx + 0.25, cy, zh ? L"点话筒 2" : L"Spot 2", 0);
        int pair[2] = {a, b};
        for (int id : pair)
            if (Item* m = pr.Find(id)) {
                m->spotType = L"grand_piano";   // spot mics on the piano
                m->model = L"KM 184";
                m->pattern = 1;                 // cardioid
            }
        MicGroup g;
        g.ids = {a, b};
        g.kind = 0;      // stereo group
        pr.micGroups.push_back(g);
    }

    // 5. cabling devices: layer 03 only
    {
        // 8-channel sound card, 5 m to the left of the stage's top-left corner
        Device sc;
        sc.id = pr.nextDeviceId++;
        sc.kind = DevKind::SoundCard;
        sc.x = -5.0;
        sc.y = -0.5;
        sc.label = zh ? L"声卡 1" : L"Audio Interface 1";
        sc.model = L"8ch";
        sc.channels = 8;
        sc.faceDir = 1;          // faces the stage (right)
        pr.devices.push_back(sc);

        // 4-channel 「接口箱 / Stagebox」 behind the stage (one box kind only)
        Device ab;
        ab.id = pr.nextDeviceId++;
        ab.kind = DevKind::AnalogBox;
        ab.x = pr.stage.w * 0.25;   // 3.5 m
        ab.y = pr.stage.d + 0.6;    // 10.6 m, 0.6 m behind the rear edge
        ab.label = zh ? L"接口箱 1" : L"Stagebox 1";
        ab.model = L"4ch";
        ab.channels = 4;
        ab.faceDir = 0;          // faces the stage (up)
        pr.devices.push_back(ab);

        // wireless receiver for the lavalier, stage left next to the sound card
        Device rx;
        rx.id = pr.nextDeviceId++;
        rx.kind = DevKind::Receiver;
        rx.x = -4.2;
        rx.y = 0.6;
        rx.label = zh ? L"无线接收器 1" : L"Wireless Receiver 1";
        rx.model = L"4ch";
        rx.channels = 4;
        rx.faceDir = 1;          // faces the stage (right)
        pr.devices.push_back(rx);
    }
}

// ---------------------------------------------------------------------------
// cable helpers
// ---------------------------------------------------------------------------
CableEnd ResolveCableEnd(const Project& pr, int kind, int id) {
    CableEnd e;
    if (kind == 1) {   // microphone (Item)
        for (const auto& it : pr.items) {
            if (it.id != id || it.kind != ItemKind::Mic) continue;
            e.ok = true;
            e.mic = true;
            e.kind = DevKind::Mic;
            e.id = it.id;
            e.x = it.x;
            e.y = it.y;
            e.name = it.label.empty() ? std::wstring(L10n::T(S::TypeMic)) : it.label;
            // one capsule is one channel; the main microphone's capsules (L/R = 2,
            // Decca L/C/R = 3) travel together on a single cable, so its endpoint
            // capacity is the capsule count — otherwise every main-mic cable would
            // look "over capacity"
            const bool mainMic = (it.defId == L"mainmic");
            e.capacity = mainMic ? (it.mainTech == 3 ? 3 : 2) : 1;
            e.required = e.capacity;
            return e;
        }
        return e;
    }
    for (const auto& d : pr.devices) {
        if (d.id != id) continue;
        e.ok = true;
        e.mic = false;
        e.kind = d.kind;
        e.id = d.id;
        e.x = d.x;
        e.y = d.y;
        e.name = d.label.empty() ? DeviceKindTitle(d.kind) : d.label;
        e.capacity = pr.DeviceCapacity(d);
        e.required = (std::max)(1, d.channels);
        return e;
    }
    return e;
}

double CablePolylineLen(const CableLink& ln) {
    double total = 0.0;
    for (size_t i = 1; i < ln.path.size(); ++i) {
        const double dx = ln.path[i].first - ln.path[i - 1].first;
        const double dy = ln.path[i].second - ln.path[i - 1].second;
        total += std::sqrt(dx * dx + dy * dy);
    }
    return total;
}

// ---------------------------------------------------------------------------
// cable length pipeline.  NO slack, NO end allowance, NO reserve: the length is
// what has to be ordered.  A microphone cable drops from the capsule to the floor
// (the mic's own height) and then follows the floor polyline; a flown one uses the
// four legs.  03 / 04 show that value rounded UP to the next standard size.
// ---------------------------------------------------------------------------
double CableMicHeight(const Project& pr, const CableLink& ln) {
    for (const auto& it : pr.items) {
        if (ln.aKind == 1 && it.id == ln.a) return (std::max)(0.0, it.height);
        if (ln.bKind == 1 && it.id == ln.b) return (std::max)(0.0, it.height);
    }
    return 0.0;   // a device → device trunk has no microphone height
}

double CollectorLen(const CableLink& ln) {
    double sum = 0.0;
    for (const auto& c : ln.collectorPaths) sum += PolyLen(c);
    return sum;
}

std::pair<double, double> GroupConvergencePos(const Project& pr, int itemId) {
    const std::vector<std::pair<double, double>> pts = GroupMemberPos(pr, itemId);
    if (pts.empty()) return {0.0, 0.0};
    if (pts.size() == 1) return pts[0];
    const int g = pr.GroupOf(itemId);
    const bool decca = (g >= 0 && pr.micGroups[(size_t)g].kind == 1);
    if (decca && pts.size() == 3) {
        // the CENTRE capsule is the member that sits closest to the midpoint of the
        // other two — it is the one protruding forward, so it is excluded from the
        // average and the convergence point lands on the L/R line
        int centre = -1;
        double bestD = 1e18;
        for (int i = 0; i < 3; ++i) {
            const auto& a = pts[(size_t)((i + 1) % 3)];
            const auto& b = pts[(size_t)((i + 2) % 3)];
            const auto& p = pts[(size_t)i];
            const double mx = (a.first + b.first) * 0.5;
            const double my = (a.second + b.second) * 0.5;
            const double dd = std::hypot(p.first - mx, p.second - my);
            if (dd < bestD) {
                bestD = dd;
                centre = i;
            }
        }
        if (centre >= 0) {
            double sx = 0.0, sy = 0.0;
            for (int i = 0; i < 3; ++i) {
                if (i == centre) continue;
                sx += pts[(size_t)i].first;
                sy += pts[(size_t)i].second;
            }
            return {sx * 0.5, sy * 0.5};
        }
    }
    double sx = 0.0, sy = 0.0;
    for (const auto& p : pts) {
        sx += p.first;
        sy += p.second;
    }
    return {sx / (double)pts.size(), sy / (double)pts.size()};
}

double CableRawLen(const Project& pr, const CableLink& ln) {
    // a bound group travels its collector stubs FIRST (member → convergence point)
    // and only then makes the one main run, so both parts are the cable
    const double collectors = CollectorLen(ln);
    if (ln.flown)
        return FlownRunLength(ln.legUp, ln.legCeil, ln.legWall, ln.legFloor) + collectors;
    if (ln.path.size() < 2) return collectors;
    return CableMicHeight(pr, ln) + CablePolylineLen(ln) + collectors;
}

double StandardCableLen(double raw) {
    static const double kStd[] = {5.0, 8.0, 10.0, 12.0, 15.0, 18.0, 20.0, 25.0, 30.0};
    if (raw <= kStd[0]) return kStd[0];
    for (double s : kStd)
        if (raw <= s + 1e-9) return s;
    return std::ceil(raw / 5.0) * 5.0;   // above 30 m: the next multiple of 5
}

double CableOverrideLen(const Project& pr, const CableLink& ln) {
    // a grouped microphone is wired ONCE, so any member's override belongs to that
    // one cable: the first non-zero override of the group wins
    auto ofItem = [&](int id) {
        for (const auto& it : pr.items)
            if (it.id == id) return (std::max)(0.0, it.cableLenOverride);
        return 0.0;
    };
    const int micId = (ln.aKind == 1) ? ln.a : (ln.bKind == 1 ? ln.b : -1);
    if (micId < 0) return 0.0;
    if (const double v = ofItem(micId); v > 0.0) return v;
    const int g = pr.GroupOf(micId);
    if (g < 0) return 0.0;
    for (int id : pr.micGroups[(size_t)g].ids)
        if (const double v = ofItem(id); v > 0.0) return v;
    return 0.0;
}

double CableRealLen(const Project& pr, const CableLink& ln) {
    const double ov = CableOverrideLen(pr, ln);
    return ov > 0.0 ? ov : CableRawLen(pr, ln);
}

double CableReservedLen(const Project& pr, const CableLink& ln) {
    const double ov = CableOverrideLen(pr, ln);
    return ov > 0.0 ? ov : StandardCableLen(CableRawLen(pr, ln));
}

// REAL length of the single DRAWN RUN that belongs to one microphone — the number
// the 05 tag prints for it (App::RunRealLen in Cabling.cpp follows the same rule
// while painting).  A bound group draws one run per member and a main microphone one
// run per capsule (CableLink::capsulePaths); a plain link has the one path.  A manual
// override, a flown route and a converged group cable are whole-link quantities, so
// those fall back to the link's real length exactly like the tag does.
double CableRunRealLen(const Project& pr, const CableLink& ln, int micId, int capsule) {
    const double ov = CableOverrideLen(pr, ln);
    if (ov > 0.0) return ov;
    if (ln.flown) return CableRawLen(pr, ln);
    if (CollectorLen(ln) > 0.0) return CableRawLen(pr, ln);
    // which run belongs to this microphone: the one whose endpoint sits on it (for
    // the main microphone, the endpoint of the requested capsule)
    const std::vector<std::pair<double, double>>* run = nullptr;
    if (ln.capsulePaths.size() >= 2) {
        double bx = 0.0, by = 0.0;
        bool have = false;
        if (const Item* m = pr.Find(micId)) {
            bx = m->x;
            by = m->y;
            have = true;
            if (capsule >= 0) {
                const std::vector<std::pair<double, double>> caps = MicCapsulePos(*m);
                if (capsule < (int)caps.size()) {
                    bx = caps[(size_t)capsule].first;
                    by = caps[(size_t)capsule].second;
                }
            }
        }
        double bestD = -1.0;
        for (const auto& r : ln.capsulePaths) {
            if (r.size() < 2) continue;
            if (!have) {   // no position: just take the first real run
                run = &r;
                break;
            }
            const double dA = std::hypot(r.front().first - bx, r.front().second - by);
            const double dB = std::hypot(r.back().first - bx, r.back().second - by);
            const double d = (std::min)(dA, dB);
            if (bestD < 0.0 || d < bestD) {
                bestD = d;
                run = &r;
            }
        }
    } else if (ln.path.size() >= 2) {
        run = &ln.path;
    }
    if (!run) return CableRealLen(pr, ln);
    return CableMicHeight(pr, ln) + PolyLen(*run);
}

// ---------------------------------------------------------------------------
// validation: channel budget (the protocol length rule is gone: one stagebox
// kind, no protocols, no per-protocol cable limit)
// ---------------------------------------------------------------------------

namespace {

// true when a link overbooks the endpoint.  A MICROPHONE is never a capacity
// constraint: its capacity is exactly its own capsule count, so a 2- or 4-channel
// cable that lands on a microphone is legal and must not raise "!".
bool EndpointOver(const CableEnd& e, int channels) {
    return e.ok && !e.mic && e.capacity > 0 && channels > e.capacity;
}

} // namespace

int MicItemChannels(const Item& it) {
    if (it.defId == L"mainmic") return it.mainTech == 3 ? 3 : 2;
    // one stage object, TWO channels: the coincidence pair of an intensity
    // technique (XY / MS / Blumlein) travels on ONE cable
    if (IsIntensityTech(it.defId)) return 2;
    return 1;   // spot mic / lavalier / one capsule of a technique group
}

// full, localized kind name — ONE 「接口箱」 for both box enum values, no ADDA
std::wstring DeviceKindTitle(DevKind k) {
    switch (k) {
    case DevKind::Mic: return L10n::T(S::TypeMic);
    case DevKind::AnalogBox: return L10n::T(S::CbKindStagebox);
    case DevKind::DigitalBox: return L10n::T(S::CbKindStagebox);
    case DevKind::SoundCard: return L10n::T(S::CbKindSoundCard);
    case DevKind::Receiver: return L10n::T(S::CbKindReceiver);
    }
    return L"";
}

int DeviceFlowRank(DevKind k) {
    switch (k) {
    case DevKind::Mic:        return 0;
    case DevKind::AnalogBox:  return 1;
    case DevKind::DigitalBox: return 1;   // merged into the stagebox
    case DevKind::Receiver:   return 1;
    case DevKind::SoundCard:  return 3;
    }
    return 0;
}

namespace {

// channels a microphone endpoint carries: the member count when it is bound into
// a MicGroup (the whole group travels on ONE cable), else its capsule count
int MicEndpointChannels(const Project& pr, const Item& it) {
    const int g = pr.GroupOf(it.id);
    if (g >= 0 && pr.micGroups[(size_t)g].ids.size() >= 2)
        return (int)pr.micGroups[(size_t)g].ids.size();
    return MicItemChannels(it);
}

} // namespace

std::vector<std::pair<double, double>> GroupMemberPos(const Project& pr, int itemId) {
    std::vector<std::pair<double, double>> out;
    const int g = pr.GroupOf(itemId);
    if (g >= 0 && pr.micGroups[(size_t)g].ids.size() >= 2) {
        for (int id : pr.micGroups[(size_t)g].ids)
            for (const auto& it : pr.items)
                if (it.id == id) { out.push_back({it.x, it.y}); break; }
        if (out.size() >= 2) return out;
        out.clear();
    }
    for (const auto& it : pr.items)
        if (it.id == itemId) { out.push_back({it.x, it.y}); break; }
    return out;
}

int DerivedCableChannels(const Project& pr, const CableLink& ln) {
    const CableEnd a = ResolveCableEnd(pr, ln.aKind, ln.a);
    const CableEnd b = ResolveCableEnd(pr, ln.bKind, ln.b);
    // a microphone cable carries exactly that microphone's channels (never the
    // device's), so a 1-channel lavalier feeding an 8-channel box is one channel
    if (a.ok && a.mic)
        for (const auto& it : pr.items)
            if (it.id == ln.a) return MicEndpointChannels(pr, it);
    if (b.ok && b.mic)
        for (const auto& it : pr.items)
            if (it.id == ln.b) return MicEndpointChannels(pr, it);
    if (!a.ok || !b.ok) return std::clamp(ln.channels, 1, 64);
    // device ↔ device trunk: a trunk carries what both ends can handle, so an
    // 8-channel sound card feeding a 4-channel box is a 4-channel link (legal)
    const int cap = (std::min)((std::max)(1, a.capacity), (std::max)(1, b.capacity));
    if (ln.channelsUser && ln.channels > 0) return std::clamp(ln.channels, 1, cap);
    return std::clamp(cap, 1, 64);
}

int DeviceLoad(const Project& pr, const std::vector<CableLink>& links, int devId) {
    if (!pr.FindDevice(devId)) return 0;
    int load = 0;
    for (const auto& ln : links) {
        const int ch = (std::max)(1, ln.channels);
        const bool aMic = ln.aKind == 1, bMic = ln.bKind == 1;
        if (aMic || bMic) {
            // a microphone cable always feeds its device end
            const int dev = aMic ? (ln.bKind == 0 ? ln.b : -1)
                                 : (ln.aKind == 0 ? ln.a : -1);
            if (dev == devId) load += ch;
            continue;
        }
        if (ln.aKind != 0 || ln.bKind != 0) continue;
        const Device* da = pr.FindDevice(ln.a);
        const Device* db = pr.FindDevice(ln.b);
        if (!da || !db) continue;
        const int ra = DeviceFlowRank(da->kind), rb = DeviceFlowRank(db->kind);
        // the signal flows towards the sound card; equal stages fall back to the id
        const bool aDown = (ra != rb) ? (ra > rb) : (ln.a > ln.b);
        if ((aDown ? ln.a : ln.b) == devId) load += ch;
    }
    return load;
}

bool DeviceOverloaded(const Project& pr, const std::vector<CableLink>& links,
                      int devId) {
    const Device* d = pr.FindDevice(devId);
    if (!d) return false;
    return DeviceLoad(pr, links, devId) > pr.DeviceCapacity(*d);
}

CableIssue CheckCable(const Project& pr, const CableLink& ln) {
    CableIssue is;
    const CableEnd a = ResolveCableEnd(pr, ln.aKind, ln.a);
    const CableEnd b = ResolveCableEnd(pr, ln.bKind, ln.b);
    // 1) the cable's own demand against what the receiving end accepts …
    const bool capA = EndpointOver(a, ln.channels);
    const bool capB = EndpointOver(b, ln.channels);
    // 2) … and the per-device budget: everything arriving at one input set.
    //    Differing device capacities are NOT a fault: an 8-channel sound card
    //    (a hub, unlimited) connected to a 4-channel box is a legal 4-channel trunk.
    const bool loadA = a.ok && !a.mic && DeviceOverloaded(pr, pr.links, a.id);
    const bool loadB = b.ok && !b.mic && DeviceOverloaded(pr, pr.links, b.id);
    is.overChannels = capA || capB || loadA || loadB;
    if (loadA || loadB) {
        const CableEnd& bad = loadA ? a : b;
        is.overByLoad = true;
        is.overName = bad.name;
        is.overCap = bad.capacity;
        is.overLoad = DeviceLoad(pr, pr.links, bad.id);
    } else if (capA || capB) {
        const CableEnd& bad = capA ? a : b;
        is.overName = bad.name;
        is.overCap = bad.capacity;
        is.overLoad = ln.channels;
    }
    is.radio = ln.radio;
    is.length = CableRealLen(pr, ln);   // 实长: 05 tags / hover
    return is;
}

bool CableProblemText(const Project& pr, const CableLink& ln, std::wstring* why) {
    const CableIssue is = CheckCable(pr, ln);
    if (!why) return is.overChannels;
    if (is.overChannels) {
        if (is.overByLoad)
            // 「输入超载：接口箱 1 已接 6 路，容量 4 路」
            *why = dr::Fmt(L10n::T(S::CbOverloadFmt), is.overName.c_str(),
                           is.overLoad, is.overCap);
        else
            // 「容量不足：需要 2 通道，接收器 只有 1 通道可用」
            *why = dr::Fmt(L10n::T(S::LinkOverFmt), ln.channels,
                           is.overName.c_str(), is.overCap);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Router.  There is no automatic channel assignment any more: every cable is one
// the user drew with the right mouse button, and its polyline is derived from the
// two endpoints alone.  The rule is SHORTEST ROUTE (docs/CABLING.md §走廊):
//   - straight when the two exit points are aligned, otherwise ONE corner;
//   - a crossing is only a picture and is NEVER avoided by a longer run — the
//     length is a real cable budget;
//   - the ONLY detour: the direct run would cut through a device rectangle (that
//     other L variant is then used) — nothing else may lengthen a run;
//   - the rear half is the UPSTAGE half: the stage rectangle is world x∈[0,w],
//     y∈[0,d] with y = 0 on the back (upstage) edge, so "rear" is y < d / 2.
//     There the corner may be cut by one 45° segment, which is strictly SHORTER
//     than the L and is the only diagonal allowed; the DOWNSTAGE half
//     (y >= d / 2, the audience side) stays strictly 0° / 90° with
//     vertical-first preference;
//   - the cable leaves the microphone marker on the side facing the target and
//     reaches the device on its nearest boundary point;
//   - a flown microphone goes up to the ceiling, across to the nearest wall, down
//     the wall and only then over the floor (the two vertical legs are stored on
//     the link, a plan view cannot show them).
// ---------------------------------------------------------------------------
namespace {

using PathPt = std::pair<double, double>;

double RoutePathLen(const std::vector<PathPt>& p) {
    double len = 0.0;
    for (size_t i = 1; i < p.size(); ++i) {
        const double dx = p[i].first - p[i - 1].first;
        const double dy = p[i].second - p[i - 1].second;
        len += std::sqrt(dx * dx + dy * dy);
    }
    return len;
}

// axis-aligned appends: the only two directions a cable run may take
void PushH(std::vector<PathPt>& pts, double x) {
    if (std::fabs(x - pts.back().first) < 1e-9) return;
    pts.push_back({x, pts.back().second});
}
void PushV(std::vector<PathPt>& pts, double y) {
    if (std::fabs(y - pts.back().second) < 1e-9) return;
    pts.push_back({pts.back().first, y});
}

// the point one `len` away from `p` along the outward direction of a face
PathPt Outward(const PathPt& p, int dir, double len) {
    switch (dir) {
    case 0: return {p.first, p.second - len};
    case 1: return {p.first + len, p.second};
    case 2: return {p.first, p.second + len};
    case 3: return {p.first - len, p.second};
    }
    return p;
}

// Collapse duplicate / micro segments, snap runs to the axes and drop every
// corner that is really a straight run (< 8°) or a U-turn spike (> 172°).
// The axis snap matters: a lane offset that happens to land a centimetre beside
// the microphone column must not be left behind as a hair-thin diagonal — the
// whole run is pulled onto the axis instead.
void SimplifyPath(std::vector<PathPt>& p) {
    const double tol = 0.02;       // duplicate / micro segment
    const double axisTol = 0.03;   // "this is meant to be horizontal/vertical"
    for (int iter = 0; iter < 8; ++iter) {
        bool changed = false;
        // --- axis snap: every segment becomes exactly horizontal or vertical ---
        for (size_t i = 1; i < p.size(); ++i) {
            const double dx = p[i].first - p[i - 1].first;
            const double dy = p[i].second - p[i - 1].second;
            if (std::fabs(dx) <= axisTol && std::fabs(dy) <= axisTol) {
                p.erase(p.begin() + (std::ptrdiff_t)i);   // micro segment
                --i;
                changed = true;
            } else if (std::fabs(dx) <= axisTol) {
                p[i].first = p[i - 1].first;              // nearly vertical
                changed = true;
            } else if (std::fabs(dy) <= axisTol) {
                p[i].second = p[i - 1].second;            // nearly horizontal
                changed = true;
            }
        }
        // --- drop corners that are straight or a spike -------------------------
        for (size_t i = 1; i + 1 < p.size(); ++i) {
            const double v1x = p[i].first - p[i - 1].first;
            const double v1y = p[i].second - p[i - 1].second;
            const double v2x = p[i + 1].first - p[i].first;
            const double v2y = p[i + 1].second - p[i].second;
            const double l1 = std::hypot(v1x, v1y), l2 = std::hypot(v2x, v2y);
            if (l1 < 1e-9 || l2 < 1e-9) {
                p.erase(p.begin() + (std::ptrdiff_t)i);
                --i;
                changed = true;
                continue;
            }
            const double dot =
                std::clamp((v1x * v2x + v1y * v2y) / (l1 * l2), -1.0, 1.0);
            const double deg = std::acos(dot) * 180.0 / 3.14159265358979323846;
            if (deg < 8.0 || deg > 172.0) {
                p.erase(p.begin() + (std::ptrdiff_t)i);
                --i;
                changed = true;
            }
        }
        // --- duplicate points (the merge above can create them) ---------------
        for (size_t i = 1; i < p.size(); ++i) {
            if (std::hypot(p[i].first - p[i - 1].first,
                           p[i].second - p[i - 1].second) <= tol) {
                p.erase(p.begin() + (std::ptrdiff_t)i);
                --i;
                changed = true;
            }
        }
        if (!changed) break;
    }
}

// ---------------------------------------------------------------------------
// Rear-half 45°.  The routing rule is "shortest route between the two exit
// points": straight when aligned, otherwise ONE corner.  Nothing is ever made
// longer to avoid a crossing — a crossing is only a picture, while the length is
// a real cable budget.  In the REAR half of the stage — the UPSTAGE half, i.e.
// every point with y < stage.d / 2 (y = 0 is the back edge and y = d the audience
// edge, see stageRect order in Model.h) — the corner may be cut by ONE 45°
// segment, which is strictly SHORTER than the L: the cut is min(|dx|, |dy|),
// i.e. the shortest 0/45/90 connection of the two points.  The DOWNSTAGE half
// (y >= stage.d / 2, the audience side) stays strictly 0° / 90°.
// ---------------------------------------------------------------------------
void ChamferRearCorner(const Project& pr, std::vector<PathPt>& p) {
    if (p.size() != 3) return;   // the corner of an L is the middle vertex
    const double rear = pr.stage.d * 0.5;
    const PathPt a = p[0], c = p[1], b = p[2];
    const double ax = c.first - a.first, ay = c.second - a.second;
    const double bx = b.first - c.first, by = b.second - c.second;
    const bool aVert = std::fabs(ax) < 1e-9, aHorz = std::fabs(ay) < 1e-9;
    const bool bVert = std::fabs(bx) < 1e-9, bHorz = std::fabs(by) < 1e-9;
    // a genuine 0/90 corner, and every point of it in the rear (upstage) half
    if (!((aVert && bHorz) || (aHorz && bVert))) return;
    if (!(a.second < rear && c.second < rear && b.second < rear)) return;
    const double la = std::fabs(ax) + std::fabs(ay), lb = std::fabs(bx) + std::fabs(by);
    if (la < 1e-9 || lb < 1e-9) return;
    const double cut = (std::min)(la, lb);
    if (cut < 0.15) return;      // a shorter cut would not read on screen
    const PathPt A2{c.first - ax / la * cut, c.second - ay / la * cut};
    const PathPt B2{c.first + bx / lb * cut, c.second + by / lb * cut};
    std::vector<PathPt> out{a};
    if (std::hypot(A2.first - a.first, A2.second - a.second) > 1e-6) out.push_back(A2);
    if (out.empty() || std::hypot(B2.first - out.back().first,
                                  B2.second - out.back().second) > 1e-6)
        out.push_back(B2);
    out.push_back(b);
    p.clear();
    for (const auto& q : out)
        if (p.empty() || std::hypot(q.first - p.back().first,
                                    q.second - p.back().second) > 1e-9)
            p.push_back(q);
}

// The point on a device rectangle's boundary nearest to (ox, oy) — pure
// nearest-point attachment, no "interface face" concept any more.
PathPt NearestOnRect(const CableEnd& e, double ox, double oy) {
    const double l = e.x - kCableDevW * 0.5, r = e.x + kCableDevW * 0.5;
    const double t = e.y - kCableDevH * 0.5, b = e.y + kCableDevH * 0.5;
    if (ox >= l && ox <= r && oy >= t && oy <= b) {
        // inside the footprint: project onto the closest edge
        const double dl = ox - l, dr = r - ox, dt = oy - t, db = b - oy;
        const double m = (std::min)((std::min)(dl, dr), (std::min)(dt, db));
        if (m == dl) return {l, oy};
        if (m == dr) return {r, oy};
        if (m == dt) return {ox, t};
        return {ox, b};
    }
    return {std::clamp(ox, l, r), std::clamp(oy, t, b)};
}

// Which side a boundary point lies on: 0 up, 1 right, 2 down, 3 left.
int SideOfPoint(const CableEnd& e, const PathPt& p) {
    const double l = e.x - kCableDevW * 0.5, r = e.x + kCableDevW * 0.5;
    const double t = e.y - kCableDevH * 0.5, b = e.y + kCableDevH * 0.5;
    const double dl = std::fabs(p.first - l), dr = std::fabs(p.first - r);
    const double dt = std::fabs(p.second - t), db = std::fabs(p.second - b);
    const double m = (std::min)((std::min)(dl, dr), (std::min)(dt, db));
    if (m == dt) return 0;
    if (m == dr) return 1;
    if (m == db) return 2;
    return 3;
}

// Where a cable leaves an object.  A microphone leaves at the point of its
// marker circle nearest the target (radial), a device at the point of its
// rectangle nearest the target — never 0.30 / 0.65 m short of it.
PathPt CableExitPoint(const CableEnd& e, double ox, double oy, double micRadius) {
    if (e.mic) {
        const double rr = (micRadius > 0.0 ? micRadius : kCableMicRadius);
        double dx = ox - e.x, dy = oy - e.y;
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6) return {e.x, e.y + rr};
        return {e.x + dx / len * rr, e.y + dy / len * rr};
    }
    return NearestOnRect(e, ox, oy);
}

// true when a segment's bounding box overlaps a device rectangle (5 cm margin)
bool SegmentHitsDevice(const PathPt& a, const PathPt& b, const Device& dv) {
    const double m = 0.05;
    const double L = dv.x - kCableDevW * 0.5 - m, R = dv.x + kCableDevW * 0.5 + m;
    const double T = dv.y - kCableDevH * 0.5 - m, B = dv.y + kCableDevH * 0.5 + m;
    const double xmin = (std::min)(a.first, b.first), xmax = (std::max)(a.first, b.first);
    const double ymin = (std::min)(a.second, b.second), ymax = (std::max)(a.second, b.second);
    return xmax > L && xmin < R && ymax > T && ymin < B;
}

// The SHORTEST route between two exit points: straight when aligned, otherwise
// ONE corner (vertical-first; in the rear = upstage half, y < d / 2, the corner
// is cut by a 45° segment, which is strictly shorter).  `selfA` / `selfB` are the
// device ids the run belongs to — a run may touch its own boxes, never another
// one: that (and only that, plus leaving the canvas) may lengthen a run.
std::vector<PathPt> RouteShortest(const Project& pr, const PathPt& P0, const PathPt& F,
                                  int selfA, int selfB) {
    if (std::fabs(P0.first - F.first) <= 0.05 || std::fabs(P0.second - F.second) <= 0.05) {
        std::vector<PathPt> p{P0, F};      // already aligned: one straight segment
        SimplifyPath(p);
        return p;
    }
    std::vector<std::vector<PathPt>> cand;
    cand.push_back({P0, {P0.first, F.second}, F});   // vertical first
    cand.push_back({P0, {F.first, P0.second}, F});   // horizontal first
    for (auto& c : cand) ChamferRearCorner(pr, c);   // rear (upstage) half only, never longer
    int bestCuts = 1 << 30;
    double bestLen = 1e18;
    std::vector<PathPt> best;
    for (auto& c : cand) {
        SimplifyPath(c);
        int cuts = 0;
        for (const auto& dv : pr.devices) {
            if (dv.id == selfA || dv.id == selfB) continue;
            for (size_t k = 1; k < c.size(); ++k)
                if (SegmentHitsDevice(c[k - 1], c[k], dv)) { ++cuts; break; }
        }
        const double len = RoutePathLen(c);
        if (best.empty() || cuts < bestCuts ||
            (cuts == bestCuts && len < bestLen - 1e-9)) {
            bestCuts = cuts;
            bestLen = len;
            best = c;
        }
    }
    return best;
}

// ONE run of a cable: the polyline plus — for a flown (吊装) microphone — the four
// measured legs, which the painter / the length rule need but a plan view cannot
// show.  The path always starts at the source point of the run.
struct RunRoute {
    std::vector<PathPt> path;
    bool flown = false;
    int ceilSegs = 0;
    double legUp = 0.0, legCeil = 0.0, legWall = 0.0, legFloor = 0.0;
};

// Flown microphone: up from the capsule to the ceiling, across the ceiling to the
// SIDE WALL THE TARGET SITS ON, down that wall (the full room height) and then the
// shortest floor run from the wall base to the target.  The ceiling run and the
// floor run are drawn; the two vertical legs are only in the length.
//
// The side wall is NOT "the nearest of the four walls": a flown cable must never
// travel to the front / rear wall (y = 0 / y = d), so only x = 0 and x = stage.w
// are candidates and the choice follows the TARGET DEVICE — a box on the left half
// of the stage is reached over the left wall, one on the right half over the right
// wall.  A target exactly on the centre line falls back to the nearer side wall.
// The ceiling run is therefore always horizontal (parallel to the stage front) and
// its length is measured along x only.
RunRoute RouteFlownRun(const Project& pr, const CableEnd& to, const PathPt& c,
                       double flyHeight, double micRadius) {
    RunRoute r;
    const double w = pr.stage.w;
    const double rr = (micRadius > 0.0 ? micRadius : kCableMicRadius);
    // the wall the cable runs down: the side the TARGET DEVICE sits on
    const double half = w * 0.5;
    double wallX;
    if (to.x > half) wallX = w;
    else if (to.x < half) wallX = 0.0;
    else wallX = (c.first < half) ? 0.0 : w;   // dead centre: the nearer side wall
    const PathPt wall{wallX, c.second};
    // leave the marker towards the wall (the ceiling run)
    const double dx = wall.first - c.first;
    const double side = (dx >= 0.0) ? 1.0 : -1.0;
    const PathPt start{c.first + side * rr, c.second};
    r.path.push_back(start);
    r.path.push_back(wall);
    r.ceilSegs = 1;                       // the first segment is the ceiling run
    const PathPt F = CableExitPoint(to, wall.first, wall.second, micRadius);
    const std::vector<PathPt> floor =
        RouteShortest(pr, wall, F, -1, to.mic ? -1 : to.id);
    for (size_t i = 1; i < floor.size(); ++i) r.path.push_back(floor[i]);
    r.flown = true;
    r.legUp = (std::max)(0.0, pr.ceiling - flyHeight);
    // horizontal ceiling run, parallel to the stage front: |micX − wallX|
    r.legCeil = std::fabs(dx);
    r.legWall = pr.ceiling;
    r.legFloor = RoutePathLen(floor);
    return r;
}

// ONE run of a cable: from the source point `c` (a microphone capsule / group
// member, or the source device) to the nearest side of the target.  `atPoint` is
// the CONVERGED group run: it leaves the group's convergence point itself, so it
// is not pushed off a marker edge (that point is not a drawn symbol).
RunRoute RouteRun(const Project& pr, const CableEnd& from, const CableEnd& to,
                  const PathPt& c, double micRadius, const Item* micItem,
                  bool atPoint = false) {
    if (micItem && micItem->flown)
        // the flown run starts at the microphone's OWN height above the floor;
        // the ceiling (indoor height) supplies the rest of the run
        return RouteFlownRun(pr, to, c, micItem->height, micRadius);
    RunRoute r;
    const double rr = (micRadius > 0.0 ? micRadius : kCableMicRadius);
    const PathPt F = CableExitPoint(to, c.first, c.second, micRadius);
    PathPt P0;
    if (atPoint) {
        P0 = c;   // the group's convergence point, nothing to leave
    } else if (from.mic) {
        const double dir = (F.second < c.second) ? -1.0 : 1.0;
        P0 = {c.first, c.second + dir * rr};   // leave the marker towards the target
    } else {
        P0 = CableExitPoint(from, to.x, to.y, micRadius);   // leave the box edge
    }
    r.path = RouteShortest(pr, P0, F, (from.mic || atPoint) ? -1 : from.id,
                           to.mic ? -1 : to.id);
    return r;
}

// Where a floor run starts: the microphone marker (the stub leaves radially
// towards the target) or a device face (the stub is perpendicular to that face).
// `faceDir < 0` means "no stub" (the wall base of a flown run).
struct RunStart {
    bool mic = false;
    double x = 0.0, y = 0.0;            // microphone centre
    double r = kCableMicRadius;         // drawn marker radius
    PathPt p;                           // device face point / wall base
    int faceDir = -1;
};

// floor part of a run: from an arbitrary start point to the target device, using
// the corridor rules.  Split out so the flown router can start the floor leg at
// the wall base instead of at the microphone.
std::vector<PathPt> RouteFloorPath(const Project& pr, const RunStart& s,
                                   const CableEnd& to, int lane, double micRadius,
                                   int sidePref = 0) {
    const double w = pr.stage.w, d = pr.stage.d;
    const double laneOff = 0.12 * (lane + 1);
    // pure nearest-point attachment: the target side is whichever face of the
    // device rectangle is closest to where the run starts (no stored faceDir)
    const double refX = s.mic ? s.x : s.p.first;
    const double refY = s.mic ? s.y : s.p.second;
    const PathPt F = CableExitPoint(to, refX, refY, micRadius);
    const int toDir = to.mic ? 2 : SideOfPoint(to, F);
    const bool horizFace = (toDir == 0 || toDir == 2);
    // the side away from the target box (per-lane columns widen away from it)
    const double sideAway = (to.x < w * 0.5) ? -1.0 : 1.0;
    // parallel arrivals spread sideways along the face, so a fan into one box
    // stays parallel all the way (never a diagonal in the last metre)
    PathPt P1 = F;
    if (!to.mic) {
        const double spr = 0.12 * ((lane % 5) + 1) * ((lane % 2) ? -1.0 : 1.0);
        const double off = std::clamp(spr, -0.34, 0.34);
        if (horizFace) P1.first += off;
        else P1.second += off;
    }
    // the cable always reaches the face perpendicular: one gap outside it
    const PathPt A1 = to.mic ? P1 : Outward(P1, toDir, 0.28);

    // --- start point + perpendicular stub -----------------------------------
    const double stub = 0.18 + 0.06 * (double)lane;
    PathPt P0;
    PathPt A0;
    if (s.mic) {
        // leave the marker on the side that faces the target
        const int dir = (A1.second < s.y - 1e-6) ? 0 : 2;
        P0 = {s.x, s.y + (dir == 2 ? s.r : -s.r)};
        A0 = {P0.first, P0.second + (dir == 2 ? stub : -stub)};
    } else if (s.faceDir >= 0) {
        P0 = s.p;
        A0 = Outward(P0, s.faceDir, stub);
    } else {
        P0 = A0 = s.p;   // wall base (flown): no stub
    }

    // --- already aligned: one clean straight segment -------------------------
    const bool sameCol = std::fabs(A0.first - A1.first) <= 0.02;
    const bool sameRow = std::fabs(A0.second - A1.second) <= 0.02;
    const bool startVert = s.mic || s.faceDir == 0 || s.faceDir == 2;
    const bool startHorz = !s.mic && (s.faceDir == 1 || s.faceDir == 3);
    if ((horizFace && sameCol && startVert) || (!horizFace && sameRow && startHorz))
        return {P0, P1};

    // --- corridor ------------------------------------------------------------
    std::vector<PathPt> pts{P0, A0};
    if (horizFace) {
        // A microphone drops straight down its own column: a lane detour measured
        // in centimetres would show up as exactly the little jog the user saw.
        // Trunks (device → device) use the centre line / side lane instead.
        const bool mid = A0.first > w / 3.0 && A0.first < 2.0 * w / 3.0;
        // the corridor side is a choice: -1 = left of the box, +1 = right.  The
        // caller tries both and keeps the one with fewer crossings.
        const double side = (sidePref != 0) ? (double)sidePref
                                            : ((to.x < w * 0.5) ? -1.0 : 1.0);
        const double laneCol =
            mid ? (w * 0.5 + laneOff * side) : side * (0.35 + laneOff);
        // NOTE: sharing the approach column measured better than widening it per
        // lane (the widening created more crossings elsewhere) — kept shared.
        const double col = (s.mic || s.faceDir < 0) ? A0.first : laneCol;
        // row: the face row, shifted outwards per lane so a fan stays parallel
        const double row = A1.second + (toDir == 2 ? 1.0 : -1.0) * 0.12 * (double)lane;
        PushH(pts, col);       // onto the corridor column
        PushV(pts, row);       // travel along it
        PushH(pts, A1.first);  // over to the face column
        PushV(pts, P1.second); // perpendicular stub into the face
    } else {
        // vertical face: the run comes off the face row, perpendicular to it
        const double row = A1.second + 0.12 * (double)lane *
                                            ((toDir == 1) ? 1.0 : -1.0);
        PushH(pts, A1.first);   // over to the face column (outside the box)
        PushV(pts, row);        // travel to the face row
        PushH(pts, P1.first);   // straight into the face
    }
    SimplifyPath(pts);
    // the painter, the hit test and the midpoint tag want ≥ 3 vertices; a
    // straight run gets an honest midpoint rather than a fake corner
    if (pts.size() < 3) {
        const PathPt a = pts.front(), b = pts.back();
        pts.insert(pts.begin() + 1, {(a.first + b.first) * 0.5, (a.second + b.second) * 0.5});
    }
    (void)d;
    return pts;
}

// ---------------------------------------------------------------------------
// flown (吊装) microphone: legUp mic → ceiling, legCeil ceiling → the SIDE wall the
// target device sits on, legWall down the wall, legFloor along the floor
// corridors.  Only the last two can be drawn in a plan view.
// ---------------------------------------------------------------------------
struct FlownRoute {
    std::vector<PathPt> path;
    int ceilSegs = 0;      // leading path segments that run above head height
    double legUp = 0.0, legCeil = 0.0, legWall = 0.0, legFloor = 0.0;
    double run() const { return legUp + legCeil + legWall + legFloor; }
};

FlownRoute RouteFlownPath(const Project& pr, const CableEnd& mic,
                          const CableEnd& dev, int lane, double flyHeight,
                          double micRadius) {
    FlownRoute r;
    const double w = pr.stage.w, d = pr.stage.d;
    const double laneOff = 0.12 * (lane + 1);
    const PathPt p0{mic.x, mic.y};
    const PathPt start{mic.x, mic.y + (micRadius > 0.0 ? micRadius : kCableMicRadius)};

    // Only the two SIDE walls (x = 0 / x = w) can carry the ceiling run, and the
    // choice follows the TARGET DEVICE's side of the stage — never the front /
    // rear wall.  A target exactly on the centre line uses the nearer side wall.
    const double half = w * 0.5;
    double wallX;
    if (dev.x > half) wallX = w;
    else if (dev.x < half) wallX = 0.0;
    else wallX = (p0.first < half) ? 0.0 : w;
    const double spread = laneOff * ((lane % 2) ? 1.0 : -1.0);
    PathPt wall{wallX, p0.second + spread};
    wall.second = std::clamp(wall.second, 0.0, d);

    r.path.push_back(start);
    r.path.push_back(wall);
    r.ceilSegs = 1;
    RunStart wallStart;
    wallStart.p = wall;      // the wall base: no start stub, the floor leg is next
    wallStart.faceDir = -1;
    const std::vector<PathPt> floorPath =
        RouteFloorPath(pr, wallStart, dev, lane, micRadius);
    for (size_t i = 1; i < floorPath.size(); ++i) r.path.push_back(floorPath[i]);

    r.legUp = (std::max)(0.0, pr.ceiling - flyHeight);
    r.legCeil = std::fabs(wall.first - start.first);   // horizontal, along x only
    r.legWall = pr.ceiling;
    r.legFloor = RoutePathLen(floorPath);
    return r;
}

// reverse a polyline in place (the router always runs microphone → device)
void ReversePath(std::vector<PathPt>& p) { std::reverse(p.begin(), p.end()); }

} // namespace

// ---------------------------------------------------------------------------
// capsules of a microphone (the main microphone is one marker per capsule)
// ---------------------------------------------------------------------------
double PolyLen(const std::vector<std::pair<double, double>>& p) {
    double sum = 0.0;
    for (size_t i = 1; i < p.size(); ++i)
        sum += std::hypot(p[i].first - p[i - 1].first, p[i].second - p[i - 1].second);
    return sum;
}

int MicCapsuleCount(const Item& it) {
    if (it.defId != L"mainmic") return 1;
    return it.mainTech == 3 ? 3 : 2;   // AB / mixed / intensity = L+R, Decca = L+C+R
}

std::vector<std::pair<double, double>> MicCapsulePos(const Item& it) {
    std::vector<std::pair<double, double>> out;
    const int n = MicCapsuleCount(it);
    if (n < 2) {
        out.push_back({it.x, it.y});
        return out;
    }
    // the capsules sit side by side along the microphone's side axis, exactly like
    // the symbol on 01/02: side = (cos angle, sin angle), spacing apart.  The 2 cm
    // floor only keeps a Degenerate spacing (an old file with 0) from collapsing the
    // two caps onto one point; the UI clamps the field to 0.10 … 3.00 m, so every
    // real pair is EXACTLY spacing × ppm_ apart.
    const double kPi = 3.14159265358979323846;
    const double rad = it.angle * kPi / 180.0;
    const double sideX = std::cos(rad), sideY = std::sin(rad);
    // a Decca Tree's centre capsule protrudes FORWARD by `center` metres (the
    // same distance the 01/02 symbol draws it at); it is never scaled
    const double fwdX = std::sin(rad), fwdY = -std::cos(rad);
    const bool decca = (it.mainTech == 3);
    const double sp = (std::max)(0.02, it.spacing);
    // L and R are EXACTLY `spacing` apart (the same 1:1 rule as an AB pair): the
    // two outer capsules sit at ±spacing/2 and the Decca centre capsule sits ON
    // the pair midpoint, pushed forward by `center` — the 01/02 symbol draws it
    // there, and the cable run for every capsule starts on that drawn circle
    for (int i = 0; i < n; ++i) {
        double off = 0.0;
        if (decca) {
            if (i == 0) off = -sp * 0.5;
            else if (i == 2) off = sp * 0.5;
        } else {
            off = ((double)i - (double)(n - 1) * 0.5) * sp;
        }
        double px = it.x + sideX * off;
        double py = it.y + sideY * off;
        if (decca && i == 1) {
            px += fwdX * it.center;
            py += fwdY * it.center;
        }
        out.push_back({px, py});
    }
    return out;
}

// ---------------------------------------------------------------------------
// marker scale: the cable terminal radius follows the DRAWN symbol of the mic —
// the main microphone is bigger (1.3×), a point mic grew with its 1.7× stage
// symbol (0.5 × 1.7 = 0.85) and the lavalier keeps its 1.75× of that.  The
// router and the 03 marker both use this factor, so the cable really stops on
// the circle.
double MicMarkerScaleOf(const Project& pr, int micId) {
    for (const auto& it : pr.items)
        if (it.id == micId && it.kind == ItemKind::Mic) {
            if (it.defId == L"mainmic") return 1.3;
            if (it.defId == L"lavalier") return 0.5 * 1.7;   // same as a point mic
            return 0.5 * 1.7;                                       // 0.85
        }
    return 0.5 * 1.7;   // 0.85
}
static void BuildCablePathEx(const Project& pr, CableLink& ln, int lane,
                             double micRadius, int sidePref, double gridDist) {
    const CableEnd a = ResolveCableEnd(pr, ln.aKind, ln.a);
    const CableEnd b = ResolveCableEnd(pr, ln.bKind, ln.b);
    ln.path.clear();
    ln.capsulePaths.clear();
    ln.collectorPaths.clear();
    ln.flown = false;
    ln.ceilSegs = 0;
    ln.legUp = ln.legCeil = ln.legWall = ln.legFloor = 0.0;
    if (!a.ok || !b.ok) return;

    // the run is always routed from the microphone end (if any) to the device
    const bool aIsMic = a.mic;
    const CableEnd& from = a.mic ? a : b;
    const CableEnd& to = a.mic ? b : a;

    // a flown microphone starts at the ceiling, not on the floor
    const Item* micItem = nullptr;
    if (a.mic || b.mic) {
        const int micId = a.mic ? ln.a : ln.b;
        for (const auto& it : pr.items)
            if (it.id == micId) { micItem = &it; break; }
    }
    const bool flown = (micItem && micItem->flown);

    // one routed polyline for one microphone capsule (a device-only run passes the
    // device exit point instead).  `laneIdx` keeps parallel capsule runs apart and
    // `atPoint` marks the CONVERGED group run, which starts on the convergence point
    // itself instead of on a drawn marker edge.
    auto route = [&](double micX, double micY, int laneIdx, bool atPoint,
                     bool& flownOut, int& ceilOut, double& lu, double& lc, double& lw,
                     double& lf) {
        std::vector<PathPt> p;
        flownOut = false;
        ceilOut = 0;
        lu = lc = lw = lf = 0.0;
        // ---- shortest direct route: no rows, no ring corridors, no detours ----
        if (g_corridorMode == CorridorMode::Grid && !to.mic) {
            const PathPt c = (from.mic || atPoint) ? PathPt{micX, micY}
                                                   : PathPt{from.x, from.y};
            const RunRoute run = RouteRun(pr, from, to, c, micRadius, micItem, atPoint);
            flownOut = run.flown;
            ceilOut = run.ceilSegs;
            lu = run.legUp;
            lc = run.legCeil;
            lw = run.legWall;
            lf = run.legFloor;
            std::vector<PathPt> p = run.path;
            if (!aIsMic) ReversePath(p);
            return p;
        }
        if (flown) {
            CableEnd f = from;
            f.x = micX;
            f.y = micY;
            const FlownRoute fr = RouteFlownPath(pr, f, to, laneIdx, micItem->height,
                                                 micRadius);
            p = fr.path;
            flownOut = true;
            ceilOut = fr.ceilSegs;
            lu = fr.legUp;
            lc = fr.legCeil;
            lw = fr.legWall;
            lf = fr.legFloor;
        } else {
            RunStart st;
            if (from.mic && !atPoint) {
                st.mic = true;
                st.x = micX;
                st.y = micY;
                st.r = (micRadius > 0.0 ? micRadius : kCableMicRadius);
            } else if (atPoint) {
                // the group's convergence point: no marker, no start stub — the
                // run simply leaves it (the same shape the wall base of a flown
                // run uses)
                st.p = {micX, micY};
                st.faceDir = -1;
            } else {
                // nearest boundary point towards the other end, stub perpendicular
                st.p = CableExitPoint(from, to.x, to.y, micRadius);
                st.faceDir = SideOfPoint(from, st.p);
            }
            p = RouteFloorPath(pr, st, to, laneIdx, micRadius, sidePref);
        }
        if (!aIsMic) ReversePath(p);   // keep path[0] on endpoint a
        return p;
    };

    // ---- a bound GROUP converges first --------------------------------------
    // A stereo pair / technique pair / Decca tree is ONE cable: every member runs a
    // short collector stub to the group's convergence point (the average of the
    // member positions, or the L/R midpoint for a Decca tree, whose centre capsule
    // is excluded from the average) and ONE main run leaves that point for the
    // target.  The main mic's OWN capsules are not a group: they keep one run each.
    if (micItem) {
        const int gi = pr.GroupOf(micItem->id);
        if (gi >= 0 && pr.micGroups[(size_t)gi].ids.size() >= 2) {
            const std::vector<std::pair<double, double>> members =
                GroupMemberPos(pr, micItem->id);
            if (members.size() >= 2) {
                const std::pair<double, double> conv =
                    GroupConvergencePos(pr, micItem->id);
                for (const auto& m : members) ln.collectorPaths.push_back({m, conv});
                bool fo = false;
                int cs = 0;
                double u = 0, c = 0, w = 0, f = 0;
                ln.path = route(conv.first, conv.second, lane, true, fo, cs, u, c, w, f);
                ln.flown = fo;
                ln.ceilSegs = cs;
                ln.legUp = u;
                ln.legCeil = c;
                ln.legWall = w;
                ln.legFloor = f;
                return;
            }
        }
    }

    // ONE cable, several parallel runs: the main microphone is one marker per
    // capsule (L/R, L/C/R).  Every run gets its own lane inside the link:
    // lane × n + i.
    std::vector<std::pair<double, double>> caps;
    if (micItem && !flown) {
        caps = GroupMemberPos(pr, micItem->id);
        if (caps.size() < 2 && MicCapsuleCount(*micItem) >= 2)
            caps = MicCapsulePos(*micItem);
    }
    const int nRun = (int)caps.size();
    if (nRun >= 2) {
        bool fo0 = false;
        int cs0 = 0;
        double u0 = 0, c0 = 0, w0 = 0, f0 = 0;
        for (int i = 0; i < nRun; ++i) {
            bool fo;
            int cs;
            double u, c, w, f;
            const auto pos = caps[(size_t)i];
            ln.capsulePaths.push_back(
                route(pos.first, pos.second, lane * nRun + i, false, fo, cs, u, c, w, f));
            if (i == 0) { fo0 = fo; cs0 = cs; u0 = u; c0 = c; w0 = w; f0 = f; }
        }
        ln.path = ln.capsulePaths.front();
        ln.flown = fo0;
        ln.ceilSegs = cs0;
        ln.legUp = u0;
        ln.legCeil = c0;
        ln.legWall = w0;
        ln.legFloor = f0;
        // each run has its own length; the aggregate length of the link is the one
        // of its first run (the tags print the per-run numbers)
        return;
    }

    bool fo;
    int cs;
    double u, c, w, f;
    ln.path = route(from.mic ? from.x : 0.0, from.mic ? from.y : 0.0, lane, false, fo, cs,
                    u, c, w, f);
    ln.flown = fo;
    ln.ceilSegs = cs;
    ln.legUp = u;
    ln.legCeil = c;
    ln.legWall = w;
    ln.legFloor = f;
}

// ---------------------------------------------------------------------------
// crossings + lane ordering
// ---------------------------------------------------------------------------
namespace {

bool SegCrosses(const PathPt& a1, const PathPt& a2, const PathPt& b1, const PathPt& b2) {
    const double eps = 0.03;   // touching / shared endpoints are not crossings
    const bool av = std::fabs(a1.first - a2.first) < 0.02;
    const bool ah = std::fabs(a1.second - a2.second) < 0.02;
    const bool bv = std::fabs(b1.first - b2.first) < 0.02;
    const bool bh = std::fabs(b1.second - b2.second) < 0.02;
    if ((av && bv) || (ah && bh)) return false;      // parallel / collinear
    if (!av && !ah) return false;                    // degenerate segment
    if (!bv && !bh) return false;
    // vertical × horizontal
    const PathPt& va = av ? a1 : b1;
    const PathPt& vb = av ? a2 : b2;
    const PathPt& h1 = av ? b1 : a1;
    const PathPt& h2 = av ? b2 : a2;
    const double xv = va.first;
    const double yh = h1.second;
    const double ylo = (std::min)(va.second, vb.second), yhi = (std::max)(va.second, vb.second);
    const double xlo = (std::min)(h1.first, h2.first), xhi = (std::max)(h1.first, h2.first);
    return xv > xlo + eps && xv < xhi - eps && yh > ylo + eps && yh < yhi - eps;
}

// every polyline of a link (the routed path + the capsule runs) with its owner
struct RunRef {
    const std::vector<PathPt>* pts = nullptr;
    int link = 0;
};

std::vector<RunRef> CollectRuns(const std::vector<CableLink>& links) {
    std::vector<RunRef> out;
    for (size_t i = 0; i < links.size(); ++i) {
        const CableLink& ln = links[i];
        if (ln.path.size() >= 2) out.push_back({&ln.path, (int)i});
        for (const auto& cp : ln.capsulePaths)
            if (cp.size() >= 2) out.push_back({&cp, (int)i});
    }
    return out;
}

} // namespace

int CountCrossings(const std::vector<CableLink>& links) {
    const std::vector<RunRef> runs = CollectRuns(links);
    int n = 0;
    for (size_t i = 0; i < runs.size(); ++i)
        for (size_t j = i + 1; j < runs.size(); ++j) {
            // the two capsule runs of the SAME cable never count
            if (runs[i].link == runs[j].link) continue;
            const auto& A = *runs[i].pts;
            const auto& B = *runs[j].pts;
            for (size_t a = 1; a < A.size(); ++a)
                for (size_t b = 1; b < B.size(); ++b)
                    if (SegCrosses(A[a - 1], A[a], B[b - 1], B[b])) ++n;
        }
    return n;
}

double TotalRunLen(const std::vector<CableLink>& links) {
    double sum = 0.0;
    for (const auto& ln : links) {
        sum += CablePolylineLen(ln);
        for (const auto& cp : ln.capsulePaths) sum += PolyLen(cp);
    }
    return sum;
}

CorridorMode g_corridorMode = CorridorMode::Grid;   // Grid is the default (fewer crossings)

void BuildCablePath(const Project& pr, CableLink& ln, int lane, double micRadius) {
    BuildCablePathEx(pr, ln, lane, micRadius, 0, -1.0);
}

void RebuildAllCablePaths(Project& pr, double micRadius) {
    // derived per link: the cable's channel count and the radio flag
    for (auto& ln : pr.links) {
        ln.channels = DerivedCableChannels(pr, ln);
        ln.radio = IsRadioLink(pr, ln);
    }

    // ---- lane order = geometric order ---------------------------------------
    // Every cable into one device gets a lane in the order of its coordinate
    // along the side it enters (x for a top/bottom entry, y for a left/right
    // entry).  A monotone assignment cannot produce a fan-in crossing.
    std::vector<int> lane(pr.links.size(), 0);
    {
        std::map<int, std::vector<int>> fan;   // target device -> link indices
        for (size_t i = 0; i < pr.links.size(); ++i) {
            const CableLink& ln = pr.links[i];
            int tgt = -1;
            if (ln.aKind == 0 && ln.bKind != 0) tgt = ln.a;
            else if (ln.bKind == 0 && ln.aKind != 0) tgt = ln.b;
            else if (ln.aKind == 0 && ln.bKind == 0) {
                // trunk: the signal flows towards the sound card
                const Device* da = pr.FindDevice(ln.a);
                const Device* db = pr.FindDevice(ln.b);
                if (da && db) tgt = (DeviceFlowRank(da->kind) >= DeviceFlowRank(db->kind)) ? ln.a : ln.b;
            }
            if (tgt > 0) fan[tgt].push_back((int)i);
        }
        for (auto& kv : fan) {
            const Device* dv = pr.FindDevice(kv.first);
            auto& idxs = kv.second;
            if (!dv) {
                for (size_t k = 0; k < idxs.size(); ++k) lane[idxs[k]] = (int)k;
                continue;
            }
            const CableEnd me = ResolveCableEnd(pr, 0, kv.first);
            auto entryCoord = [&](int li) {
                const CableLink& ln = pr.links[(size_t)li];
                const bool aIsDev = (ln.aKind == 0 && ln.a == kv.first);
                const CableEnd oe = ResolveCableEnd(pr, aIsDev ? ln.bKind : ln.aKind,
                                                    aIsDev ? ln.b : ln.a);
                const PathPt p = NearestOnRect(me, oe.x, oe.y);
                const int side = SideOfPoint(me, p);
                return (side == 0 || side == 2) ? oe.x : oe.y;
            };
            std::stable_sort(idxs.begin(), idxs.end(), [&](int a, int b) {
                const double ca = entryCoord(a), cb = entryCoord(b);
                if (std::fabs(ca - cb) > 1e-9) return ca < cb;
                return a < b;
            });
            for (size_t k = 0; k < idxs.size(); ++k) lane[idxs[k]] = (int)k;
        }
    }

    // ---- shortest direct routing (default) -----------------------------------
    // Every cable gets the SHORTEST route: straight when aligned, otherwise ONE
    // corner (vertical-first; one 45 deg cut in the rear = upstage half, y < d/2).
    // Crossings are allowed: a crossing is only a picture and is never bought
    // with a longer cable.  The only detour is a device rectangle in the way.
    if (g_corridorMode == CorridorMode::Grid) {
        for (size_t i = 0; i < pr.links.size(); ++i) {
            CableLink& ln = pr.links[i];
            const bool aMic = (ln.aKind == 1);
            const CableEnd me = ResolveCableEnd(pr, aMic ? ln.aKind : ln.bKind,
                                                aMic ? ln.a : ln.b);
            const CableEnd te = ResolveCableEnd(pr, aMic ? ln.bKind : ln.aKind,
                                                aMic ? ln.b : ln.a);
            if (!me.ok || !te.ok) continue;
            const Item* micItem = nullptr;
            if (me.mic) {
                const int mid = aMic ? ln.a : ln.b;
                for (const auto& it : pr.items)
                    if (it.id == mid) { micItem = &it; break; }
            }
            // The DRAWN marker is not the same size for every microphone (the main
            // mic 1.3×, a point / lavalier mic 0.85×), so the run must leave THAT
            // circle: the terminal radius is the base marker radius × this mic's
            // factor — exactly App::MicMarkerRadiusF()/ppm_.  Without it a point mic
            // cable overshoots the circle and a main-mic cable stops inside it.
            const double mr = micItem ? micRadius * MicMarkerScaleOf(pr, micItem->id)
                                      : micRadius;
            // One parallel run per source point: the main microphone's capsules
            // (L/R, L/C/R), or — for a grouped microphone — EVERY member of the
            // MicGroup, because a group is wired as ONE cable.  Each run leaves its
            // own member's marker, so the runs stay parallel by themselves.
            //
            // A BOUND GROUP converges first instead: one short collector stub from
            // every member to the group's convergence point, then ONE main run from
            // that point to the target.
            ln.collectorPaths.clear();
            ln.capsulePaths.clear();
            std::vector<std::pair<double, double>> caps;
            if (micItem) {
                const int g = pr.GroupOf(micItem->id);
                if (g >= 0 && pr.micGroups[(size_t)g].ids.size() >= 2) {
                    const auto members = GroupMemberPos(pr, micItem->id);
                    if (members.size() >= 2) {
                        const auto conv = GroupConvergencePos(pr, micItem->id);
                        for (const auto& m : members)
                            ln.collectorPaths.push_back({m, conv});
                        const RunRoute r = RouteRun(pr, me, te, conv, mr, micItem,
                                                    true);
                        ln.path = r.path;
                        ln.flown = r.flown;
                        ln.ceilSegs = r.ceilSegs;
                        ln.legUp = r.legUp;
                        ln.legCeil = r.legCeil;
                        ln.legWall = r.legWall;
                        ln.legFloor = r.legFloor;
                        continue;
                    }
                }
                caps = GroupMemberPos(pr, micItem->id);
            }
            if (caps.size() < 2 && micItem && MicCapsuleCount(*micItem) >= 2)
                caps = MicCapsulePos(*micItem);
            if (caps.empty()) caps.push_back({me.x, me.y});
            std::vector<std::vector<PathPt>> runs;
            RunRoute first;
            for (const auto& c : caps) {
                const RunRoute r = RouteRun(pr, me, te, c, mr, micItem);
                if (runs.empty()) first = r;
                runs.push_back(r.path);
            }
            if (!runs.empty()) {
                ln.path = runs.front();
                ln.capsulePaths = runs.size() >= 2 ? runs
                                                  : std::vector<std::vector<PathPt>>{};
                ln.flown = first.flown;
                ln.ceilSegs = first.ceilSegs;
                ln.legUp = first.legUp;
                ln.legCeil = first.legCeil;
                ln.legWall = first.legWall;
                ln.legFloor = first.legFloor;
            }
        }
    }
    // ---- incremental routing: trunks first, then mic cables -----------------
    if (g_corridorMode == CorridorMode::Grid) {
        // grid mode is fully determined by the row allocation above
    } else {
    // Trunks are the obstacles (they cross the whole stage); every mic cable then
    // evaluates BOTH corridor sides (left / right of the target box) and keeps the
    // one with fewer crossings against what is already routed.  A crossing may
    // cost up to 2 m of extra cable (crossings matter more than a metre).
    std::vector<int> sideBy(pr.links.size(), 0);
    std::vector<int> order;
    for (size_t i = 0; i < pr.links.size(); ++i) {
        const CableLink& l = pr.links[i];
        if (l.aKind != 1 && l.bKind != 1) order.push_back((int)i);
    }
    for (size_t i = 0; i < pr.links.size(); ++i) {
        const CableLink& l = pr.links[i];
        if (l.aKind == 1 || l.bKind == 1) order.push_back((int)i);
    }
    auto crossOf = [&](const CableLink& cand, const std::vector<int>& done) {
        int n = 0;
        std::vector<CableLink> tmp;
        tmp.reserve(done.size() + 1);
        for (int k : done) tmp.push_back(pr.links[(size_t)k]);
        tmp.push_back(cand);
        const std::vector<RunRef> runs = CollectRuns(tmp);
        const int last = (int)tmp.size() - 1;
        for (const auto& r : runs) {
            if (r.link != last) continue;
            for (const auto& o : runs) {
                if (o.link == last) continue;
                for (size_t a = 1; a < r.pts->size(); ++a)
                    for (size_t b = 1; b < o.pts->size(); ++b)
                        if (SegCrosses((*r.pts)[a - 1], (*r.pts)[a], (*o.pts)[b - 1],
                                       (*o.pts)[b]))
                            ++n;
            }
        }
        return n;
    };
    std::vector<int> done;
    for (int li : order) {
        CableLink& ln = pr.links[(size_t)li];
        {
            const int mid = (ln.aKind == 1) ? ln.a : ln.b;
            const double mr = (ln.aKind == 1 || ln.bKind == 1)
                                  ? micRadius * MicMarkerScaleOf(pr, mid) : micRadius;
            BuildCablePathEx(pr, ln, lane[(size_t)li], mr, 0, -1.0);
        }
        CableLink candA = ln;
        int bestCross = crossOf(candA, done);
        double bestL = CablePolylineLen(candA);
        int bestSide = 1;

        ln = candA;
        sideBy[(size_t)li] = bestSide;
        done.push_back(li);
    }

    auto build = [&](const std::vector<int>& lanes) {
        for (size_t i = 0; i < pr.links.size(); ++i) {
            CableLink& ln = pr.links[i];
            BuildCablePathEx(pr, ln, lanes[i], micRadius, sideBy[i], -1.0);
        }
    };

    // ---- crossing-reduction pass (≤5 passes, deterministic) -----------------
    int best = CountCrossings(pr.links);
    double bestLen = TotalRunLen(pr.links);
    for (int pass = 0; pass < 5 && best > 0; ++pass) {
        bool improved = false;
        for (size_t i = 0; i < pr.links.size() && !improved; ++i) {
            for (int d : {1, -1}) {
                if (lane[i] + d < 0) continue;
                std::vector<int> trial = lane;
                trial[i] += d;
                build(trial);
                const int c = CountCrossings(pr.links);
                const double len = TotalRunLen(pr.links);
                if (c < best && len <= bestLen + 0.5) {
                    best = c;
                    bestLen = len;
                    lane = trial;
                    improved = true;
                    break;
                }
            }
        }
        if (!improved) {
            build(lane);   // restore the best known assignment
            break;
        }
    }
    }   // end Linear-only routing

    // Project::topo is a pure mirror of the manual wiring now
    pr.topo.links = pr.links;
    pr.topo.valid = !pr.links.empty();
}

// ---------------------------------------------------------------------------
// ADAT is gone: there is no patch cable, no protocol and no placement rule.
// ---------------------------------------------------------------------------
bool IsWirelessMicItem(const Item& it) {
    return it.kind == ItemKind::Mic && it.defId == L"lavalier";
}

// ---------------------------------------------------------------------------
// Connection exclusion matrix (排异).  ONE function decides every legal pair;
// the right-click path, the project loader and the fuzz hook all use it, so an
// illegal pair can never create a link (and could never create the radio link
// that used to crash the painter).
//
// There is one stagebox kind and no protocol any more, so the whole matrix is:
//   wired mic → stagebox / sound card : OK      wired mic → receiver : NO
//   radio mic → stagebox / sound card : NO      radio mic → receiver : OK
//   receiver  → sound card / stagebox : OK      receiver  → receiver : NO
//   stagebox  → sound card            : OK      stagebox  → stagebox  : NO
//   sound card→ any device            : OK (hub)  sound card → sound card : NO
//   microphone ↔ microphone : NO
// ---------------------------------------------------------------------------
bool CanConnectRule(const Project& pr, int aKind, int aId, int bKind, int bId,
                    std::wstring* why, const CableLink* ignore) {
    if (why) why->clear();
    auto reject = [&](const wchar_t* msg) {
        if (why) *why = msg ? msg : L"";
        return false;
    };
    if (aKind == bKind && aId == bId) return reject(L"");           // same object
    const CableEnd a = ResolveCableEnd(pr, aKind, aId);
    const CableEnd b = ResolveCableEnd(pr, bKind, bId);
    if (!a.ok || !b.ok) return reject(L"");
    const Item* micA = nullptr;
    const Item* micB = nullptr;
    for (const auto& it : pr.items) {
        if (aKind == 1 && it.id == aId) micA = &it;
        if (bKind == 1 && it.id == bId) micB = &it;
    }
    const Device* devA = (aKind == 0) ? pr.FindDevice(aId) : nullptr;
    const Device* devB = (bKind == 0) ? pr.FindDevice(bId) : nullptr;

    // "Already wired" is NOT a legality rule: ONE cable per microphone / per bound
    // group is enforced by App::CreateCable, which REPLACES an existing cable when
    // the same microphone is connected again (that is how a microphone is re-routed
    // after a binding change without deleting the old route first).  The exclusion
    // matrix below therefore only judges the PAIR (mic ↔ mic, protocol, device
    // kinds); the loader still passes the link being re-validated as ignore.
    (void)ignore;

    if (micA && micB) return reject(L"");                          // mic → mic

    if (micA || micB) {
        const Item& mic = micA ? *micA : *micB;
        const Device& dev = micA ? *devB : *devA;
        const bool radioMic = IsWirelessMicItem(mic);
        if (dev.kind == DevKind::Receiver) {
            // a radio link: only a wireless microphone feeds a receiver
            if (!radioMic) return reject(L10n::T(S::CbExWiredRx));
            return true;
        }
        if (radioMic) return reject(L10n::T(S::CbExRadioWired));
        return true;   // wired mic → stagebox / sound card
    }

    if (!devA || !devB) return reject(L"");
    const DevKind ka = devA->kind, kb = devB->kind;
    const bool aSc = (ka == DevKind::SoundCard), bSc = (kb == DevKind::SoundCard);
    if (aSc && bSc) return reject(L"");          // the hub never chains to a hub
    if (aSc || bSc) return true;                 // the hub accepts every device
    const bool aRx = (ka == DevKind::Receiver), bRx = (kb == DevKind::Receiver);
    if (aRx && bRx) return reject(L"");          // receiver ↔ receiver
    const bool aBox = DeviceIsStagebox(ka), bBox = DeviceIsStagebox(kb);
    if (aBox && bBox) return reject(L"");        // stagebox ↔ stagebox
    if ((aBox && bRx) || (aRx && bBox)) return true;   // receiver → stagebox
    return reject(L"");
}
bool IsRadioLink(const Project& pr, const CableLink& ln) {
    // a wireless microphone (the lavalier) and a receiver both mean "radio"
    auto micIsWireless = [&](int kind, int id) {
        if (kind != 1) return false;
        for (const auto& it : pr.items)
            if (it.id == id) return it.defId == L"lavalier";
        return false;
    };
    auto devIsReceiver = [&](int kind, int id) {
        if (kind != 0) return false;
        const Device* d = pr.FindDevice(id);
        return d && d->kind == DevKind::Receiver;
    };
    return micIsWireless(ln.aKind, ln.a) || micIsWireless(ln.bKind, ln.b) ||
           devIsReceiver(ln.aKind, ln.a) || devIsReceiver(ln.bKind, ln.b);
}

// ---------------------------------------------------------------------------
// cable lookups (the inspector totals and the channel-table 链路 / 长度 columns
// all read the same records)
// ---------------------------------------------------------------------------
const CableLink* FindMicCable(const std::vector<CableLink>& links, int itemId) {
    for (const auto& l : links) {
        if (l.aKind == 1 && l.a == itemId) return &l;
        if (l.bKind == 1 && l.b == itemId) return &l;
    }
    return nullptr;
}

const CableLink* FindMicCableForItem(const Project& pr,
                                     const std::vector<CableLink>& links,
                                     int itemId) {
    if (const CableLink* exact = FindMicCable(links, itemId)) return exact;
    // a grouped microphone is wired ONCE for the whole MicGroup: every member
    // resolves to that one link, so hover / select / the channel table agree
    const int g = pr.GroupOf(itemId);
    if (g < 0) return nullptr;
    for (int id : pr.micGroups[(size_t)g].ids) {
        if (id == itemId) continue;
        if (const CableLink* ln = FindMicCable(links, id)) return ln;
    }
    return nullptr;
}

const Device* FindTrunkTarget(const Project& pr, const std::vector<CableLink>& links,
                              int devId) {
    for (const auto& l : links) {
        if (l.aKind != 0 || l.bKind != 0) continue;   // trunk = device → device
        const bool a = (l.a == devId);
        const bool b = (l.b == devId);
        if (!a && !b) continue;
        const Device* d = pr.FindDevice(a ? l.b : l.a);
        if (d && d->kind == DevKind::SoundCard) return d;
    }
    return nullptr;
}

namespace {

// the name the operator reads on the canvas / in the device list
std::wstring DevName(const Device& dv) {
    return dv.label.empty() ? DeviceKindTitle(dv.kind) : dv.label;
}

} // namespace

std::wstring CableChainText(const Project& pr, const std::vector<CableLink>& links,
                            const CableLink& ln) {
    // device labels only, no protocol prefix: 「接口箱 1 → 声卡」
    const Device* devA = (ln.aKind == 0) ? pr.FindDevice(ln.a) : nullptr;
    const Device* devB = (ln.bKind == 0) ? pr.FindDevice(ln.b) : nullptr;

    // device → device (the trunk that feeds the sound card)
    if (devA && devB) return DevName(*devA) + L" → " + DevName(*devB);

    const Device* dev = devA ? devA : devB;
    if (!dev) return std::wstring();   // mic → mic never happens

    // the microphone end: its own label, or the group it is bound into
    std::wstring src;
    const int micId = (ln.aKind == 1) ? ln.a : ln.b;
    for (const auto& it : pr.items)
        if (it.id == micId) {
            src = it.label.empty() ? std::wstring(L10n::T(S::TypeMic)) : it.label;
            break;
        }
    if (src.empty()) src = L10n::T(S::TypeMic);
    if (dev->kind == DevKind::SoundCard) return src + L" → " + DevName(*dev);

    // the box feeds a sound card: name that hop
    std::wstring s = DevName(*dev);
    if (const Device* sc = FindTrunkTarget(pr, links, dev->id))
        s += L" → " + DevName(*sc);
    return s;
}

CableStats DeviceCableStats(const Project& pr, const std::vector<CableLink>& links,
                            int devId, bool reserved) {
    CableStats st;
    for (const auto& l : links) {
        if (l.radio) continue;                        // radio links are not cables
        if (l.aKind != 1 && l.bKind != 1) continue;   // microphone cables only
        const bool onDev = (l.aKind == 0 && l.a == devId) ||
                           (l.bKind == 0 && l.b == devId);
        if (!onDev) continue;
        ++st.links;
        st.channels += l.channels;
        st.length += reserved ? CableReservedLen(pr, l) : CableRealLen(pr, l);
    }
    return st;
}

CableStats TotalCableStats(const Project& pr, const std::vector<CableLink>& links,
                           bool reserved) {
    CableStats st;
    for (const auto& l : links) {
        if (l.radio) continue;                        // radio links are not cables
        if (l.aKind != 1 && l.bKind != 1) continue;
        ++st.links;
        st.channels += l.channels;
        st.length += reserved ? CableReservedLen(pr, l) : CableRealLen(pr, l);
    }
    return st;
}
