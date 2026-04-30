#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <bcrypt.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <set>
#include <utility>
#include <variant>
#include <vector>

namespace ffw {

using ByteVec = std::vector<unsigned char>;

static const wchar_t* kWindowClass = L"FarFarWestUnlockAllToolWindow";
static const wchar_t* kWindowTitle = L"FarFarWest Unlock all tool";
static const wchar_t* kPartySuffix = L"NicoArnoEvilRaptorFireshineRobbo";
static const int kInt32Max = 2147483647;

static const wchar_t* kBuildableWeapons[] = {
    L"itemBoomerang",
    L"itemBow",
    L"itemDualRevolver",
    L"itemLongRanger",
    L"itemMinigun",
    L"itemPistol",
    L"itemQuadCylinder",
    L"itemSheriffStar",
    L"itemShotgun",
    L"itemWinchester",
};

static const wchar_t* kSpellItems[] = {
    L"itemAcid",
    L"itemCactus",
    L"itemElec",
    L"itemFire",
    L"itemVoodoo",
};

static const wchar_t* kUtilityItems[] = {
    L"itemUtilityAmmo",
    L"itemUtilityBottleCrate",
    L"itemUtilityHealBottle",
    L"itemUtilityImpulse",
};

struct ColorPalette {
    COLORREF bgTop = RGB(10, 7, 20);
    COLORREF bgBottom = RGB(28, 8, 48);
    COLORREF panel = RGB(22, 20, 38);
    COLORREF panelAlt = RGB(28, 24, 48);
    COLORREF border = RGB(88, 64, 140);
    COLORREF accent = RGB(168, 92, 255);
    COLORREF accentSoft = RGB(108, 66, 168);
    COLORREF text = RGB(245, 240, 255);
    COLORREF textMuted = RGB(188, 176, 216);
    COLORREF success = RGB(96, 214, 168);
};

static ColorPalette gColors;

static std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), NULL, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

static std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size, NULL, NULL);
    return out;
}

static std::wstring Trim(const std::wstring& value) {
    size_t start = 0;
    size_t end = value.size();
    while (start < end && iswspace(value[start])) ++start;
    while (end > start && iswspace(value[end - 1])) --end;
    return value.substr(start, end - start);
}

static bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), value.begin());
}

static bool EndsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

static std::string ShortName(const std::string& name) {
    size_t pos = name.find('_');
    return pos == std::string::npos ? name : name.substr(0, pos);
}

static std::string FriendlyItemName(const std::string& itemName) {
    std::string name = itemName;
    if (StartsWith(name, "item")) {
        name = name.substr(4);
    }
    std::string out;
    for (size_t i = 0; i < name.size(); ++i) {
        if (i > 0 && isupper(static_cast<unsigned char>(name[i])) && islower(static_cast<unsigned char>(name[i - 1]))) {
            out.push_back(' ');
        }
        out.push_back(name[i]);
    }
    return out;
}

static bool NameMatchesList(const std::string& itemName, const wchar_t* const* values, size_t count) {
    std::wstring wide = Utf8ToWide(itemName);
    for (size_t i = 0; i < count; ++i) {
        if (wide == values[i]) return true;
    }
    return false;
}

static std::wstring JoinLines(const std::vector<std::wstring>& lines) {
    std::wostringstream oss;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) oss << L"\r\n";
        oss << lines[i];
    }
    return oss.str();
}

struct Reader {
    const ByteVec* buffer = NULL;
    size_t pos = 0;

    explicit Reader(const ByteVec& bytes) : buffer(&bytes), pos(0) {}

    void Require(size_t count) const {
        if (pos + count > buffer->size()) {
            throw std::runtime_error("Unexpected end of file");
        }
    }

    ByteVec ReadBytes(size_t count) {
        Require(count);
        ByteVec out(buffer->begin() + static_cast<long long>(pos),
                    buffer->begin() + static_cast<long long>(pos + count));
        pos += count;
        return out;
    }

    unsigned char U8() {
        Require(1);
        return (*buffer)[pos++];
    }

    std::int16_t I16() {
        Require(2);
        std::int16_t out = static_cast<std::int16_t>((*buffer)[pos] | ((*buffer)[pos + 1] << 8));
        pos += 2;
        return out;
    }

    std::int32_t I32() {
        Require(4);
        std::int32_t out =
            static_cast<std::int32_t>((*buffer)[pos]) |
            (static_cast<std::int32_t>((*buffer)[pos + 1]) << 8) |
            (static_cast<std::int32_t>((*buffer)[pos + 2]) << 16) |
            (static_cast<std::int32_t>((*buffer)[pos + 3]) << 24);
        pos += 4;
        return out;
    }

    std::int64_t I64() {
        Require(8);
        std::uint64_t out = 0;
        for (int i = 0; i < 8; ++i) {
            out |= static_cast<std::uint64_t>((*buffer)[pos + i]) << (8 * i);
        }
        pos += 8;
        return static_cast<std::int64_t>(out);
    }

    std::string FString() {
        std::int32_t n = I32();
        if (n == 0) return "";
        if (n > 0) {
            ByteVec bytes = ReadBytes(static_cast<size_t>(n));
            if (!bytes.empty() && bytes.back() == 0) bytes.pop_back();
            return std::string(bytes.begin(), bytes.end());
        }
        int wcharCount = -n;
        ByteVec bytes = ReadBytes(static_cast<size_t>(wcharCount) * 2u);
        if (bytes.size() >= 2 && bytes[bytes.size() - 2] == 0 && bytes[bytes.size() - 1] == 0) {
            bytes.resize(bytes.size() - 2);
        }
        std::wstring wide;
        wide.resize(bytes.size() / 2);
        memcpy(&wide[0], &bytes[0], bytes.size());
        return WideToUtf8(wide);
    }

    bool PlausibleFStringAt(size_t at) const {
        if (at + 4 > buffer->size()) return false;
        if ((*buffer)[at] == 0) return false;
        std::int32_t n =
            static_cast<std::int32_t>((*buffer)[at]) |
            (static_cast<std::int32_t>((*buffer)[at + 1]) << 8) |
            (static_cast<std::int32_t>((*buffer)[at + 2]) << 16) |
            (static_cast<std::int32_t>((*buffer)[at + 3]) << 24);
        if (n == 0) return true;
        if (n > 0) {
            if (n > 4096 || at + 4 + static_cast<size_t>(n) > buffer->size()) return false;
            return (*buffer)[at + 4 + static_cast<size_t>(n) - 1] == 0;
        }
        n = -n;
        return n <= 4096 && at + 4 + static_cast<size_t>(n) * 2u <= buffer->size();
    }
};

struct Writer {
    ByteVec bytes;

    void WriteBytes(const ByteVec& data) {
        bytes.insert(bytes.end(), data.begin(), data.end());
    }

    void WriteU8(unsigned char value) {
        bytes.push_back(value);
    }

    void WriteI16(std::int16_t value) {
        bytes.push_back(static_cast<unsigned char>(value & 0xFF));
        bytes.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
    }

    void WriteI32(std::int32_t value) {
        for (int i = 0; i < 4; ++i) {
            bytes.push_back(static_cast<unsigned char>((value >> (8 * i)) & 0xFF));
        }
    }

    void WriteI64(std::int64_t value) {
        std::uint64_t raw = static_cast<std::uint64_t>(value);
        for (int i = 0; i < 8; ++i) {
            bytes.push_back(static_cast<unsigned char>((raw >> (8 * i)) & 0xFF));
        }
    }

    void WriteFString(const std::string& value) {
        bool ascii = true;
        for (size_t i = 0; i < value.size(); ++i) {
            if (static_cast<unsigned char>(value[i]) > 0x7F) {
                ascii = false;
                break;
            }
        }
        if (value.empty()) {
            WriteI32(0);
            return;
        }
        if (ascii) {
            WriteI32(static_cast<std::int32_t>(value.size() + 1));
            bytes.insert(bytes.end(), value.begin(), value.end());
            bytes.push_back(0);
            return;
        }

        std::wstring wide = Utf8ToWide(value);
        WriteI32(-static_cast<std::int32_t>(wide.size() + 1));
        const unsigned char* raw = reinterpret_cast<const unsigned char*>(wide.data());
        bytes.insert(bytes.end(), raw, raw + wide.size() * sizeof(wchar_t));
        bytes.push_back(0);
        bytes.push_back(0);
    }
};

struct MetaInfo;
struct Value;
struct Property;

typedef std::shared_ptr<MetaInfo> MetaInfoPtr;
typedef std::shared_ptr<Value> ValuePtr;

struct MetaInfo {
    int kind = 0;
    std::string structType;
    std::string structPath;
    std::string structGuid;
    std::string innerType;
    std::string keyType;
    std::string valueType;
    MetaInfoPtr innerMeta;
    MetaInfoPtr keyMeta;
    MetaInfoPtr valueMeta;
};

struct StructValue {
    std::vector<Property> properties;
};

struct ArrayValue {
    std::vector<ValuePtr> items;
};

struct MapEntry {
    ValuePtr key;
    ValuePtr value;
};

struct MapValue {
    int numKeysToRemove = 0;
    std::vector<MapEntry> items;
};

struct RawValue {
    ByteVec bytes;
};

struct Value {
    typedef std::variant<std::monostate, std::int32_t, std::string, StructValue, ArrayValue, MapValue, RawValue> Variant;
    Variant data;
};

struct Property {
    std::string name;
    std::string type;
    MetaInfoPtr meta;
    ByteVec metaRaw;
    ValuePtr value;
    ByteVec valueTrailing;
    std::optional<ByteVec> rawOverride;
};

static MetaInfoPtr ParseMeta(Reader& reader, const std::string& type) {
    MetaInfoPtr meta(new MetaInfo());
    meta->kind = reader.I32();

    if (type == "StructProperty") {
        meta->structType = reader.FString();
        reader.I32();
        meta->structPath = reader.FString();
        reader.I32();
        meta->structGuid = reader.FString();
        reader.I32();
        return meta;
    }
    if (type == "ArrayProperty") {
        meta->innerType = reader.FString();
        meta->innerMeta = ParseMeta(reader, meta->innerType);
        return meta;
    }
    if (type == "MapProperty") {
        meta->keyType = reader.FString();
        meta->keyMeta = ParseMeta(reader, meta->keyType);
        meta->valueType = reader.FString();
        meta->valueMeta = ParseMeta(reader, meta->valueType);
        return meta;
    }
    return meta;
}

static ValuePtr ReadValue(Reader& reader, const std::string& type, const MetaInfoPtr& meta, std::int32_t size);
static void WriteValue(Writer& writer, const std::string& type, const MetaInfoPtr& meta, const ValuePtr& value);

static ValuePtr ReadInlineValue(Reader& reader, const std::string& type, const MetaInfoPtr& meta) {
    if (type == "IntProperty") {
        ValuePtr value(new Value());
        value->data = reader.I32();
        return value;
    }
    if (type == "NameProperty" || type == "ObjectProperty") {
        ValuePtr value(new Value());
        value->data = reader.FString();
        return value;
    }
    if (type == "StructProperty") {
        return ReadValue(reader, type, meta, 0);
    }
    throw std::runtime_error("Unsupported inline property type");
}

static std::vector<Property> ReadProperties(Reader& reader);

static ValuePtr ReadValue(Reader& reader, const std::string& type, const MetaInfoPtr& meta, std::int32_t size) {
    ValuePtr value(new Value());
    if (type == "IntProperty") {
        value->data = reader.I32();
        return value;
    }
    if (type == "NameProperty" || type == "ObjectProperty") {
        value->data = reader.FString();
        return value;
    }
    if (type == "StructProperty") {
        StructValue structValue;
        structValue.properties = ReadProperties(reader);
        value->data = structValue;
        return value;
    }
    if (type == "ArrayProperty") {
        ArrayValue arrayValue;
        int count = reader.I32();
        for (int i = 0; i < count; ++i) {
            arrayValue.items.push_back(ReadInlineValue(reader, meta->innerType, meta->innerMeta));
        }
        value->data = arrayValue;
        return value;
    }
    if (type == "MapProperty") {
        MapValue mapValue;
        mapValue.numKeysToRemove = reader.I32();
        int count = reader.I32();
        for (int i = 0; i < count; ++i) {
            MapEntry entry;
            entry.key = ReadInlineValue(reader, meta->keyType, meta->keyMeta);
            entry.value = ReadInlineValue(reader, meta->valueType, meta->valueMeta);
            mapValue.items.push_back(entry);
        }
        value->data = mapValue;
        return value;
    }

    RawValue raw;
    raw.bytes = reader.ReadBytes(static_cast<size_t>(size));
    value->data = raw;
    return value;
}

static Property ReadProperty(Reader& reader, const std::string& name) {
    Property property;
    property.name = name;
    property.type = reader.FString();
    size_t metaStart = reader.pos;
    property.meta = ParseMeta(reader, property.type);
    property.metaRaw.assign(reader.buffer->begin() + static_cast<long long>(metaStart),
                            reader.buffer->begin() + static_cast<long long>(reader.pos));
    std::int32_t size = reader.I32();
    unsigned char terminator = reader.U8();
    if (terminator != 0) throw std::runtime_error("Invalid property terminator");
    size_t valueStart = reader.pos;
    property.value = ReadValue(reader, property.type, property.meta, size);
    size_t consumed = reader.pos - valueStart;
    if (consumed < static_cast<size_t>(size)) {
        property.valueTrailing = reader.ReadBytes(static_cast<size_t>(size) - consumed);
    }
    return property;
}

static std::vector<Property> ReadProperties(Reader& reader) {
    std::vector<Property> props;
    while (true) {
        std::string name = reader.FString();
        if (name == "None") break;
        props.push_back(ReadProperty(reader, name));
    }
    return props;
}

static void WriteInlineValue(Writer& writer, const std::string& type, const MetaInfoPtr& meta, const ValuePtr& value) {
    (void)meta;
    if (type == "IntProperty") {
        writer.WriteI32(std::get<std::int32_t>(value->data));
        return;
    }
    if (type == "NameProperty" || type == "ObjectProperty") {
        writer.WriteFString(std::get<std::string>(value->data));
        return;
    }
    if (type == "StructProperty") {
        WriteValue(writer, type, meta, value);
        return;
    }
    throw std::runtime_error("Unsupported inline property write");
}

static ByteVec WriteValueBytes(const std::string& type, const MetaInfoPtr& meta, const ValuePtr& value, const ByteVec& trailing) {
    Writer writer;
    WriteValue(writer, type, meta, value);
    writer.WriteBytes(trailing);
    return writer.bytes;
}

static void WriteProperty(Writer& writer, const Property& property);

static void WriteProperties(Writer& writer, const std::vector<Property>& properties) {
    for (size_t i = 0; i < properties.size(); ++i) {
        writer.WriteFString(properties[i].name);
        WriteProperty(writer, properties[i]);
    }
    writer.WriteFString("None");
}

static void WriteValue(Writer& writer, const std::string& type, const MetaInfoPtr& meta, const ValuePtr& value) {
    if (type == "IntProperty") {
        writer.WriteI32(std::get<std::int32_t>(value->data));
        return;
    }
    if (type == "NameProperty" || type == "ObjectProperty") {
        writer.WriteFString(std::get<std::string>(value->data));
        return;
    }
    if (type == "StructProperty") {
        const StructValue& structValue = std::get<StructValue>(value->data);
        WriteProperties(writer, structValue.properties);
        return;
    }
    if (type == "ArrayProperty") {
        const ArrayValue& arrayValue = std::get<ArrayValue>(value->data);
        writer.WriteI32(static_cast<std::int32_t>(arrayValue.items.size()));
        for (size_t i = 0; i < arrayValue.items.size(); ++i) {
            WriteInlineValue(writer, meta->innerType, meta->innerMeta, arrayValue.items[i]);
        }
        return;
    }
    if (type == "MapProperty") {
        const MapValue& mapValue = std::get<MapValue>(value->data);
        writer.WriteI32(mapValue.numKeysToRemove);
        writer.WriteI32(static_cast<std::int32_t>(mapValue.items.size()));
        for (size_t i = 0; i < mapValue.items.size(); ++i) {
            WriteInlineValue(writer, meta->keyType, meta->keyMeta, mapValue.items[i].key);
            WriteInlineValue(writer, meta->valueType, meta->valueMeta, mapValue.items[i].value);
        }
        return;
    }
    if (std::holds_alternative<RawValue>(value->data)) {
        writer.WriteBytes(std::get<RawValue>(value->data).bytes);
        return;
    }
    throw std::runtime_error("Unsupported property write");
}

static void WriteProperty(Writer& writer, const Property& property) {
    writer.WriteFString(property.type);
    ByteVec payload = property.rawOverride.has_value()
        ? *property.rawOverride
        : WriteValueBytes(property.type, property.meta, property.value, property.valueTrailing);
    writer.WriteBytes(property.metaRaw);
    writer.WriteI32(static_cast<std::int32_t>(payload.size()));
    writer.WriteU8(0);
    writer.WriteBytes(payload);
}

struct SaveFile {
    ByteVec headerRaw;
    ByteVec preProperties;
    std::vector<Property> properties;
    ByteVec trailing;
};

static SaveFile ParseGvas(const ByteVec& bytes) {
    Reader reader(bytes);
    ByteVec magic = reader.ReadBytes(4);
    if (!(magic.size() == 4 && magic[0] == 'G' && magic[1] == 'V' && magic[2] == 'A' && magic[3] == 'S')) {
        throw std::runtime_error("Bad GVAS magic");
    }
    reader.I32();
    reader.I32();
    int saveGameFileVersion = 0;
    {
        Reader headerReader(bytes);
        headerReader.ReadBytes(4);
        saveGameFileVersion = headerReader.I32();
    }
    if (saveGameFileVersion >= 3) reader.I32();
    reader.I16();
    reader.I16();
    reader.I16();
    reader.I32();
    reader.FString();
    reader.I32();
    int customVersionCount = reader.I32();
    for (int i = 0; i < customVersionCount; ++i) {
        reader.ReadBytes(16);
        reader.I32();
    }
    reader.FString();

    SaveFile file;
    file.headerRaw.assign(bytes.begin(), bytes.begin() + static_cast<long long>(reader.pos));
    if (!reader.PlausibleFStringAt(reader.pos) &&
        reader.pos < bytes.size() &&
        bytes[reader.pos] == 0 &&
        reader.PlausibleFStringAt(reader.pos + 1)) {
        file.preProperties.push_back(bytes[reader.pos]);
        reader.pos += 1;
    }

    file.properties = ReadProperties(reader);
    file.trailing.assign(bytes.begin() + static_cast<long long>(reader.pos), bytes.end());
    return file;
}

static ByteVec SerializeGvas(const SaveFile& file) {
    Writer writer;
    writer.WriteBytes(file.headerRaw);
    writer.WriteBytes(file.preProperties);
    WriteProperties(writer, file.properties);
    writer.WriteBytes(file.trailing);
    return writer.bytes;
}

static ByteVec ReadFileBytes(const std::wstring& path) {
    std::ifstream stream(path.c_str(), std::ios::binary);
    if (!stream) throw std::runtime_error("Unable to read file");
    return ByteVec((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

static void WriteFileBytes(const std::wstring& path, const ByteVec& data) {
    std::ofstream stream(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Unable to write file");
    stream.write(reinterpret_cast<const char*>(&data[0]), static_cast<std::streamsize>(data.size()));
}

// AES-256 implementation based on the standard Rijndael round structure.
class Aes256 {
public:
    Aes256() = default;

    void SetKey(const ByteVec& key) {
        if (key.size() != 32) throw std::runtime_error("AES-256 key must be 32 bytes");
        ExpandKey(key);
    }

    ByteVec EncryptCbc(const ByteVec& plain, const ByteVec& iv) const {
        if (iv.size() != 16 || (plain.size() % 16) != 0) throw std::runtime_error("Invalid AES CBC input");
        ByteVec out(plain.size());
        ByteVec prev = iv;
        for (size_t block = 0; block < plain.size(); block += 16) {
            unsigned char state[16];
            for (int i = 0; i < 16; ++i) state[i] = plain[block + i] ^ prev[i];
            EncryptBlock(state);
            for (int i = 0; i < 16; ++i) {
                out[block + i] = state[i];
                prev[i] = state[i];
            }
        }
        return out;
    }

    ByteVec DecryptCbc(const ByteVec& cipher, const ByteVec& iv) const {
        if (iv.size() != 16 || (cipher.size() % 16) != 0) throw std::runtime_error("Invalid AES CBC input");
        ByteVec out(cipher.size());
        ByteVec prev = iv;
        for (size_t block = 0; block < cipher.size(); block += 16) {
            unsigned char state[16];
            memcpy(state, &cipher[block], 16);
            unsigned char copy[16];
            memcpy(copy, state, 16);
            DecryptBlock(state);
            for (int i = 0; i < 16; ++i) {
                out[block + i] = state[i] ^ prev[i];
                prev[i] = copy[i];
            }
        }
        return out;
    }

private:
    unsigned char roundKey[240] = {};

    static unsigned char XTime(unsigned char x) {
        return static_cast<unsigned char>((x << 1) ^ ((x >> 7) * 0x1B));
    }

    static unsigned char Mul(unsigned char a, unsigned char b) {
        unsigned char result = 0;
        while (b) {
            if (b & 1) result ^= a;
            a = XTime(a);
            b >>= 1;
        }
        return result;
    }

    static unsigned char Rotl8(unsigned char x, unsigned char shift) {
        return static_cast<unsigned char>((x << shift) | (x >> (8 - shift)));
    }

    static unsigned char SBox(unsigned char x) {
        unsigned char inv = 0;
        if (x != 0) {
            for (int i = 1; i < 256; ++i) {
                if (Mul(x, static_cast<unsigned char>(i)) == 1) {
                    inv = static_cast<unsigned char>(i);
                    break;
                }
            }
        }
        return static_cast<unsigned char>(inv ^ Rotl8(inv, 1) ^ Rotl8(inv, 2) ^ Rotl8(inv, 3) ^ Rotl8(inv, 4) ^ 0x63);
    }

    static unsigned char InvSBox(unsigned char x) {
        for (int i = 0; i < 256; ++i) {
            if (SBox(static_cast<unsigned char>(i)) == x) return static_cast<unsigned char>(i);
        }
        return 0;
    }

    void ExpandKey(const ByteVec& key) {
        static const unsigned char rcon[15] = {
            0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1B,0x36,0x6C,0xD8,0xAB,0x4D,0x9A
        };
        memcpy(roundKey, &key[0], 32);
        int bytesGenerated = 32;
        int rconIteration = 1;
        unsigned char temp[4];
        while (bytesGenerated < 240) {
            for (int i = 0; i < 4; ++i) temp[i] = roundKey[bytesGenerated - 4 + i];
            if (bytesGenerated % 32 == 0) {
                unsigned char t = temp[0];
                temp[0] = SBox(temp[1]) ^ rcon[rconIteration - 1];
                temp[1] = SBox(temp[2]);
                temp[2] = SBox(temp[3]);
                temp[3] = SBox(t);
                ++rconIteration;
            } else if (bytesGenerated % 32 == 16) {
                for (int i = 0; i < 4; ++i) temp[i] = SBox(temp[i]);
            }
            for (int i = 0; i < 4; ++i) {
                roundKey[bytesGenerated] = static_cast<unsigned char>(roundKey[bytesGenerated - 32] ^ temp[i]);
                ++bytesGenerated;
            }
        }
    }

    void AddRoundKey(unsigned char* state, int round) const {
        for (int i = 0; i < 16; ++i) state[i] ^= roundKey[round * 16 + i];
    }

    static void SubBytes(unsigned char* state) {
        for (int i = 0; i < 16; ++i) state[i] = SBox(state[i]);
    }

    static void InvSubBytes(unsigned char* state) {
        for (int i = 0; i < 16; ++i) state[i] = InvSBox(state[i]);
    }

    static void ShiftRows(unsigned char* state) {
        unsigned char temp[16];
        memcpy(temp, state, 16);
        state[0] = temp[0];
        state[1] = temp[5];
        state[2] = temp[10];
        state[3] = temp[15];
        state[4] = temp[4];
        state[5] = temp[9];
        state[6] = temp[14];
        state[7] = temp[3];
        state[8] = temp[8];
        state[9] = temp[13];
        state[10] = temp[2];
        state[11] = temp[7];
        state[12] = temp[12];
        state[13] = temp[1];
        state[14] = temp[6];
        state[15] = temp[11];
    }

    static void InvShiftRows(unsigned char* state) {
        unsigned char temp[16];
        memcpy(temp, state, 16);
        state[0] = temp[0];
        state[1] = temp[13];
        state[2] = temp[10];
        state[3] = temp[7];
        state[4] = temp[4];
        state[5] = temp[1];
        state[6] = temp[14];
        state[7] = temp[11];
        state[8] = temp[8];
        state[9] = temp[5];
        state[10] = temp[2];
        state[11] = temp[15];
        state[12] = temp[12];
        state[13] = temp[9];
        state[14] = temp[6];
        state[15] = temp[3];
    }

    static void MixColumns(unsigned char* state) {
        for (int c = 0; c < 4; ++c) {
            unsigned char* col = state + (c * 4);
            unsigned char a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
            col[0] = static_cast<unsigned char>(Mul(a0, 2) ^ Mul(a1, 3) ^ a2 ^ a3);
            col[1] = static_cast<unsigned char>(a0 ^ Mul(a1, 2) ^ Mul(a2, 3) ^ a3);
            col[2] = static_cast<unsigned char>(a0 ^ a1 ^ Mul(a2, 2) ^ Mul(a3, 3));
            col[3] = static_cast<unsigned char>(Mul(a0, 3) ^ a1 ^ a2 ^ Mul(a3, 2));
        }
    }

    static void InvMixColumns(unsigned char* state) {
        for (int c = 0; c < 4; ++c) {
            unsigned char* col = state + (c * 4);
            unsigned char a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
            col[0] = static_cast<unsigned char>(Mul(a0, 14) ^ Mul(a1, 11) ^ Mul(a2, 13) ^ Mul(a3, 9));
            col[1] = static_cast<unsigned char>(Mul(a0, 9) ^ Mul(a1, 14) ^ Mul(a2, 11) ^ Mul(a3, 13));
            col[2] = static_cast<unsigned char>(Mul(a0, 13) ^ Mul(a1, 9) ^ Mul(a2, 14) ^ Mul(a3, 11));
            col[3] = static_cast<unsigned char>(Mul(a0, 11) ^ Mul(a1, 13) ^ Mul(a2, 9) ^ Mul(a3, 14));
        }
    }

    void EncryptBlock(unsigned char* state) const {
        AddRoundKey(state, 0);
        for (int round = 1; round < 14; ++round) {
            SubBytes(state);
            ShiftRows(state);
            MixColumns(state);
            AddRoundKey(state, round);
        }
        SubBytes(state);
        ShiftRows(state);
        AddRoundKey(state, 14);
    }

    void DecryptBlock(unsigned char* state) const {
        AddRoundKey(state, 14);
        for (int round = 13; round > 0; --round) {
            InvShiftRows(state);
            InvSubBytes(state);
            AddRoundKey(state, round);
            InvMixColumns(state);
        }
        InvShiftRows(state);
        InvSubBytes(state);
        AddRoundKey(state, 0);
    }
};

static ByteVec Sha256(const ByteVec& input) {
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    DWORD objectSize = 0;
    DWORD resultSize = 0;
    ByteVec objectBuffer;
    ByteVec digest(32);

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) {
        throw std::runtime_error("BCryptOpenAlgorithmProvider failed");
    }
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &resultSize, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("BCryptGetProperty failed");
    }
    objectBuffer.resize(objectSize);
    if (BCryptCreateHash(algorithm, &hash, objectBuffer.data(), objectSize, NULL, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("BCryptCreateHash failed");
    }
    if (BCryptHashData(hash, const_cast<PUCHAR>(input.data()), static_cast<ULONG>(input.size()), 0) != 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("BCryptHashData failed");
    }
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) != 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("BCryptFinishHash failed");
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return digest;
}

static ByteVec RandomBytes(size_t count) {
    ByteVec out(count);
    if (BCryptGenRandom(NULL, out.data(), static_cast<ULONG>(out.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        throw std::runtime_error("BCryptGenRandom failed");
    }
    return out;
}

static ByteVec DeriveKey(const std::wstring& seed) {
    std::string utf8 = WideToUtf8(seed);
    return Sha256(ByteVec(utf8.begin(), utf8.end()));
}

static ByteVec DecryptSaveBytes(const ByteVec& data, const ByteVec& key) {
    if (data.size() < 32 || ((data.size() - 16) % 16) != 0) {
        throw std::runtime_error("Bad encrypted save size");
    }
    ByteVec iv(data.begin(), data.begin() + 16);
    ByteVec cipher(data.begin() + 16, data.end());
    Aes256 aes;
    aes.SetKey(key);
    ByteVec plain = aes.DecryptCbc(cipher, iv);
    if (plain.size() < 4 || !(plain[0] == 'G' && plain[1] == 'V' && plain[2] == 'A' && plain[3] == 'S')) {
        throw std::runtime_error("Decrypt produced invalid GVAS data");
    }
    return plain;
}

static ByteVec EncryptSaveBytes(ByteVec plain, const ByteVec& key) {
    if (plain.size() < 4 || !(plain[0] == 'G' && plain[1] == 'V' && plain[2] == 'A' && plain[3] == 'S')) {
        throw std::runtime_error("Plaintext must start with GVAS");
    }
    if ((plain.size() % 16) != 0) {
        plain.resize(plain.size() + (16 - (plain.size() % 16)), 0);
    }
    ByteVec iv = RandomBytes(16);
    Aes256 aes;
    aes.SetKey(key);
    ByteVec cipher = aes.EncryptCbc(plain, iv);
    ByteVec out = iv;
    out.insert(out.end(), cipher.begin(), cipher.end());
    return out;
}

static std::wstring SeedForSavePath(const std::wstring& path) {
    wchar_t filename[MAX_PATH];
    lstrcpynW(filename, PathFindFileNameW(path.c_str()), MAX_PATH);
    std::wstring stem = filename;
    size_t dot = stem.find_last_of(L'.');
    if (dot != std::wstring::npos) stem = stem.substr(0, dot);
    std::wstring digits;
    for (size_t i = 0; i < stem.size(); ++i) {
        if (iswdigit(stem[i])) digits.push_back(stem[i]);
        else break;
    }
    if (digits.empty()) {
        throw std::runtime_error("Save filename must start with a SteamID");
    }
    return digits + kPartySuffix;
}

static Property* FindPropertyByPrefix(std::vector<Property>& properties, const std::string& prefix) {
    for (size_t i = 0; i < properties.size(); ++i) {
        if (StartsWith(properties[i].name, prefix)) return &properties[i];
    }
    return NULL;
}

static const Property* FindPropertyByPrefix(const std::vector<Property>& properties, const std::string& prefix) {
    for (size_t i = 0; i < properties.size(); ++i) {
        if (StartsWith(properties[i].name, prefix)) return &properties[i];
    }
    return NULL;
}

static StructValue* GetStruct(Property& property) {
    return std::get_if<StructValue>(&property.value->data);
}

static const StructValue* GetStruct(const Property& property) {
    return std::get_if<StructValue>(&property.value->data);
}

static ArrayValue* GetArray(Property& property) {
    return std::get_if<ArrayValue>(&property.value->data);
}

static const ArrayValue* GetArray(const Property& property) {
    return std::get_if<ArrayValue>(&property.value->data);
}

static MapValue* GetMap(Property& property) {
    return std::get_if<MapValue>(&property.value->data);
}

static const MapValue* GetMap(const Property& property) {
    return std::get_if<MapValue>(&property.value->data);
}

static std::vector<Property>* PlayerProgressProperties(SaveFile& save) {
    for (size_t i = 0; i < save.properties.size(); ++i) {
        if (save.properties[i].name == "playerProgress") {
            StructValue* value = GetStruct(save.properties[i]);
            return value ? &value->properties : NULL;
        }
    }
    return NULL;
}

static Property* FindPlayerProp(SaveFile& save, const std::string& prefix) {
    std::vector<Property>* props = PlayerProgressProperties(save);
    return props ? FindPropertyByPrefix(*props, prefix) : NULL;
}

static Property* FindInventoryAmountProp(SaveFile& save, const std::string& itemName) {
    Property* inventoryProp = FindPlayerProp(save, "runtimeInventory");
    if (!inventoryProp) return NULL;
    ArrayValue* array = GetArray(*inventoryProp);
    if (!array) return NULL;

    for (size_t i = 0; i < array->items.size(); ++i) {
        StructValue* row = std::get_if<StructValue>(&array->items[i]->data);
        if (!row) continue;
        Property* nameProp = FindPropertyByPrefix(row->properties, "name");
        Property* amountProp = FindPropertyByPrefix(row->properties, "amount");
        if (!nameProp || !amountProp) continue;
        std::string* value = std::get_if<std::string>(&nameProp->value->data);
        if (value && *value == itemName) return amountProp;
    }
    return NULL;
}

static MapEntry* FindChallengeEntry(SaveFile& save, const std::string& key) {
    Property* challengesProp = FindPlayerProp(save, "challenges");
    if (!challengesProp) return NULL;
    MapValue* map = GetMap(*challengesProp);
    if (!map) return NULL;
    for (size_t i = 0; i < map->items.size(); ++i) {
        std::string* entryKey = std::get_if<std::string>(&map->items[i].key->data);
        if (entryKey && *entryKey == key) return &map->items[i];
    }
    return NULL;
}

static int LevelFromAmount(int amount) {
    int clamped = amount < 0 ? 0 : amount;
    int level = (clamped + 999) / 1000;
    return level < 1 ? 1 : level;
}

static int AmountForLevel(int level, int currentAmount) {
    int progress = std::max(currentAmount, 0) % 1000;
    if (progress == 0 && currentAmount > 0) progress = 1000;
    return std::max(level - 1, 0) * 1000 + progress;
}

static bool IsCharacterItem(const std::string& itemName) {
    return itemName == "itemHero" || itemName == "itemSheriffStar";
}

static bool IsSpellItem(const std::string& itemName) {
    return NameMatchesList(itemName, kSpellItems, sizeof(kSpellItems) / sizeof(kSpellItems[0]));
}

static bool IsUtilityItem(const std::string& itemName) {
    return NameMatchesList(itemName, kUtilityItems, sizeof(kUtilityItems) / sizeof(kUtilityItems[0]));
}

static bool IsWeaponItem(const std::string& itemName) {
    return !IsSpellItem(itemName) && !IsUtilityItem(itemName) && !IsCharacterItem(itemName);
}

static int SetChallengeValue(SaveFile& save, const std::string& key, int value) {
    MapEntry* entry = FindChallengeEntry(save, key);
    if (!entry) return 0;
    entry->value->data = value;
    if (StartsWith(key, "item") && EndsWith(key, "Lvl")) {
        std::string itemName = key.substr(0, key.size() - 3);
        Property* amountProp = FindInventoryAmountProp(save, itemName);
        if (amountProp) {
            int* amount = std::get_if<std::int32_t>(&amountProp->value->data);
            int currentAmount = amount ? *amount : 0;
            int nextAmount = std::min(AmountForLevel(value, currentAmount), kInt32Max);
            amountProp->value->data = static_cast<std::int32_t>(nextAmount);
        }
    }
    return 1;
}

static Property CloneProperty(const Property& source);

static ValuePtr CloneValue(const ValuePtr& value) {
    ValuePtr copy(new Value());
    if (std::holds_alternative<std::int32_t>(value->data)) {
        copy->data = std::get<std::int32_t>(value->data);
    } else if (std::holds_alternative<std::string>(value->data)) {
        copy->data = std::get<std::string>(value->data);
    } else if (std::holds_alternative<StructValue>(value->data)) {
        StructValue out;
        const StructValue& source = std::get<StructValue>(value->data);
        for (size_t i = 0; i < source.properties.size(); ++i) {
            out.properties.push_back(CloneProperty(source.properties[i]));
        }
        copy->data = out;
    } else if (std::holds_alternative<ArrayValue>(value->data)) {
        ArrayValue out;
        const ArrayValue& source = std::get<ArrayValue>(value->data);
        for (size_t i = 0; i < source.items.size(); ++i) {
            out.items.push_back(CloneValue(source.items[i]));
        }
        copy->data = out;
    } else if (std::holds_alternative<MapValue>(value->data)) {
        MapValue out;
        const MapValue& source = std::get<MapValue>(value->data);
        out.numKeysToRemove = source.numKeysToRemove;
        for (size_t i = 0; i < source.items.size(); ++i) {
            MapEntry entry;
            entry.key = CloneValue(source.items[i].key);
            entry.value = CloneValue(source.items[i].value);
            out.items.push_back(entry);
        }
        copy->data = out;
    } else if (std::holds_alternative<RawValue>(value->data)) {
        copy->data = std::get<RawValue>(value->data);
    }
    return copy;
}

static Property CloneProperty(const Property& source) {
    Property copy = source;
    copy.value = CloneValue(source.value);
    return copy;
}

static bool AddInventoryItem(SaveFile& save, const std::string& itemName, int amount) {
    if (FindInventoryAmountProp(save, itemName)) return false;
    Property* inventoryProp = FindPlayerProp(save, "runtimeInventory");
    if (!inventoryProp) return false;
    ArrayValue* array = GetArray(*inventoryProp);
    if (!array || array->items.empty()) return false;

    StructValue* templateValue = NULL;
    for (size_t i = 0; i < array->items.size(); ++i) {
        StructValue* row = std::get_if<StructValue>(&array->items[i]->data);
        if (!row) continue;
        if (FindPropertyByPrefix(row->properties, "name") && FindPropertyByPrefix(row->properties, "amount")) {
            templateValue = row;
            break;
        }
    }
    if (!templateValue) return false;

    ValuePtr rowValue(new Value());
    StructValue newRow;
    for (size_t i = 0; i < templateValue->properties.size(); ++i) {
        Property prop = CloneProperty(templateValue->properties[i]);
        std::string shortName = ShortName(prop.name);
        if (shortName == "name") {
            prop.value->data = itemName;
        } else if (shortName == "amount") {
            prop.value->data = static_cast<std::int32_t>(amount);
        }
        newRow.properties.push_back(prop);
    }
    rowValue->data = newRow;
    array->items.push_back(rowValue);
    return true;
}

static void EnsureChallengeItem(SaveFile& save, const std::string& key, int value) {
    MapEntry* entry = FindChallengeEntry(save, key);
    if (entry) {
        entry->value->data = static_cast<std::int32_t>(value);
        return;
    }
    Property* challengesProp = FindPlayerProp(save, "challenges");
    if (!challengesProp) return;
    MapValue* map = GetMap(*challengesProp);
    if (!map) return;

    MapEntry newEntry;
    newEntry.key.reset(new Value());
    newEntry.key->data = key;
    newEntry.value.reset(new Value());
    newEntry.value->data = static_cast<std::int32_t>(value);
    map->items.push_back(newEntry);
}

static bool SetOrAddInventoryAmount(SaveFile& save, const std::string& itemName, int amount) {
    Property* amountProp = FindInventoryAmountProp(save, itemName);
    if (amountProp) {
        amountProp->value->data = static_cast<std::int32_t>(amount);
        return true;
    }
    return AddInventoryItem(save, itemName, amount);
}

static int EnsureWeaponPrestigeInventory(SaveFile& save, int target) {
    int changed = 0;
    for (size_t i = 0; i < sizeof(kBuildableWeapons) / sizeof(kBuildableWeapons[0]); ++i) {
        std::string base = WideToUtf8(kBuildableWeapons[i]);
        std::string prestigeName = base + "Prestige";
        Property* amountProp = FindInventoryAmountProp(save, prestigeName);
        if (!amountProp) {
            if (AddInventoryItem(save, prestigeName, target)) ++changed;
            amountProp = FindInventoryAmountProp(save, prestigeName);
        }
        if (amountProp) {
            amountProp->value->data = static_cast<std::int32_t>(target);
            ++changed;
        }
    }
    return changed;
}

static std::vector<std::string> MissingBuildableWeaponItems(SaveFile& save) {
    std::vector<std::string> missing;
    for (size_t i = 0; i < sizeof(kBuildableWeapons) / sizeof(kBuildableWeapons[0]); ++i) {
        std::string itemName = WideToUtf8(kBuildableWeapons[i]);
        if (!FindInventoryAmountProp(save, itemName)) {
            missing.push_back(itemName);
        }
    }
    return missing;
}

static int AddMissingBuildableWeapons(SaveFile& save) {
    int added = 0;
    std::vector<std::string> missing = MissingBuildableWeaponItems(save);
    for (size_t i = 0; i < missing.size(); ++i) {
        if (AddInventoryItem(save, missing[i], 1)) {
            EnsureChallengeItem(save, missing[i] + "Lvl", 1);
            ++added;
        }
    }
    return added;
}

static int SetAllWeaponLevels(SaveFile& save, int level) {
    int changed = 0;
    Property* challengesProp = FindPlayerProp(save, "challenges");
    if (!challengesProp) return 0;
    MapValue* map = GetMap(*challengesProp);
    if (!map) return 0;
    for (size_t i = 0; i < map->items.size(); ++i) {
        std::string* key = std::get_if<std::string>(&map->items[i].key->data);
        if (!key || !StartsWith(*key, "item") || !EndsWith(*key, "Lvl")) continue;
        std::string itemName = key->substr(0, key->size() - 3);
        if (IsWeaponItem(itemName)) {
            changed += SetChallengeValue(save, *key, level);
        }
    }
    return changed;
}

static int SetAllSpellLevels(SaveFile& save, int level) {
    int changed = 0;
    Property* challengesProp = FindPlayerProp(save, "challenges");
    if (!challengesProp) return 0;
    MapValue* map = GetMap(*challengesProp);
    if (!map) return 0;
    for (size_t i = 0; i < map->items.size(); ++i) {
        std::string* key = std::get_if<std::string>(&map->items[i].key->data);
        if (!key || !StartsWith(*key, "item") || !EndsWith(*key, "Lvl")) continue;
        std::string itemName = key->substr(0, key->size() - 3);
        if (IsSpellItem(itemName)) {
            changed += SetChallengeValue(save, *key, level);
        }
    }
    return changed;
}

static void UnlockAll(SaveFile& save) {
    AddMissingBuildableWeapons(save);
    SetAllWeaponLevels(save, 100);
    EnsureWeaponPrestigeInventory(save, 10);
    EnsureChallengeItem(save, "Prestige", 10);
    EnsureChallengeItem(save, "itemHeroLvl", 100);
    SetOrAddInventoryAmount(save, "moneySoul", 300000);
    SetOrAddInventoryAmount(save, "moneyGold", 9999999);
}

static std::wstring DefaultSaveDir() {
    wchar_t localAppData[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, localAppData);
    std::wstring dir = localAppData;
    dir += L"\\FarFarWest\\Saved\\SaveGames";
    return dir;
}

static std::optional<std::wstring> FindLatestSave() {
    std::wstring pattern = DefaultSaveDir() + L"\\*.save";
    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) return std::nullopt;

    FILETIME latest = {};
    std::optional<std::wstring> path;
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (lstrlenW(data.cFileName) != 22) continue;
        bool valid = true;
        for (int i = 0; i < 17; ++i) {
            if (!iswdigit(data.cFileName[i])) valid = false;
        }
        if (!valid || lstrcmpiW(data.cFileName + 17, L".save") != 0) continue;
        if (!path.has_value() || CompareFileTime(&data.ftLastWriteTime, &latest) > 0) {
            latest = data.ftLastWriteTime;
            path = DefaultSaveDir() + L"\\" + data.cFileName;
        }
    } while (FindNextFileW(handle, &data));
    FindClose(handle);
    return path;
}

struct AppState {
    std::optional<std::wstring> savePath;
    ByteVec key;
    SaveFile save;
    bool loaded = false;
    std::wstring status = L"Open a save file or auto-import from FarFarWest.";
};

enum ControlId {
    IDC_BTN_OPEN = 1001,
    IDC_BTN_AUTO_IMPORT,
    IDC_BTN_OPEN_FOLDER,
    IDC_BTN_SAVE,
    IDC_BTN_SAVE_AS,
    IDC_BTN_WEAPON_100,
    IDC_BTN_SPELL_100,
    IDC_BTN_PRESTIGE_10,
    IDC_BTN_ADD_BUILDABLE,
    IDC_BTN_UNLOCK_ALL,
    IDC_TAB_EDITOR,
    IDC_EDIT_SEARCH,
    IDC_LIST_ROWS,
    IDC_EDIT_DETAIL,
    IDC_EDIT_VALUE,
    IDC_BTN_APPLY,
    IDC_STATIC_SELECTED,
    IDC_STATIC_TYPE,
    IDC_STATIC_STATUS
};

enum TabKey {
    TAB_OVERVIEW = 0,
    TAB_INVENTORY,
    TAB_LEVELS,
    TAB_UPGRADES,
    TAB_JOKERS,
    TAB_REWARDS,
    TAB_OTHER
};

struct UiRow {
    std::string id;
    std::wstring line;
    std::wstring selectedText;
    std::wstring typeName;
    std::wstring valueText;
    bool editable = true;
};

static std::wstring ToLowerWide(std::wstring text) {
    for (wchar_t& ch : text) ch = static_cast<wchar_t>(towlower(ch));
    return text;
}

static bool ContainsCaseInsensitive(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    return ToLowerWide(haystack).find(ToLowerWide(needle)) != std::wstring::npos;
}

static bool IsStringValue(const ValuePtr& value) {
    return std::holds_alternative<std::string>(value->data);
}

static bool IsIntValue(const ValuePtr& value) {
    return std::holds_alternative<std::int32_t>(value->data);
}

static bool IsScalarValue(const ValuePtr& value) {
    return IsStringValue(value) || IsIntValue(value);
}

static std::wstring ValueToWideString(const ValuePtr& value) {
    if (IsStringValue(value)) return Utf8ToWide(std::get<std::string>(value->data));
    if (IsIntValue(value)) return std::to_wstring(std::get<std::int32_t>(value->data));
    return L"";
}

static std::wstring PropertyTypeToWide(const Property& property) {
    if (property.type == "IntProperty") return L"int";
    if (property.type == "NameProperty") return L"name";
    if (property.type == "ObjectProperty") return L"object";
    if (property.type == "ArrayProperty") return L"array";
    if (property.type == "MapProperty") return L"map";
    if (property.type == "StructProperty") return L"struct";
    return Utf8ToWide(property.type);
}

static Property* FindMapStructPropByKey(SaveFile& save, const std::string& topLevelPrefix, const std::string& itemKey) {
    Property* prop = FindPlayerProp(save, topLevelPrefix);
    if (!prop) return NULL;
    MapValue* map = GetMap(*prop);
    if (!map) return NULL;
    for (MapEntry& entry : map->items) {
        std::string* key = std::get_if<std::string>(&entry.key->data);
        if (!key || *key != itemKey) continue;
        StructValue* structure = std::get_if<StructValue>(&entry.value->data);
        if (!structure) return NULL;
        return structure->properties.empty() ? NULL : &structure->properties[0];
    }
    return NULL;
}

static int RunSmokeTest(const std::wstring& path) {
    try {
        std::wstring seed = SeedForSavePath(path);
        ByteVec encrypted = ReadFileBytes(path);
        ByteVec key = DeriveKey(seed);
        ByteVec plain = DecryptSaveBytes(encrypted, key);
        SaveFile save = ParseGvas(plain);
        ByteVec roundtrip = SerializeGvas(save);
        ByteVec encryptedRoundtrip = EncryptSaveBytes(roundtrip, key);
        if (roundtrip.empty() || encryptedRoundtrip.empty()) {
            return 3;
        }
        return 0;
    } catch (...) {
        return 2;
    }
}

class AppWindow {
public:
    AppWindow() = default;

    bool Create(HINSTANCE instance) {
        hInstance_ = instance;
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
        InitCommonControlsEx(&icc);

        WNDCLASSW wc = {};
        wc.lpfnWndProc = WindowProcSetup;
        wc.hInstance = instance;
        wc.lpszClassName = kWindowClass;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        wc.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClassW(&wc);

        hwnd_ = CreateWindowExW(
            0, kWindowClass, kWindowTitle,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1320, 900,
            NULL, NULL, instance, this);
        return hwnd_ != NULL;
    }

    int Run() {
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return static_cast<int>(msg.wParam);
    }

private:
    HINSTANCE hInstance_ = NULL;
    HWND hwnd_ = NULL;
    HFONT uiFont_ = NULL;
    HFONT titleFont_ = NULL;
    HFONT subtitleFont_ = NULL;
    HBRUSH panelBrush_ = NULL;
    HBRUSH panelAltBrush_ = NULL;

    HWND btnOpen_ = NULL;
    HWND btnAutoImport_ = NULL;
    HWND btnOpenFolder_ = NULL;
    HWND btnSave_ = NULL;
    HWND btnSaveAs_ = NULL;
    HWND btnWeapon100_ = NULL;
    HWND btnSpell100_ = NULL;
    HWND btnPrestige10_ = NULL;
    HWND btnAddBuildable_ = NULL;
    HWND btnUnlockAll_ = NULL;
    HWND tabControl_ = NULL;
    HWND searchEdit_ = NULL;
    HWND rowList_ = NULL;
    HWND detailEdit_ = NULL;
    HWND valueEdit_ = NULL;
    HWND applyButton_ = NULL;
    HWND selectedStatic_ = NULL;
    HWND typeStatic_ = NULL;
    HWND statusLabel_ = NULL;

    AppState state_;
    int currentTab_ = TAB_OVERVIEW;
    std::vector<UiRow> rows_;
    std::vector<int> filteredRows_;
    std::wstring searchText_;

    static LRESULT CALLBACK WindowProcSetup(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            AppWindow* self = reinterpret_cast<AppWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProcThunk));
            self->hwnd_ = hwnd;
            return self->HandleMessage(msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    static LRESULT CALLBACK WindowProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        AppWindow* self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        return self ? self->HandleMessage(msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void ApplyWindowTheme() {
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark));
    }

    void CreateControls() {
        uiFont_ = CreateFontW(
            -18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        titleFont_ = CreateFontW(
            -30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        subtitleFont_ = CreateFontW(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        panelBrush_ = CreateSolidBrush(gColors.panel);
        panelAltBrush_ = CreateSolidBrush(gColors.panelAlt);

        btnOpen_ = CreateButton(L"Open", IDC_BTN_OPEN);
        btnAutoImport_ = CreateButton(L"Auto Import Save", IDC_BTN_AUTO_IMPORT);
        btnOpenFolder_ = CreateButton(L"Open Save Folder", IDC_BTN_OPEN_FOLDER);
        btnSave_ = CreateButton(L"Save", IDC_BTN_SAVE);
        btnSaveAs_ = CreateButton(L"Save As", IDC_BTN_SAVE_AS);
        btnWeapon100_ = CreateButton(L"Weapon Levels 100", IDC_BTN_WEAPON_100);
        btnSpell100_ = CreateButton(L"Spell Levels 100", IDC_BTN_SPELL_100);
        btnPrestige10_ = CreateButton(L"Weapon Prestige 10", IDC_BTN_PRESTIGE_10);
        btnAddBuildable_ = CreateButton(L"Add Missing Buildable", IDC_BTN_ADD_BUILDABLE);
        btnUnlockAll_ = CreateButton(L"Unlock All", IDC_BTN_UNLOCK_ALL);

        tabControl_ = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                      0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_TAB_EDITOR), hInstance_, NULL);
        searchEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                      0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_EDIT_SEARCH), hInstance_, NULL);
        rowList_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                                   0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_LIST_ROWS), hInstance_, NULL);
        detailEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
                                      0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_EDIT_DETAIL), hInstance_, NULL);
        valueEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                     0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_EDIT_VALUE), hInstance_, NULL);
        applyButton_ = CreateButton(L"Apply Value", IDC_BTN_APPLY);
        selectedStatic_ = CreateWindowExW(0, L"STATIC", L"Selected", WS_CHILD | WS_VISIBLE,
                                          0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_STATIC_SELECTED), hInstance_, NULL);
        typeStatic_ = CreateWindowExW(0, L"STATIC", L"Type", WS_CHILD | WS_VISIBLE,
                                      0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_STATIC_TYPE), hInstance_, NULL);
        statusLabel_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                       0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_STATIC_STATUS), hInstance_, NULL);

        const wchar_t* tabs[] = {
            L"Overview", L"Inventory", L"Levels & Stats", L"Item Upgrades",
            L"Jokers", L"Rewards", L"Other Fields"
        };
        for (int i = 0; i < 7; ++i) {
            TCITEMW item = {};
            item.mask = TCIF_TEXT;
            item.pszText = const_cast<wchar_t*>(tabs[i]);
            TabCtrl_InsertItem(tabControl_, i, &item);
        }

        HWND controls[] = {
            btnOpen_, btnAutoImport_, btnOpenFolder_, btnSave_, btnSaveAs_,
            btnWeapon100_, btnSpell100_, btnPrestige10_, btnAddBuildable_, btnUnlockAll_,
            tabControl_, searchEdit_, rowList_, detailEdit_, valueEdit_,
            applyButton_, selectedStatic_, typeStatic_, statusLabel_
        };
        for (HWND control : controls) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
        }

        SetWindowTextW(searchEdit_, L"");
        SetWindowTextW(detailEdit_, L"Pick a field from the current tab to inspect or edit it.");
    }

    HWND CreateButton(const wchar_t* text, int id) {
        return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(id), hInstance_, NULL);
    }

    void LayoutControls(int width, int height) {
        const int margin = 18;
        const int gap = 12;
        const int buttonHeight = 38;
        const int topY = 96;
        const int buttonWidths[] = { 96, 150, 150, 96, 108, 150, 138, 150, 176, 132 };
        HWND buttons[] = { btnOpen_, btnAutoImport_, btnOpenFolder_, btnSave_, btnSaveAs_, btnWeapon100_, btnSpell100_, btnPrestige10_, btnAddBuildable_, btnUnlockAll_ };
        int x = margin;
        for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
            MoveWindow(buttons[i], x, topY, buttonWidths[i], buttonHeight, TRUE);
            x += buttonWidths[i] + gap;
        }

        int contentTop = topY + buttonHeight + 20;
        int contentHeight = height - contentTop - 88;
        MoveWindow(tabControl_, margin, contentTop, width - margin * 2, contentHeight, TRUE);

        RECT tabRect;
        GetClientRect(tabControl_, &tabRect);
        TabCtrl_AdjustRect(tabControl_, FALSE, &tabRect);

        int leftWidth = (tabRect.right - tabRect.left) * 56 / 100;
        int rightX = tabRect.left + leftWidth + gap;
        int rightWidth = (tabRect.right - tabRect.left) - leftWidth - gap;
        int searchHeight = 30;

        MoveWindow(searchEdit_, tabRect.left, tabRect.top, leftWidth, searchHeight, TRUE);
        MoveWindow(rowList_, tabRect.left, tabRect.top + searchHeight + 10, leftWidth,
                   tabRect.bottom - tabRect.top - searchHeight - 10, TRUE);

        MoveWindow(selectedStatic_, rightX, tabRect.top, rightWidth, 24, TRUE);
        MoveWindow(detailEdit_, rightX, tabRect.top + 30, rightWidth, 250, TRUE);
        MoveWindow(typeStatic_, rightX, tabRect.top + 292, rightWidth, 24, TRUE);
        MoveWindow(valueEdit_, rightX, tabRect.top + 322, rightWidth, 30, TRUE);
        MoveWindow(applyButton_, rightX, tabRect.top + 364, rightWidth, 36, TRUE);
        MoveWindow(statusLabel_, margin, height - 52, width - margin * 2, 28, TRUE);
    }

    std::vector<UiRow> BuildRows() {
        std::vector<UiRow> out;
        if (!state_.loaded) return out;

        std::vector<Property>* playerProps = PlayerProgressProperties(state_.save);
        if (!playerProps) return out;

        if (currentTab_ == TAB_OVERVIEW || currentTab_ == TAB_OTHER) {
            static const std::set<std::string> covered = {
                "versionId", "selectedPlayerSkin", "selectedMountSkin", "selectedSpellA", "selectedSpellB",
                "selectedSpellC", "unlockedDifficulties", "itemsUpgrades", "itemJokers", "runtimeInventory",
                "challenges", "rewardedChallenges", "title", "selectedItemA", "selectedItemB"
            };
            for (Property& prop : *playerProps) {
                if (!IsScalarValue(prop.value)) continue;
                std::string shortName = ShortName(prop.name);
                bool inOverview = currentTab_ == TAB_OVERVIEW && covered.find(shortName) != covered.end();
                bool inOther = currentTab_ == TAB_OTHER && covered.find(shortName) == covered.end();
                if (!inOverview && !inOther) continue;

                UiRow row;
                row.id = (currentTab_ == TAB_OVERVIEW ? "overview:" : "other:") + shortName;
                row.selectedText = Utf8ToWide(shortName);
                row.typeName = PropertyTypeToWide(prop);
                row.valueText = ValueToWideString(prop.value);
                row.line = row.selectedText + L" = " + row.valueText;
                out.push_back(row);
            }
            if (currentTab_ == TAB_OTHER && out.empty()) {
                UiRow row;
                row.id = "other:none";
                row.line = L"No uncategorized editable fields";
                row.selectedText = row.line;
                row.typeName = L"info";
                row.valueText = L"";
                row.editable = false;
                out.push_back(row);
            }
        } else if (currentTab_ == TAB_INVENTORY) {
            Property* inventoryProp = FindPlayerProp(state_.save, "runtimeInventory");
            ArrayValue* rows = inventoryProp ? GetArray(*inventoryProp) : NULL;
            if (rows) {
                for (ValuePtr& rowValue : rows->items) {
                    StructValue* rowStruct = std::get_if<StructValue>(&rowValue->data);
                    if (!rowStruct) continue;
                    Property* nameProp = FindPropertyByPrefix(rowStruct->properties, "name");
                    Property* amountProp = FindPropertyByPrefix(rowStruct->properties, "amount");
                    if (!nameProp || !amountProp || !IsStringValue(nameProp->value) || !IsIntValue(amountProp->value)) continue;
                    std::string itemName = std::get<std::string>(nameProp->value->data);
                    UiRow row;
                    row.id = "inventory:" + itemName;
                    row.selectedText = Utf8ToWide(itemName);
                    row.typeName = L"int";
                    row.valueText = std::to_wstring(std::get<std::int32_t>(amountProp->value->data));
                    row.line = row.selectedText + L" = " + row.valueText;
                    out.push_back(row);
                }
            }
        } else if (currentTab_ == TAB_LEVELS) {
            Property* challengesProp = FindPlayerProp(state_.save, "challenges");
            MapValue* map = challengesProp ? GetMap(*challengesProp) : NULL;
            if (map) {
                for (MapEntry& entry : map->items) {
                    if (!IsStringValue(entry.key) || !IsIntValue(entry.value)) continue;
                    std::string key = std::get<std::string>(entry.key->data);
                    UiRow row;
                    row.id = "levels:" + key;
                    row.selectedText = Utf8ToWide(key);
                    row.typeName = L"int";
                    row.valueText = std::to_wstring(std::get<std::int32_t>(entry.value->data));
                    std::wstring linked;
                    if (StartsWith(key, "item") && EndsWith(key, "Lvl")) {
                        Property* amountProp = FindInventoryAmountProp(state_.save, key.substr(0, key.size() - 3));
                        if (amountProp && IsIntValue(amountProp->value)) {
                            linked = L" | inventory " + std::to_wstring(std::get<std::int32_t>(amountProp->value->data));
                        }
                    }
                    row.line = row.selectedText + linked + L" = " + row.valueText;
                    out.push_back(row);
                }
            }
        } else if (currentTab_ == TAB_UPGRADES) {
            Property* upgradesProp = FindPlayerProp(state_.save, "itemsUpgrades");
            MapValue* map = upgradesProp ? GetMap(*upgradesProp) : NULL;
            if (map) {
                for (MapEntry& entry : map->items) {
                    if (!IsStringValue(entry.key)) continue;
                    std::string itemName = std::get<std::string>(entry.key->data);
                    StructValue* itemStruct = std::get_if<StructValue>(&entry.value->data);
                    if (!itemStruct) continue;
                    Property* tweaksProp = FindPropertyByPrefix(itemStruct->properties, "tweaks");
                    MapValue* tweaks = tweaksProp ? GetMap(*tweaksProp) : NULL;
                    if (!tweaks) continue;
                    for (MapEntry& tweak : tweaks->items) {
                        if (!IsStringValue(tweak.key) || !IsIntValue(tweak.value)) continue;
                        std::string upgradeName = std::get<std::string>(tweak.key->data);
                        UiRow row;
                        row.id = "upgrades:" + itemName + ":" + upgradeName;
                        row.selectedText = Utf8ToWide(itemName) + L" / " + Utf8ToWide(upgradeName);
                        row.typeName = L"int";
                        row.valueText = std::to_wstring(std::get<std::int32_t>(tweak.value->data));
                        row.line = row.selectedText + L" = " + row.valueText;
                        out.push_back(row);
                    }
                }
            }
        } else if (currentTab_ == TAB_JOKERS) {
            Property* jokersProp = FindPlayerProp(state_.save, "itemJokers");
            MapValue* map = jokersProp ? GetMap(*jokersProp) : NULL;
            if (map) {
                for (MapEntry& entry : map->items) {
                    if (!IsStringValue(entry.key)) continue;
                    std::string itemName = std::get<std::string>(entry.key->data);
                    StructValue* itemStruct = std::get_if<StructValue>(&entry.value->data);
                    if (!itemStruct) continue;
                    for (Property& prop : itemStruct->properties) {
                        std::string shortName = ShortName(prop.name);
                        if (IsScalarValue(prop.value)) {
                            UiRow row;
                            row.id = "jokers:" + itemName + ":" + shortName;
                            row.selectedText = Utf8ToWide(itemName) + L" / " + Utf8ToWide(shortName);
                            row.typeName = PropertyTypeToWide(prop);
                            row.valueText = ValueToWideString(prop.value);
                            row.line = row.selectedText + L" = " + row.valueText;
                            out.push_back(row);
                        } else {
                            ArrayValue* arr = GetArray(prop);
                            if (!arr) continue;
                            for (size_t i = 0; i < arr->items.size(); ++i) {
                                UiRow row;
                                std::ostringstream id;
                                id << "jokers:" << itemName << ":" << shortName << ":" << i;
                                row.id = id.str();
                                row.selectedText = Utf8ToWide(itemName) + L" / " + Utf8ToWide(shortName) + L"[" + std::to_wstring(static_cast<int>(i)) + L"]";
                                row.typeName = L"name";
                                row.valueText = ValueToWideString(arr->items[i]);
                                row.line = row.selectedText + L" = " + row.valueText;
                                out.push_back(row);
                            }
                        }
                    }
                }
            }
        } else if (currentTab_ == TAB_REWARDS) {
            Property* rewardsProp = FindPlayerProp(state_.save, "rewardedChallenges");
            ArrayValue* arr = rewardsProp ? GetArray(*rewardsProp) : NULL;
            if (arr) {
                for (size_t i = 0; i < arr->items.size(); ++i) {
                    UiRow row;
                    std::ostringstream id;
                    id << "rewards:" << i;
                    row.id = id.str();
                    row.selectedText = L"Reward " + std::to_wstring(static_cast<int>(i));
                    row.typeName = L"name";
                    row.valueText = ValueToWideString(arr->items[i]);
                    row.line = row.selectedText + L" = " + row.valueText;
                    out.push_back(row);
                }
            }
        }
        return out;
    }

    void PopulateList(const std::string& preserveId = "") {
        rows_ = BuildRows();
        filteredRows_.clear();
        SendMessageW(rowList_, LB_RESETCONTENT, 0, 0);
        int selectedIndex = -1;
        for (size_t i = 0; i < rows_.size(); ++i) {
            if (!ContainsCaseInsensitive(rows_[i].line + L" " + rows_[i].valueText, searchText_)) continue;
            filteredRows_.push_back(static_cast<int>(i));
            int addedIndex = static_cast<int>(SendMessageW(rowList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(rows_[i].line.c_str())));
            if (!preserveId.empty() && rows_[i].id == preserveId) {
                selectedIndex = addedIndex;
            }
        }

        if (!filteredRows_.empty()) {
            if (selectedIndex < 0) selectedIndex = 0;
            SendMessageW(rowList_, LB_SETCURSEL, selectedIndex, 0);
        }
        UpdateSelectionDetails();
        SetWindowTextW(statusLabel_, state_.status.c_str());
    }

    UiRow* SelectedRow() {
        int selected = static_cast<int>(SendMessageW(rowList_, LB_GETCURSEL, 0, 0));
        if (selected == LB_ERR || selected < 0 || selected >= static_cast<int>(filteredRows_.size())) return NULL;
        return &rows_[filteredRows_[selected]];
    }

    void UpdateSelectionDetails() {
        UiRow* row = SelectedRow();
        if (!row) {
            SetWindowTextW(selectedStatic_, L"Selected");
            SetWindowTextW(typeStatic_, L"Type");
            SetWindowTextW(detailEdit_, L"No row selected.");
            SetWindowTextW(valueEdit_, L"");
            EnableWindow(valueEdit_, FALSE);
            EnableWindow(applyButton_, FALSE);
            return;
        }
        SetWindowTextW(selectedStatic_, (L"Selected: " + row->selectedText).c_str());
        SetWindowTextW(typeStatic_, (L"Type: " + row->typeName).c_str());
        SetWindowTextW(detailEdit_, (row->line + L"\r\n\r\nSave writes the current value back into the loaded FarFarWest save.").c_str());
        SetWindowTextW(valueEdit_, row->valueText.c_str());
        EnableWindow(valueEdit_, row->editable);
        EnableWindow(applyButton_, row->editable);
    }

    bool SetOverviewValue(const std::string& shortName, const std::wstring& text) {
        std::vector<Property>* playerProps = PlayerProgressProperties(state_.save);
        if (!playerProps) return false;
        Property* prop = FindPropertyByPrefix(*playerProps, shortName);
        if (!prop) return false;
        if (prop->type == "IntProperty") {
            prop->value->data = static_cast<std::int32_t>(_wtoi(text.c_str()));
            return true;
        }
        if (prop->type == "NameProperty" || prop->type == "ObjectProperty") {
            prop->value->data = WideToUtf8(text);
            return true;
        }
        return false;
    }

    bool SetInventoryValue(const std::string& itemName, const std::wstring& text) {
        Property* amountProp = FindInventoryAmountProp(state_.save, itemName);
        if (!amountProp) return false;
        int value = _wtoi(text.c_str());
        amountProp->value->data = static_cast<std::int32_t>(value);
        MapEntry* level = FindChallengeEntry(state_.save, itemName + "Lvl");
        if (level) level->value->data = static_cast<std::int32_t>(LevelFromAmount(value));
        return true;
    }

    bool SetLevelValue(const std::string& key, const std::wstring& text) {
        return SetChallengeValue(state_.save, key, _wtoi(text.c_str())) != 0;
    }

    bool SetUpgradeValue(const std::string& itemName, const std::string& upgradeName, const std::wstring& text) {
        Property* upgradesProp = FindPlayerProp(state_.save, "itemsUpgrades");
        MapValue* map = upgradesProp ? GetMap(*upgradesProp) : NULL;
        if (!map) return false;
        for (MapEntry& entry : map->items) {
            if (!IsStringValue(entry.key) || std::get<std::string>(entry.key->data) != itemName) continue;
            StructValue* itemStruct = std::get_if<StructValue>(&entry.value->data);
            if (!itemStruct) return false;
            Property* tweaksProp = FindPropertyByPrefix(itemStruct->properties, "tweaks");
            MapValue* tweaks = tweaksProp ? GetMap(*tweaksProp) : NULL;
            if (!tweaks) return false;
            for (MapEntry& tweak : tweaks->items) {
                if (!IsStringValue(tweak.key) || std::get<std::string>(tweak.key->data) != upgradeName) continue;
                tweak.value->data = static_cast<std::int32_t>(_wtoi(text.c_str()));
                return true;
            }
        }
        return false;
    }

    bool SetJokerValue(const std::vector<std::string>& parts, const std::wstring& text) {
        if (parts.size() < 3) return false;
        Property* jokersProp = FindPlayerProp(state_.save, "itemJokers");
        MapValue* map = jokersProp ? GetMap(*jokersProp) : NULL;
        if (!map) return false;
        for (MapEntry& entry : map->items) {
            if (!IsStringValue(entry.key) || std::get<std::string>(entry.key->data) != parts[1]) continue;
            StructValue* itemStruct = std::get_if<StructValue>(&entry.value->data);
            if (!itemStruct) return false;
            Property* prop = FindPropertyByPrefix(itemStruct->properties, parts[2]);
            if (!prop) return false;
            if (parts.size() == 3 && IsStringValue(prop->value)) {
                prop->value->data = WideToUtf8(text);
                return true;
            }
            if (parts.size() == 4) {
                ArrayValue* arr = GetArray(*prop);
                if (!arr) return false;
                int index = atoi(parts[3].c_str());
                if (index < 0 || index >= static_cast<int>(arr->items.size())) return false;
                arr->items[index]->data = WideToUtf8(text);
                return true;
            }
        }
        return false;
    }

    bool SetRewardValue(const std::string& indexText, const std::wstring& text) {
        Property* rewardsProp = FindPlayerProp(state_.save, "rewardedChallenges");
        ArrayValue* arr = rewardsProp ? GetArray(*rewardsProp) : NULL;
        if (!arr) return false;
        int index = atoi(indexText.c_str());
        if (index < 0 || index >= static_cast<int>(arr->items.size())) return false;
        arr->items[index]->data = WideToUtf8(text);
        return true;
    }

    std::vector<std::string> SplitId(const std::string& value) {
        std::vector<std::string> parts;
        std::string current;
        for (char ch : value) {
            if (ch == ':') {
                parts.push_back(current);
                current.clear();
            } else {
                current.push_back(ch);
            }
        }
        parts.push_back(current);
        return parts;
    }

    bool ApplyCurrentEdit() {
        UiRow* row = SelectedRow();
        if (!row || !row->editable) return false;
        wchar_t buffer[4096];
        GetWindowTextW(valueEdit_, buffer, 4096);
        std::wstring text = buffer;
        std::vector<std::string> parts = SplitId(row->id);
        bool changed = false;
        if (parts[0] == "overview" || parts[0] == "other") {
            changed = SetOverviewValue(parts[1], text);
        } else if (parts[0] == "inventory") {
            changed = SetInventoryValue(parts[1], text);
        } else if (parts[0] == "levels") {
            changed = SetLevelValue(parts[1], text);
        } else if (parts[0] == "upgrades" && parts.size() == 3) {
            changed = SetUpgradeValue(parts[1], parts[2], text);
        } else if (parts[0] == "jokers") {
            changed = SetJokerValue(parts, text);
        } else if (parts[0] == "rewards" && parts.size() == 2) {
            changed = SetRewardValue(parts[1], text);
        }
        if (changed) {
            state_.status = L"Updated " + row->selectedText + L". Save to write the file.";
            PopulateList(row->id);
        }
        return changed;
    }

    void RefreshViews() {
        currentTab_ = TabCtrl_GetCurSel(tabControl_);
        if (currentTab_ < 0) currentTab_ = TAB_OVERVIEW;
        wchar_t buffer[512];
        GetWindowTextW(searchEdit_, buffer, 512);
        searchText_ = buffer;
        PopulateList();
    }

    bool LoadSaveFile(const std::wstring& path) {
        try {
            std::wstring seed = SeedForSavePath(path);
            ByteVec encrypted = ReadFileBytes(path);
            state_.key = DeriveKey(seed);
            ByteVec plain = DecryptSaveBytes(encrypted, state_.key);
            state_.save = ParseGvas(plain);
            state_.savePath = path;
            state_.loaded = true;
            state_.status = L"Loaded " + path;
            currentTab_ = TabCtrl_GetCurSel(tabControl_);
            SetWindowTextW(searchEdit_, L"");
            RefreshViews();
            return true;
        } catch (const std::exception& ex) {
            state_.status = L"Load failed.";
            RefreshViews();
            MessageBoxA(hwnd_, ex.what(), "Load failed", MB_ICONERROR);
            return false;
        }
    }

    bool SaveTo(const std::wstring& path, bool makeBackup) {
        try {
            if (!state_.loaded) throw std::runtime_error("No save is loaded");
            ByteVec plain = SerializeGvas(state_.save);
            ByteVec encrypted = EncryptSaveBytes(plain, state_.key);
            if (makeBackup && PathFileExistsW(path.c_str())) {
                SYSTEMTIME st;
                GetLocalTime(&st);
                wchar_t backupName[MAX_PATH];
                std::wstring stem = path;
                size_t dot = stem.find_last_of(L'.');
                if (dot != std::wstring::npos) stem = stem.substr(0, dot);
                wsprintfW(backupName, L"%s.backup_cpp_%04d%02d%02d_%02d%02d%02d.save",
                          stem.c_str(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                CopyFileW(path.c_str(), backupName, FALSE);
            }
            std::wstring temp = path + L".tmp";
            WriteFileBytes(temp, encrypted);
            MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
            state_.savePath = path;
            state_.status = L"Saved " + path;
            RefreshViews();
            return true;
        } catch (const std::exception& ex) {
            state_.status = L"Save failed.";
            RefreshViews();
            MessageBoxA(hwnd_, ex.what(), "Save failed", MB_ICONERROR);
            return false;
        }
    }

    std::optional<std::wstring> PickOpenPath() {
        OPENFILENAMEW ofn = {};
        wchar_t buffer[MAX_PATH] = {};
        std::wstring initialDir = DefaultSaveDir();
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = L"Save files (*.save)\0*.save\0All files (*.*)\0*.*\0";
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        ofn.lpstrInitialDir = initialDir.c_str();
        if (GetOpenFileNameW(&ofn)) return std::wstring(buffer);
        return std::nullopt;
    }

    std::optional<std::wstring> PickSavePath() {
        OPENFILENAMEW ofn = {};
        wchar_t buffer[MAX_PATH] = {};
        std::wstring initialDir = DefaultSaveDir();
        if (state_.savePath.has_value()) lstrcpynW(buffer, PathFindFileNameW(state_.savePath.value().c_str()), MAX_PATH);
        else lstrcpynW(buffer, L"edited.save", MAX_PATH);
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = L"Save files (*.save)\0*.save\0All files (*.*)\0*.*\0";
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT;
        ofn.lpstrInitialDir = initialDir.c_str();
        ofn.lpstrDefExt = L"save";
        if (GetSaveFileNameW(&ofn)) return std::wstring(buffer);
        return std::nullopt;
    }

    void OpenDefaultSaveFolder() {
        std::wstring folder = DefaultSaveDir();
        SHCreateDirectoryExW(hwnd_, folder.c_str(), NULL);
        ShellExecuteW(hwnd_, L"open", folder.c_str(), NULL, NULL, SW_SHOWDEFAULT);
    }

    void DoBulkAction(const wchar_t* actionName, const std::function<void()>& action) {
        if (!state_.loaded) {
            MessageBoxW(hwnd_, L"Open a save file first.", actionName, MB_ICONINFORMATION);
            return;
        }
        try {
            UiRow* selected = SelectedRow();
            std::string preserveId = selected ? selected->id : "";
            action();
            state_.status = std::wstring(actionName) + L" applied. Save to write the file.";
            PopulateList(preserveId);
        } catch (const std::exception& ex) {
            MessageBoxA(hwnd_, ex.what(), "Action failed", MB_ICONERROR);
        }
    }

    void HandleCommand(int id) {
        switch (id) {
        case IDC_BTN_OPEN: {
            std::optional<std::wstring> path = PickOpenPath();
            if (path.has_value()) LoadSaveFile(path.value());
            break;
        }
        case IDC_BTN_AUTO_IMPORT: {
            std::optional<std::wstring> path = FindLatestSave();
            if (!path.has_value()) {
                std::wstring message = L"No save found in:\n" + DefaultSaveDir();
                MessageBoxW(hwnd_, message.c_str(), L"Save not found", MB_ICONINFORMATION);
                break;
            }
            LoadSaveFile(path.value());
            break;
        }
        case IDC_BTN_OPEN_FOLDER:
            OpenDefaultSaveFolder();
            break;
        case IDC_BTN_SAVE:
            if (state_.savePath.has_value()) SaveTo(state_.savePath.value(), true);
            else {
                std::optional<std::wstring> path = PickSavePath();
                if (path.has_value()) SaveTo(path.value(), PathFileExistsW(path.value().c_str()) != FALSE);
            }
            break;
        case IDC_BTN_SAVE_AS: {
            std::optional<std::wstring> path = PickSavePath();
            if (path.has_value()) SaveTo(path.value(), PathFileExistsW(path.value().c_str()) != FALSE);
            break;
        }
        case IDC_BTN_WEAPON_100:
            DoBulkAction(L"Weapon levels 100", [this]() { SetAllWeaponLevels(state_.save, 100); });
            break;
        case IDC_BTN_SPELL_100:
            DoBulkAction(L"Spell levels 100", [this]() { SetAllSpellLevels(state_.save, 100); });
            break;
        case IDC_BTN_PRESTIGE_10:
            DoBulkAction(L"Weapon prestige 10", [this]() { EnsureWeaponPrestigeInventory(state_.save, 10); });
            break;
        case IDC_BTN_ADD_BUILDABLE:
            DoBulkAction(L"Add missing buildable weapons", [this]() { AddMissingBuildableWeapons(state_.save); });
            break;
        case IDC_BTN_UNLOCK_ALL:
            DoBulkAction(L"Unlock All", [this]() { UnlockAll(state_.save); });
            break;
        case IDC_BTN_APPLY:
            ApplyCurrentEdit();
            break;
        default:
            break;
        }
    }

    void PaintBackground(HDC hdc) {
        RECT rect;
        GetClientRect(hwnd_, &rect);

        TRIVERTEX verts[2] = {
            { 0, 0, static_cast<COLOR16>(GetRValue(gColors.bgTop) << 8), static_cast<COLOR16>(GetGValue(gColors.bgTop) << 8), static_cast<COLOR16>(GetBValue(gColors.bgTop) << 8), 0xFFFF },
            { rect.right, rect.bottom, static_cast<COLOR16>(GetRValue(gColors.bgBottom) << 8), static_cast<COLOR16>(GetGValue(gColors.bgBottom) << 8), static_cast<COLOR16>(GetBValue(gColors.bgBottom) << 8), 0xFFFF }
        };
        GRADIENT_RECT gradient = { 0, 1 };
        GradientFill(hdc, verts, 2, &gradient, 1, GRADIENT_FILL_RECT_V);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, gColors.text);
        HFONT old = reinterpret_cast<HFONT>(SelectObject(hdc, titleFont_));
        TextOutW(hdc, 20, 18, kWindowTitle, lstrlenW(kWindowTitle));
        SelectObject(hdc, subtitleFont_);
        SetTextColor(hdc, gColors.textMuted);
        const wchar_t* subtitle = L"Native C++ FarFarWest editor with full save sections, direct field editing, auto-import, and a dark liquid-glass inspired desktop UI.";
        TextOutW(hdc, 22, 56, subtitle, lstrlenW(subtitle));
        SelectObject(hdc, old);
    }

    LRESULT HandleColorEdit(WPARAM wParam, LPARAM lParam) {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        SetTextColor(hdc, gColors.text);
        COLORREF bg = (control == rowList_) ? gColors.panelAlt : gColors.panel;
        SetBkColor(hdc, bg);
        return reinterpret_cast<LRESULT>(control == rowList_ ? panelAltBrush_ : panelBrush_);
    }

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_CREATE:
            ApplyWindowTheme();
            CreateControls();
            RefreshViews();
            return 0;
        case WM_SIZE:
            LayoutControls(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_LIST_ROWS && HIWORD(wParam) == LBN_SELCHANGE) {
                UpdateSelectionDetails();
                return 0;
            }
            if (LOWORD(wParam) == IDC_EDIT_SEARCH && HIWORD(wParam) == EN_CHANGE) {
                RefreshViews();
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED) {
                HandleCommand(LOWORD(wParam));
            }
            return 0;
        case WM_NOTIFY:
            if (reinterpret_cast<LPNMHDR>(lParam)->hwndFrom == tabControl_ &&
                reinterpret_cast<LPNMHDR>(lParam)->code == TCN_SELCHANGE) {
                RefreshViews();
                return 0;
            }
            return 0;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            return HandleColorEdit(wParam, lParam);
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd_, &ps);
            PaintBackground(hdc);
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_DESTROY:
            if (uiFont_) DeleteObject(uiFont_);
            if (titleFont_) DeleteObject(titleFont_);
            if (subtitleFont_) DeleteObject(subtitleFont_);
            if (panelBrush_) DeleteObject(panelBrush_);
            if (panelAltBrush_) DeleteObject(panelAltBrush_);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, msg, wParam, lParam);
        }
    }
};

} // namespace ffw

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc >= 3 && lstrcmpiW(argv[1], L"--smoke-test") == 0) {
            int code = ffw::RunSmokeTest(argv[2]);
            LocalFree(argv);
            return code;
        }
        LocalFree(argv);
    }

    ffw::AppWindow app;
    if (!app.Create(hInstance)) {
        MessageBoxW(NULL, L"Unable to create the main window.", L"FarFarWest Unlock all tool", MB_ICONERROR);
        return 1;
    }
    return app.Run();
}
