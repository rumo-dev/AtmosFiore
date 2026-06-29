#pragma once

//=============================================================================
//  AudioEffect.h
//  音響エフェクトの種別・パラメータ定義
//  依存: XAudio2, X3DAudio, nlohmann/json
//=============================================================================

#include <string>
#include <variant>
#include <vector>
#include "Engine/Graphics/Model/Tiny/Json.hpp"

using json = nlohmann::json;

//-----------------------------------------------------------------------------
//  エフェクト種別
//-----------------------------------------------------------------------------
enum class AudioEffectType
{
    None        = 0,
    LowPassFilter,   // ローパスフィルタ
    HighPassFilter,  // ハイパスフィルタ
    Reverb,          // リバーブ (XAPO: ReverbEffect)
    Echo,            // エコー  (XAPO: Echo)
    Equalizer,       // イコライザ (XAPO: FXEQ)
    Volume,          // ボリューム
    Pitch,           // ピッチシフト
    Spatialize,      // 3D空間定位 (X3DAudio)
};

inline const char* AudioEffectTypeName(AudioEffectType t)
{
    switch (t)
    {
    case AudioEffectType::LowPassFilter:  return "LowPassFilter";
    case AudioEffectType::HighPassFilter: return "HighPassFilter";
    case AudioEffectType::Reverb:         return "Reverb";
    case AudioEffectType::Echo:           return "Echo";
    case AudioEffectType::Equalizer:      return "Equalizer";
    case AudioEffectType::Volume:         return "Volume";
    case AudioEffectType::Pitch:          return "Pitch";
    case AudioEffectType::Spatialize:     return "Spatialize";
    default:                              return "None";
    }
}

inline AudioEffectType AudioEffectTypeFromString(const std::string& s)
{
    if (s == "LowPassFilter")  return AudioEffectType::LowPassFilter;
    if (s == "HighPassFilter") return AudioEffectType::HighPassFilter;
    if (s == "Reverb")         return AudioEffectType::Reverb;
    if (s == "Echo")           return AudioEffectType::Echo;
    if (s == "Equalizer")      return AudioEffectType::Equalizer;
    if (s == "Volume")         return AudioEffectType::Volume;
    if (s == "Pitch")          return AudioEffectType::Pitch;
    if (s == "Spatialize")     return AudioEffectType::Spatialize;
    return AudioEffectType::None;
}

//-----------------------------------------------------------------------------
//  各エフェクトのパラメータ構造体
//-----------------------------------------------------------------------------

struct LowPassFilterParam
{
    float frequency = 8000.0f;  // Hz
    float oneOverQ  = 1.0f;

    json ToJson() const {
        return { {"frequency", frequency}, {"oneOverQ", oneOverQ} };
    }
    static LowPassFilterParam FromJson(const json& j) {
        LowPassFilterParam p;
        if (j.contains("frequency")) p.frequency = j["frequency"].get<float>();
        if (j.contains("oneOverQ"))  p.oneOverQ  = j["oneOverQ"].get<float>();
        return p;
    }
};

struct HighPassFilterParam
{
    float frequency = 200.0f;
    float oneOverQ  = 1.0f;

    json ToJson() const {
        return { {"frequency", frequency}, {"oneOverQ", oneOverQ} };
    }
    static HighPassFilterParam FromJson(const json& j) {
        HighPassFilterParam p;
        if (j.contains("frequency")) p.frequency = j["frequency"].get<float>();
        if (j.contains("oneOverQ"))  p.oneOverQ  = j["oneOverQ"].get<float>();
        return p;
    }
};

struct ReverbParam
{
    float wetDryMix   = 50.0f;   // 0-100
    float roomSize    = 0.6f;    // 0-1
    float decayTime   = 1.5f;    // 秒
    float earlyDelay  = 20.0f;   // ms
    float lateDelay   = 30.0f;   // ms
    float density     = 100.0f;  // 0-100
    float lowEQGain   = 8.0f;
    float highEQGain  = 8.0f;

    json ToJson() const {
        return {
            {"wetDryMix",  wetDryMix},
            {"roomSize",   roomSize},
            {"decayTime",  decayTime},
            {"earlyDelay", earlyDelay},
            {"lateDelay",  lateDelay},
            {"density",    density},
            {"lowEQGain",  lowEQGain},
            {"highEQGain", highEQGain}
        };
    }
    static ReverbParam FromJson(const json& j) {
        ReverbParam p;
        if (j.contains("wetDryMix"))  p.wetDryMix  = j["wetDryMix"].get<float>();
        if (j.contains("roomSize"))   p.roomSize   = j["roomSize"].get<float>();
        if (j.contains("decayTime"))  p.decayTime  = j["decayTime"].get<float>();
        if (j.contains("earlyDelay")) p.earlyDelay = j["earlyDelay"].get<float>();
        if (j.contains("lateDelay"))  p.lateDelay  = j["lateDelay"].get<float>();
        if (j.contains("density"))    p.density    = j["density"].get<float>();
        if (j.contains("lowEQGain"))  p.lowEQGain  = j["lowEQGain"].get<float>();
        if (j.contains("highEQGain")) p.highEQGain = j["highEQGain"].get<float>();
        return p;
    }
};

struct EchoParam
{
    float wetDryMix = 50.0f;
    float feedback  = 0.5f;   // 0-1
    float delay     = 250.0f; // ms

    json ToJson() const {
        return { {"wetDryMix", wetDryMix}, {"feedback", feedback}, {"delay", delay} };
    }
    static EchoParam FromJson(const json& j) {
        EchoParam p;
        if (j.contains("wetDryMix")) p.wetDryMix = j["wetDryMix"].get<float>();
        if (j.contains("feedback"))  p.feedback  = j["feedback"].get<float>();
        if (j.contains("delay"))     p.delay     = j["delay"].get<float>();
        return p;
    }
};

struct EqualizerParam
{
    // 4バンド
    float gain0 = 1.0f;    float freq0 = 100.0f;  float bw0 = 1.0f;
    float gain1 = 1.0f;    float freq1 = 800.0f;  float bw1 = 1.0f;
    float gain2 = 1.0f;    float freq2 = 3000.0f; float bw2 = 1.0f;
    float gain3 = 1.0f;    float freq3 = 10000.0f;float bw3 = 1.0f;

    json ToJson() const {
        return {
            {"gain0",gain0},{"freq0",freq0},{"bw0",bw0},
            {"gain1",gain1},{"freq1",freq1},{"bw1",bw1},
            {"gain2",gain2},{"freq2",freq2},{"bw2",bw2},
            {"gain3",gain3},{"freq3",freq3},{"bw3",bw3},
        };
    }
    static EqualizerParam FromJson(const json& j) {
        EqualizerParam p;
        auto g = [&](const char* k, float& v){ if(j.contains(k)) v = j[k].get<float>(); };
        g("gain0",p.gain0); g("freq0",p.freq0); g("bw0",p.bw0);
        g("gain1",p.gain1); g("freq1",p.freq1); g("bw1",p.bw1);
        g("gain2",p.gain2); g("freq2",p.freq2); g("bw2",p.bw2);
        g("gain3",p.gain3); g("freq3",p.freq3); g("bw3",p.bw3);
        return p;
    }
};

struct VolumeParam
{
    float volume = 1.0f; // 0-2

    json ToJson() const { return { {"volume", volume} }; }
    static VolumeParam FromJson(const json& j) {
        VolumeParam p;
        if (j.contains("volume")) p.volume = j["volume"].get<float>();
        return p;
    }
};

struct PitchParam
{
    float semitones = 0.0f; // -24 ~ +24

    json ToJson() const { return { {"semitones", semitones} }; }
    static PitchParam FromJson(const json& j) {
        PitchParam p;
        if (j.contains("semitones")) p.semitones = j["semitones"].get<float>();
        return p;
    }
};

struct SpatializeParam
{
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float velX = 0.0f, velY = 0.0f, velZ = 0.0f;
    float innerRadius      = 5.0f;
    float outerRadius      = 100.0f;
    float innerRadiusAngle = 0.0f;
    float maxDistance      = 200.0f;

    json ToJson() const {
        return {
            {"posX",posX},{"posY",posY},{"posZ",posZ},
            {"velX",velX},{"velY",velY},{"velZ",velZ},
            {"innerRadius",innerRadius},{"outerRadius",outerRadius},
            {"innerRadiusAngle",innerRadiusAngle},{"maxDistance",maxDistance}
        };
    }
    static SpatializeParam FromJson(const json& j) {
        SpatializeParam p;
        auto g = [&](const char* k, float& v){ if(j.contains(k)) v = j[k].get<float>(); };
        g("posX",p.posX); g("posY",p.posY); g("posZ",p.posZ);
        g("velX",p.velX); g("velY",p.velY); g("velZ",p.velZ);
        g("innerRadius",p.innerRadius); g("outerRadius",p.outerRadius);
        g("innerRadiusAngle",p.innerRadiusAngle); g("maxDistance",p.maxDistance);
        return p;
    }
};

//-----------------------------------------------------------------------------
//  エフェクトノード (imnodes 1ノード = 1エフェクト)
//-----------------------------------------------------------------------------
using AudioEffectParam = std::variant<
    std::monostate,
    LowPassFilterParam,
    HighPassFilterParam,
    ReverbParam,
    EchoParam,
    EqualizerParam,
    VolumeParam,
    PitchParam,
    SpatializeParam
>;

struct AudioEffectNode
{
    int            nodeId   = -1;   // imnodes 用 ID
    AudioEffectType type    = AudioEffectType::None;
    bool           enabled  = true;
    AudioEffectParam param  = std::monostate{};

    //  JSON <-> struct 変換
    json ToJson() const
    {
        json j;
        j["nodeId"]  = nodeId;
        j["type"]    = AudioEffectTypeName(type);
        j["enabled"] = enabled;

        std::visit([&](auto&& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (!std::is_same_v<T, std::monostate>)
                j["param"] = p.ToJson();
            else
                j["param"] = json::object();
        }, param);

        return j;
    }

    static AudioEffectNode FromJson(const json& j)
    {
        AudioEffectNode n;
        if (j.contains("nodeId"))  n.nodeId  = j["nodeId"].get<int>();
        if (j.contains("enabled")) n.enabled = j["enabled"].get<bool>();

        n.type = AudioEffectTypeFromString(
            j.value("type", std::string("None")));

        const json& p = j.contains("param") ? j["param"] : json::object();
        switch (n.type)
        {
        case AudioEffectType::LowPassFilter:  n.param = LowPassFilterParam::FromJson(p);  break;
        case AudioEffectType::HighPassFilter: n.param = HighPassFilterParam::FromJson(p); break;
        case AudioEffectType::Reverb:         n.param = ReverbParam::FromJson(p);         break;
        case AudioEffectType::Echo:           n.param = EchoParam::FromJson(p);           break;
        case AudioEffectType::Equalizer:      n.param = EqualizerParam::FromJson(p);      break;
        case AudioEffectType::Volume:         n.param = VolumeParam::FromJson(p);         break;
        case AudioEffectType::Pitch:          n.param = PitchParam::FromJson(p);          break;
        case AudioEffectType::Spatialize:     n.param = SpatializeParam::FromJson(p);     break;
        default: break;
        }
        return n;
    }
};

//-----------------------------------------------------------------------------
//  エフェクトチェーン (ノードリスト + リンク情報)
//-----------------------------------------------------------------------------
struct AudioEffectLink
{
    int linkId   = -1;
    int fromNode = -1;
    int toNode   = -1;

    json ToJson() const {
        return { {"linkId",linkId}, {"fromNode",fromNode}, {"toNode",toNode} };
    }
    static AudioEffectLink FromJson(const json& j) {
        AudioEffectLink l;
        if (j.contains("linkId"))   l.linkId   = j["linkId"].get<int>();
        if (j.contains("fromNode")) l.fromNode = j["fromNode"].get<int>();
        if (j.contains("toNode"))   l.toNode   = j["toNode"].get<int>();
        return l;
    }
};

struct AudioEffectChain
{
    std::string                   name;      // チェーン識別名
    std::vector<AudioEffectNode>  nodes;
    std::vector<AudioEffectLink>  links;

    json ToJson() const
    {
        json j;
        j["name"] = name;
        j["nodes"] = json::array();
        for (auto& n : nodes) j["nodes"].push_back(n.ToJson());
        j["links"] = json::array();
        for (auto& l : links) j["links"].push_back(l.ToJson());
        return j;
    }

    static AudioEffectChain FromJson(const json& j)
    {
        AudioEffectChain c;
        c.name = j.value("name", std::string(""));
        if (j.contains("nodes"))
            for (auto& jn : j["nodes"]) c.nodes.push_back(AudioEffectNode::FromJson(jn));
        if (j.contains("links"))
            for (auto& jl : j["links"]) c.links.push_back(AudioEffectLink::FromJson(jl));
        return c;
    }

    //  ノードをトポロジカル順に並べて返す (チェーン適用順)
    std::vector<int> TopologicalOrder() const
    {
        // 単純な線形チェーン想定。リンクを辿って順序を構築
        if (nodes.empty()) return {};

        // 入次数0のノードを起点にする
        std::vector<int> order;
        std::vector<int> nodeIds;
        for (auto& n : nodes) nodeIds.push_back(n.nodeId);

        std::vector<bool> visited(nodes.size(), false);

        // 入力エッジがないノードを起点とする
        auto hasIncoming = [&](int id) {
            for (auto& l : links) if (l.toNode == id) return true;
            return false;
        };

        std::vector<int> starts;
        for (auto& n : nodes)
            if (!hasIncoming(n.nodeId)) starts.push_back(n.nodeId);

        // BFS
        std::vector<int> queue = starts;
        while (!queue.empty())
        {
            int cur = queue.front(); queue.erase(queue.begin());
            order.push_back(cur);
            for (auto& l : links)
                if (l.fromNode == cur) queue.push_back(l.toNode);
        }
        return order;
    }
};
