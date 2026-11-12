#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

enum class ItemKind { Instrument, Mic };

struct Rgb {
    std::uint8_t r{0}, g{0}, b{0};
};

struct Item {
    int id = 0;
    ItemKind kind = ItemKind::Instrument;
    std::wstring defId;
    double x = 0;        // metres, origin at the stage's back-left corner
    double y = 0;
    double angle = 0;    // degrees, microphones only (0 = pointing up / back)
    double height = 1.50; // metres, microphone stand height (mics only)
    double spacing = 0.0; // metres, pair spacing for the main microphone
    double center = 0.0;  // metres, Decca Tree centre mic forward offset
    std::wstring spotType;   // instrument type (defId) this microphone is bound to as a spot mic
    std::wstring modelL, modelR, modelC;   // main mic capsule model text
    std::wstring model;                    // microphone model (spot / technique mics)
    std::wstring remark;                   // free-text remark, shown in the channel table
    int pattern = 1;      // 0 omni, 1 cardioid, 2 supercardioid, 3 figure-8, 4 wide cardioid
    int mainTech = 0;     // main mic technique: 0 AB, 1 mixed, 2 intensity
    // flown (吊装) microphone: hangs from the ceiling at `flyHeight`, so its cable
    // has to go up, along the ceiling to the nearest wall, down and over the floor
    bool flown = false;
    double flyHeight = 6.5;   // metres above the stage floor
    double w = 0;        // footprint, copied from the catalogue and adjustable
    double h = 0;
    std::wstring label;
    // manual cable length (metres, 0 = automatic): the 03 长度 cell is editable and
    // a typed number is stored here, used verbatim on every page and never rounded
    double cableLenOverride = 0.0;
    // per-item colour override, 0xRRGGBB (0 = use the muted palette slot of defId).
    // Set on an INSTRUMENT (which includes the 声部 / section targets): its own icon
    // and the icon tint + microphone / cable / tag ink of the microphones BOUND to
    // it (MicBoundInk) follow this colour.  Serialised as a trailing
    // `color 0xRRGGBB` token pair on the `item` record.
    unsigned colorOverride = 0;
};

struct MicGroup {
    std::vector<int> ids;
    int kind = 0;   // 0 = stereo group, 1 = Decca Tree, 2 = preset technique
    std::wstring tech;   // technique id when kind == 2 (ortf / nos / ...)
};

struct StageDims {
    double w = 14.0;     // metres
    double d = 10.0;
    double off = 5.0;    // surrounding off-stage margin
};

// Flown-microphone cable model (docs/CABLING.md):
//   legUp    = ceiling − height           (up from the hung mic to the ceiling)
//   legCeil  = the horizontal ceiling run to ONE OF THE TWO SIDE WALLS — the
//              side is decided by WHERE THE TARGET DEVICE SITS (left half of the
//              stage → x = 0, right half → x = stage.w; exactly centred → the
//              nearer side wall).  The front / rear walls (y = 0 / y = d) are
//              never used, so the ceiling run is always parallel to the stage
//              front and is measured along x only: |micX − wallX|.
//   legWall  = ceiling − 0                (down that side wall)
//   legFloor = the floor run from the wall base (wallX, micY) to the device,
//              along the corridor rules (edges / centre line)
// The drawn polyline only shows legCeil + legFloor — the two vertical legs
// cannot be expressed in a plan view — so they are stored on the link.
inline double FlownRunLength(double legUp, double legCeil, double legWall,
                             double legFloor) {
    return legUp + legCeil + legWall + legFloor;
}

// ---------------------------------------------------------------------------
// Cabling devices (layer 03)
//
// A device is anything a cable can end on: a microphone, the 「接口箱 /
// Stagebox」, the recording interface (sound card, a hub with unlimited inputs)
// or a wireless receiver.  Devices are deliberately independent of Item: the
// stage plot and the mic plot never draw them.
// ---------------------------------------------------------------------------
// The analogue and the digital stage box are ONE 「接口箱 / Stagebox」 now.  The
// enum keeps both values so an old .smproj still maps its device records: the
// loader turns a DigitalBox into a stagebox (channels = max(channels,
// ports × channelsPerPort)) and drops an Adda device with all its links.
enum class DevKind { Mic, AnalogBox, DigitalBox, SoundCard, Adda, Receiver };

inline bool DeviceIsStagebox(DevKind k) {
    return k == DevKind::AnalogBox || k == DevKind::DigitalBox;
}

// The three 「添加」 buttons of the 05 inspector: 接口箱 / 声卡 / 无线接收器.
inline constexpr int kAddDeviceCount = 3;

struct Device {
    int id = 0;
    DevKind kind = DevKind::AnalogBox;
    double x = 0, y = 0;      // metres, stage coordinates (same origin as Item)
    double angle = 0;         // degrees, file format only
    std::wstring label;       // 名称, e.g. "接口箱 1"
    std::wstring model;       // free-text model (file format only)
    int channels = 16;        // input channels of the box / receiver / sound card
    // file-format only: an old DigitalBox record still carries ports × per-port,
    // the loader folds them into `channels`; nothing else reads them
    int ports = 0;
    int channelsPerPort = 0;
    int faceDir = 0;          // interface face direction: 0 up, 1 right, 2 down, 3 left
    bool selected = false;
    // file format only: nothing sets it any more (the auto-placed ADAT box is
    // gone); a loaded file may still carry a lock
    bool locked = false;
};

// Every device is portless now: a stagebox is one box with a plain input channel
// count, the sound card is a hub and the receiver has its own outputs.  The two
// helpers stay so the painter and the loader can still ask the question.
inline bool DeviceHasPorts(DevKind) { return false; }

// The six sample rates a project may declare, in Hz.  The rate is a FILE field
// only now: the top-bar selector is gone and no capacity / length rule reads it.
inline int SampleRateAt(int i) {
    static const int k[6] = {44100, 48000, 88200, 96000, 176400, 192000};
    return (i >= 0 && i < 6) ? k[i] : 48000;
}
// index of a rate in the table, or -1 when it is not one of the six
inline int SampleRateIndex(int sr) {
    for (int i = 0; i < 6; ++i)
        if (SampleRateAt(i) == sr) return i;
    return -1;
}

// short badge painted inside the device box, language independent (≤5 chars,
// never the ambiguous "DI").  One code for the merged stagebox.
inline const wchar_t* DeviceCode(DevKind k) {
    switch (k) {
    case DevKind::Mic: return L"MC";
    case DevKind::AnalogBox: return L"BOX";
    case DevKind::DigitalBox: return L"BOX";
    case DevKind::SoundCard: return L"SC";
    case DevKind::Receiver: return L"RX";
    }
    return L"??";
}

// full kind name, localized (inspects the same table the old A-BOX / D-BOX /
// ADDA titles used; one 「接口箱」 for both box kinds)
std::wstring DeviceKindTitle(DevKind k);

// --- cables ---------------------------------------------------------------
// A cable joins two connectable objects: a microphone (an Item) or a cabling
// device.  Endpoints are addressed by (kind, id): kind 0 = Project::devices,
// kind 1 = Project::items (microphones), so ids can never collide.
struct CableLink {
    int a = 0, b = 0;         // endpoint ids
    int aKind = 0, bKind = 0; // 0 = cabling device, 1 = microphone item
    // channels the cable carries.  Derived, not a device property: a microphone
    // cable carries the microphone's own count, a device trunk carries
    // min(capacity(A), capacity(B)).  `channelsUser` marks an explicit override,
    // which the derivation clamps to that same minimum.
    int channels = 2;
    bool channelsUser = false;
    // A manual link came from the right-click wiring and is never touched by the
    // auto router.  An auto link is a snapshot: it exists only until the next
    // geometry / connectivity edit, which deletes it again (phase 2).
    bool manual = false;
    // flown source: the first `ceilSegs` polyline segments are the ceiling run
    // (drawn dashed), the rest is the floor run (solid).  The two vertical legs
    // live in legUp / legWall because a plan view cannot show them.
    bool flown = false;
    int ceilSegs = 0;
    double legUp = 0.0, legCeil = 0.0, legWall = 0.0, legFloor = 0.0;
    // polyline in metres (world coordinates); 3 points for the phase-1 dogleg
    std::vector<std::pair<double, double>> path;
    // A bound MicGroup converges BEFORE it travels: every member has a short
    // collector stub to the group's convergence point (the average of the member
    // positions — for a Decca tree the midpoint of the L/R capsules, the centre
    // capsule is excluded from the average but still gets its stub), and `path`
    // is the ONE main run from that convergence point to the target.  The stubs
    // are drawn thin / dashed and carry no tag; their length is part of the
    // cable (see CollectorLen).
    std::vector<std::vector<std::pair<double, double>>> collectorPaths;
    // a radio link (wireless microphone or receiver) is not a cable: drawn dashed,
    // no length limit, no length in the tag or in any total.  Derived on rebuild.
    bool radio = false;
    // A multi-capsule source (the main microphone) is ONE logical cable with one
    // polyline per capsule: path[0] stays in path, every capsule (2 for AB, 3
    // for a Decca tree) gets its own run, channel and length.
    std::vector<std::vector<std::pair<double, double>>> capsulePaths;
};

// Derived routing topology.  There is no auto-routing run any more: the cables
// are exactly the links the user wired with the right mouse button, and this
// mirror is rebuilt from Project::links after every change, so 04 拓扑图 and the
// channel-table 链路 / 长度 columns always read the live wiring.
struct RouteTopology {
    bool valid = false;    // at least one cable exists
    std::vector<CableLink> links;
};

// one resolved cable endpoint
struct CableEnd {
    bool ok = false;
    bool mic = false;
    int id = 0;         // the device / item id behind this endpoint
    double x = 0, y = 0;
    DevKind kind = DevKind::AnalogBox;
    std::wstring name;
    int capacity = 0;   // usable channels of that object
    int required = 0;   // channels the object wants to send / receive
};

// An INTENSITY-DIFFERENCE stereo technique (强度差: XY / MS / Blumlein) is ONE
// stage object with TWO channels: the two capsules are coincident (or nearly so),
// so drawing two separate microphones on the plot says nothing.  The single item
// carries the technique in its defId (the same catalogue id the palette shows),
// the L/R (or M/S) model texts in Item::modelL / Item::modelR, and it occupies
// the two channels of its technique on ONE cable.
inline bool IsIntensityTech(const std::wstring& id) {
    return id == L"xy" || id == L"ms" || id == L"blumlein";
}

struct Project {
    std::wstring piece;
    std::wstring venue;
    std::wstring conductor;
    std::wstring sysEngineer, recEngineer, asstEngineer;
    std::wstring notes;

    StageDims stage;
    double ceiling = 8.0;   // room / stage ceiling height in metres (flown mics)
    std::vector<Item> items;
    int selected = -1;
    int nextId = 1;

    // cabling devices (layer 03)
    std::vector<Device> devices;
    int nextDeviceId = 1;
    // cables between mics and devices (layer 03)
    std::vector<CableLink> links;
    // the last routing snapshot: read by 04 拓扑图 and by the channel table
    RouteTopology topo;

    // multi-selection (ctrl-click) and 2/3 microphone bindings
    std::vector<int> selIds;
    std::vector<MicGroup> micGroups;
    std::vector<int> mainMics;   // mics marked as the main pair
    // MANUAL channel-table row order: the item ids in the order the user dragged
    // the rows into.  Empty = the automatic section order (主话筒 → 侧展 → the
    // instrument sections → 自定义 → 未分配 → 环境); once the user reorders, this
    // list wins and is saved with the project.
    std::vector<int> channelOrder;

    // all colours are user-adjustable from the inspector
    Rgb instColor{0x3A, 0x37, 0x32};
    Rgb micColor{0x55, 0x4C, 0x42};
    Rgb textColor{0x33, 0x30, 0x2B};
    Rgb stageFill{0xFB, 0xF9, 0xF4};
    int sampleRate = 48000;   // file field only: nothing derives a rule from it

    bool showInstruments = true;
    bool showMics = true;
    // TEXT VISIBILITY, per kind of label (two independent switches in 图层):
    //  · showInstrumentText — instrument names + the same-type hull 「型 ×N」 labels
    //  · showMicText        — microphone name / model / capsule L·R·C and the
    //                          connection tags drawn on the 02 / 03 canvas
    // Both are PROJECT data (persisted), default ON, so an older file without them
    // loads with every label visible.
    bool showInstrumentText = true;
    bool showMicText = true;
    // 导出图片 resolution, remembered with the project: 2 = 普通, 4 = 高清 (default),
    // 8 = 超清 — output px per metre as a multiple of the on-screen ppm.  The app
    // has no settings store, so the chooser's choice rides in the .smproj
    // (`pngscale 2|4|8`); a file written before it existed loads as 4.
    int pngScale = 4;
    // 通道表 「话筒线长度」 mode: 0 = 推荐 (the rounded standard cable size, the original
    // behaviour), 1 = 实际 (the real drawn length of that row's cable run — exactly the
    // number the 05 tag shows).  The table header doubles as the switch; persisted as
    // `lenmode 0|1` in the .smproj, default 0.
    int lenMode = 0;

    Item* Selected() {
        for (auto& it : items) if (it.id == selected) return &it;
        return nullptr;
    }
    const Item* Selected() const {
        for (const auto& it : items) if (it.id == selected) return &it;
        return nullptr;
    }
    Item* Find(int id) {
        for (auto& it : items) if (it.id == id) return &it;
        return nullptr;
    }
    const Item* Find(int id) const {
        for (const auto& it : items) if (it.id == id) return &it;
        return nullptr;
    }
    void Deselect() { selected = -1; selIds.clear(); }

    bool IsSel(int id) const {
        return std::find(selIds.begin(), selIds.end(), id) != selIds.end();
    }
    int SelectionCount() const { return (int)selIds.size(); }

    // microphone group helpers
    int GroupOf(int id) const {
        for (int g = 0; g < (int)micGroups.size(); ++g)
            if (std::find(micGroups[g].ids.begin(), micGroups[g].ids.end(), id) !=
                micGroups[g].ids.end())
                return g;
        return -1;
    }
    bool HasMainMicItem() const {
        for (const auto& it : items)
            if (it.defId == L"mainmic") return true;
        return false;
    }
    bool HasDeccaTree() const {
        for (const auto& g : micGroups)
            if (g.kind == 1) return true;
        return false;
    }
    void RemoveFromGroups(int id) {
        for (auto& g : micGroups)
            g.ids.erase(std::remove(g.ids.begin(), g.ids.end(), id), g.ids.end());
        micGroups.erase(std::remove_if(micGroups.begin(), micGroups.end(),
                                       [](const MicGroup& g) {
                                           return g.ids.size() < 2;
                                       }),
                        micGroups.end());
        mainMics.erase(std::remove(mainMics.begin(), mainMics.end(), id),
                       mainMics.end());
    }

    // ---- cabling device helpers -------------------------------------------
    Device* FindDevice(int id) {
        for (auto& d : devices) if (d.id == id) return &d;
        return nullptr;
    }
    const Device* FindDevice(int id) const {
        for (const auto& d : devices) if (d.id == id) return &d;
        return nullptr;
    }
    Device* SelectedDevice() {
        for (auto& d : devices) if (d.selected) return &d;
        return nullptr;
    }
    const Device* SelectedDevice() const {
        for (const auto& d : devices) if (d.selected) return &d;
        return nullptr;
    }
    void ClearDeviceSelection() {
        for (auto& d : devices) d.selected = false;
    }
    void SelectDevice(int id) {
        for (auto& d : devices) d.selected = (d.id == id);
    }
    // no device has ports or a protocol any more: the ceiling is always 0
    int DevicePortCap(const Device&) const { return 0; }
    // A sound card is a hub: it never runs out of inputs, so its capacity is
    // unlimited and it is never a capacity constraint on a cable.
    static constexpr int kInfiniteCapacity = 1 << 20;
    static bool DeviceIsHub(const Device& d) { return d.kind == DevKind::SoundCard; }
    // total usable channels: the declared input count, except the hub (unlimited)
    // and the receiver (its own channel count, one cable per wireless channel)
    int DeviceCapacity(const Device& d) const {
        if (DeviceIsHub(d)) return kInfiniteCapacity;   // hub: unlimited inputs
        if (d.kind == DevKind::Receiver) return d.channels;
        return (std::max)(1, d.channels);
    }
    bool DeviceOverCapacity(const Device& d) const {
        if (DeviceIsHub(d)) return false;               // the hub never overflows
        return d.channels > DeviceCapacity(d);
    }

    // a deleted object must not leave dangling cables behind
    void RemoveLinksOf(int kind, int id) {
        links.erase(std::remove_if(links.begin(), links.end(),
                                   [&](const CableLink& l) {
                                       return (l.aKind == kind && l.a == id) ||
                                              (l.bKind == kind && l.b == id);
                                   }),
                    links.end());
    }
};

// Drawn footprint of a cabling device in metres.  The painter and the router
// share these so a cable really ends on the rectangle boundary.
inline constexpr double kCableDevW = 1.00;
inline constexpr double kCableDevH = 0.68;
// Fallback microphone marker radius in metres (the app passes the real one, which
// depends on the current zoom, so the cable touches the drawn circle edge).
inline constexpr double kCableMicRadius = 0.12;

// --- cable helpers (implemented in src/Model.cpp) ---------------------------
CableEnd ResolveCableEnd(const Project& pr, int kind, int id);
// Channels a microphone item needs (1, 2 for the main pair, 3 for a Decca tree).
int MicItemChannels(const Item& it);
// Channels a cable actually carries: the microphone's own count, or (device ↔
// device) min(capacity(A), capacity(B)) — a trunk never demands more than
// either end can handle.  An explicit override is clamped to the same minimum.
int DerivedCableChannels(const Project& pr, const CableLink& ln);
// Signal direction: microphones → stage boxes / receivers → digital gear →
// sound card.  Used to tell the upstream end of a trunk from the downstream one.
int DeviceFlowRank(DevKind k);
// Channels that arrive at a device from microphones and upstream links.
int DeviceLoad(const Project& pr, const std::vector<CableLink>& links, int devId);
// The device receives more channels than it has inputs.
bool DeviceOverloaded(const Project& pr, const std::vector<CableLink>& links,
                      int devId);
// The SHORTEST route from the two exit points: straight when aligned, otherwise
// ONE corner (vertical-first; one 45° cut in the rear = upstage half, y <
// stage.d/2 — y = 0 is the back edge).  A crossing is never bought with a longer
// cable; the only detour is a device rectangle in the way.  A flown (吊装)
// microphone goes up to the ceiling, to the nearest wall, down the wall and over
// the floor; the two vertical legs are stored on the link because a plan view
// cannot show them.
// `lane` spreads parallel runs, `micRadius` is the drawn mic marker radius so the
// cable terminates on the marker edge instead of short of it.
// routing model: Grid = shortest direct router (default), Linear = legacy corridor
enum class CorridorMode { Linear, Grid };
extern CorridorMode g_corridorMode;
void BuildCablePath(const Project& pr, CableLink& ln, int lane = 0,
                    double micRadius = kCableMicRadius);
// Rebuild every link's path + the derived topology from the endpoints.  Cheap
// (no assignment search), so it runs after any move / edit.
void RebuildAllCablePaths(Project& pr, double micRadius = kCableMicRadius);
// axis-aligned crossings between different cables (capsule runs of one cable
// are never counted) and the total length of every routed run
int CountCrossings(const std::vector<CableLink>& links);
double TotalRunLen(const std::vector<CableLink>& links);
// A radio link: one end is a wireless microphone (the lavalier) or a receiver.
bool IsRadioLink(const Project& pr, const CableLink& ln);
// A microphone is wireless when it is a radio mic (the lavalier).
bool IsWirelessMicItem(const Item& it);
// Connection exclusion matrix (排异): the ONE place that decides whether two
// objects may be wired.  `why` receives the localized reason on rejection.
// `ignore` lets the project loader re-validate a LINK THAT ALREADY EXISTS: that
// very link is in pr.links, so without it the "already wired" rule would reject
// every stored cable and an opened project would come back unwired.
bool CanConnectRule(const Project& pr, int aKind, int aId, int bKind, int bId,
                    std::wstring* why, const CableLink* ignore = nullptr);
double CablePolylineLen(const CableLink& ln);
// raw 2-D length of one polyline (used for a single capsule run)
double PolyLen(const std::vector<std::pair<double, double>>& p);
// Total length of a converged group's collector stubs (0 when the cable has no
// convergence).  Part of the physical cable: CableRawLen adds it.
double CollectorLen(const CableLink& ln);
// The convergence point of a microphone GROUP: the average of the member
// positions.  A Decca tree (3 members) converges at the MIDPOINT OF THE TWO OUTER
// CAPSULES — the centre capsule is excluded from the average because it is the
// one protruding forward — but still contributes its own collector stub.  A
// microphone that is not in a group returns its own position.
std::pair<double, double> GroupConvergencePos(const Project& pr, int itemId);
// capsules of a microphone: 2 for the main mic's AB/L-R, 3 for its Decca tree,
// 1 for every other microphone (positions are world coordinates along its axis)
int MicCapsuleCount(const Item& it);
std::vector<std::pair<double, double>> MicCapsulePos(const Item& it);
// The cable terminal / 03 marker radius of one microphone, as a multiple of the
// fixed marker radius: the main mic 1.3, a point mic 0.85 (0.5 × the 1.7× symbol
// growth), the lavalier 1.4875.  One factor for the drawn circle AND the cable
// terminal, so the line always stops on the circle.
double MicMarkerScaleOf(const Project& pr, int micId);
// A microphone group (02 绑定) is wired as ONE cable: the positions of all its
// members, so every member gets its own parallel run on that one link.  A lone
// microphone returns its own position.
std::vector<std::pair<double, double>> GroupMemberPos(const Project& pr, int itemId);
// --- cable length pipeline --------------------------------------------------
// REAL length (05 布线): what you physically need — the microphone's own height
// above the floor plus the floor run, or the sum of the four flown legs.  No
// slack, no end allowance, no reserve.
double CableRawLen(const Project& pr, const CableLink& ln);
// The microphone height that is added to a floor run (0 for a device trunk).
double CableMicHeight(const Project& pr, const CableLink& ln);
// RESERVED length (03 通道表 / 04 拓扑图 / PDF): the raw length rounded UP to the
// next standard size 5/8/10/12/15/18/20/25/30 m (above 30 m: next multiple of 5).
double StandardCableLen(double raw);
// A manual per-microphone override (Item::cableLenOverride, 0 = automatic).  It
// wins on EVERY page and is never rounded.
double CableOverrideLen(const Project& pr, const CableLink& ln);
// The two numbers the pages show: 实长 on 05, 含预留 on 03 / 04.
double CableRealLen(const Project& pr, const CableLink& ln);
double CableReservedLen(const Project& pr, const CableLink& ln);
// The REAL length of the one drawn RUN that belongs to `micId` (05 布线 tags): a
// bound group's cable draws one run per member and a main microphone one run per
// capsule, so the per-run number differs from the whole-link CableRealLen.  `capsule`
// selects the main microphone's capsule position (-1 = the microphone's own position).
// Falls back to the link's real length when the run cannot be identified.  This is
// the 03 通道表 「实际」 cell.
double CableRunRealLen(const Project& pr, const CableLink& ln, int micId,
                       int capsule = -1);

// --- validation: channel budget --------------------------------------------
struct CableIssue {
    bool overChannels = false;
    double length = 0.0;      // CableTotalLen
    // when a device endpoint is overloaded: its name, the channels it already
    // receives from the wiring and its capacity (for the hover reason)
    std::wstring overName;
    int overLoad = 0;
    int overCap = 0;
    bool overByLoad = false;  // true: the whole input set is over budget
    bool radio = false;       // a radio link: no cable, no length rule
};
CableIssue CheckCable(const Project& pr, const CableLink& ln);
// Fills a localized reason (channels first) and returns true when the link
// violates one of the rules.
bool CableProblemText(const Project& pr, const CableLink& ln, std::wstring* why);

// --- cable lookups shared by the router, the inspector and the channel table --
// They take the link list explicitly so the drawn cables (Project::links) and the
// stored topology (Project::topo.links) can both be queried with one code path.
// The cable that feeds a microphone (endpoint kind 1 = Item), or nullptr.
const CableLink* FindMicCable(const std::vector<CableLink>& links, int itemId);
// Group-aware lookup: a grouped microphone is wired once for the whole MicGroup,
// so any member finds that one link (hover / select / channel table).
const CableLink* FindMicCableForItem(const Project& pr,
                                     const std::vector<CableLink>& links, int itemId);
// The sound card a device feeds (the trunk link), or nullptr.
const Device* FindTrunkTarget(const Project& pr, const std::vector<CableLink>& links,
                              int devId);
// "接口箱 1 → 声卡" — device labels only, the 链路 column text.
std::wstring CableChainText(const Project& pr, const std::vector<CableLink>& links,
                            const CableLink& ln);
// channels / metres of the microphone cables that land on one device
struct CableStats {
    int links = 0;
    int channels = 0;
    double length = 0.0;
};
CableStats DeviceCableStats(const Project& pr, const std::vector<CableLink>& links,
                            int devId, bool reserved);
CableStats TotalCableStats(const Project& pr, const std::vector<CableLink>& links,
                           bool reserved);

// A believable orchestra plot for the first milestone.
void BuildDemoProject(Project& pr, bool zh);
void BuildEmptyProject(Project& pr);   // blank startup project (no demo content)

// OLD FILES: an intensity-difference technique used to be stored as TWO point
// microphones in a MicGroup of kind 2 (tech = xy / ms / blumlein).  The current
// model is ONE stage object, so the loader (and every undo snapshot, which runs
// through the same path) merges such a pair: position = the members' midpoint,
// angle = their mean angle, the two 型号 texts → modelL / modelR, the id of the
// first member kept (a cable that pointed at either member is re-pointed at it),
// the group dissolved.  The channel count stays 2 and the object stays ONE.
void MergeIntensityPairs(Project& pr);

// ---------------------------------------------------------------------------
// .smproj — the project file.  UTF-8, one record per line, "key value" style,
// text fields double quoted, no external libraries (src/ProjectIO.cpp).
// ---------------------------------------------------------------------------
bool SaveProjectFile(const Project& pr, const std::wstring& path,
                     std::wstring& err);
bool LoadProjectFile(Project& pr, const std::wstring& path, std::wstring& err,
                     int* skippedUnknown = nullptr);
// The same .smproj TEXT, in memory: the file writer, the parser and the undo
// snapshots share these, so a snapshot can never drift from a saved file.
std::wstring SerializeProject(const Project& pr);
bool ParseProject(const std::wstring& text, Project& pr, std::wstring& err,
                  int* skippedUnknown = nullptr);
