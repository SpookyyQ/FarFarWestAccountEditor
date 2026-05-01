#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <bcrypt.h>
#include <wrl/client.h>
#include "WebView2.h"

#include <algorithm>
#include <atomic>
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
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace ffw {

using ByteVec = std::vector<unsigned char>;
static const UINT WM_APP_LOAD_COMPLETE = WM_APP + 1;

static const wchar_t* kWindowClass = L"FarFarWestUnlockAllToolWindow";
static const wchar_t* kWindowTitle = L"Far Far West Unlock all tool";
static const wchar_t* kPartySuffix = L"NicoArnoEvilRaptorFireshineRobbo";
static const int kInt32Max = 2147483647;
static const char* kAppVersion = "1.4.7";

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
    COLORREF bgTop = RGB(16, 16, 24);
    COLORREF bgBottom = RGB(26, 24, 36);
    COLORREF panel = RGB(24, 24, 32);
    COLORREF panelAlt = RGB(18, 18, 26);
    COLORREF panelGlass = RGB(34, 34, 46);
    COLORREF panelEdge = RGB(52, 52, 68);
    COLORREF border = RGB(74, 74, 94);
    COLORREF accent = RGB(134, 114, 255);
    COLORREF accentSoft = RGB(98, 86, 186);
    COLORREF accentDeep = RGB(46, 42, 82);
    COLORREF accentBright = RGB(222, 216, 255);
    COLORREF text = RGB(244, 244, 248);
    COLORREF textMuted = RGB(164, 166, 182);
    COLORREF success = RGB(126, 199, 158);
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

static std::wstring HumanizeIdentifier(const std::string& value) {
    std::wstring wide = Utf8ToWide(value);
    std::wstring out;
    out.reserve(wide.size() + 8);
    for (size_t i = 0; i < wide.size(); ++i) {
        wchar_t ch = wide[i];
        if (ch == L'_' || ch == L'-' || ch == L'/') {
            if (!out.empty() && out.back() != L' ') out.push_back(L' ');
            continue;
        }

        if (!out.empty()) {
            wchar_t prev = wide[i - 1];
            bool splitBefore =
                ((iswlower(prev) || iswdigit(prev)) && iswupper(ch)) ||
                (iswalpha(prev) && iswdigit(ch)) ||
                (iswdigit(prev) && iswalpha(ch));
            if (splitBefore && out.back() != L' ') out.push_back(L' ');
        }

        if (out.empty() || out.back() == L' ') out.push_back(static_cast<wchar_t>(towupper(ch)));
        else out.push_back(ch);
    }
    return Trim(out);
}

static std::wstring FriendlyItemLabel(const std::string& itemName) {
    return Utf8ToWide(FriendlyItemName(itemName));
}

static std::wstring FriendlyLevelLabel(const std::string& key) {
    if (StartsWith(key, "item") && EndsWith(key, "Lvl") && key.size() > 3) {
        return FriendlyItemLabel(key.substr(0, key.size() - 3)) + L" Level";
    }
    return HumanizeIdentifier(key);
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
    ByteVec guidRaw;  // HasPropertyGuid (U8) + optional 16-byte PropertyGuid
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
    if (type == "EnumProperty" || type == "ByteProperty") {
        meta->innerType = reader.FString();
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

static std::string HexNumber(size_t value, int width = 0) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setfill('0');
    if (width > 0) out << std::setw(width);
    out << value;
    return out.str();
}

static std::string HexByte(unsigned char value) {
    return HexNumber(static_cast<size_t>(value), 2);
}

static std::string BytePreview(const ByteVec& bytes, size_t at, size_t radius = 8) {
    if (bytes.empty()) return "";
    size_t start = at > radius ? at - radius : 0;
    size_t end = std::min(bytes.size(), at + radius + 1);
    std::ostringstream out;
    for (size_t i = start; i < end; ++i) {
        if (i > start) out << ' ';
        if (i == at) out << '[';
        out << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>(bytes[i]);
        if (i == at) out << ']';
    }
    return out.str();
}

static std::string InvalidPropertyTerminatorMessage(
    const std::string& name,
    const std::string& type,
    size_t nameOffset,
    size_t typeOffset,
    size_t metaOffset,
    size_t sizeOffset,
    size_t terminatorOffset,
    std::int32_t declaredSize,
    unsigned char found,
    const ByteVec& bytes) {
    std::ostringstream out;
    out << "Invalid property terminator\n\n"
        << "Property name: " << name << "\n"
        << "Property type: " << type << "\n"
        << "Declared value size: " << declaredSize << "\n\n"
        << "Name offset: " << HexNumber(nameOffset) << "\n"
        << "Type offset: " << HexNumber(typeOffset) << "\n"
        << "Meta offset: " << HexNumber(metaOffset) << "\n"
        << "Size offset: " << HexNumber(sizeOffset) << "\n"
        << "Terminator offset: " << HexNumber(terminatorOffset) << "\n\n"
        << "Expected terminator: 0x00\n"
        << "Found: " << HexByte(found) << "\n"
        << "Bytes around terminator: " << BytePreview(bytes, terminatorOffset) << "\n\n"
        << "Please send this full message together with the failing .save file.";
    return out.str();
}

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

static Property ReadProperty(Reader& reader, const std::string& name, size_t nameOffset) {
    Property property;
    property.name = name;
    size_t typeOffset = reader.pos;
    property.type = reader.FString();
    size_t metaStart = reader.pos;
    property.meta = ParseMeta(reader, property.type);
    property.metaRaw.assign(reader.buffer->begin() + static_cast<long long>(metaStart),
                            reader.buffer->begin() + static_cast<long long>(reader.pos));
    size_t sizeOffset = reader.pos;
    std::int32_t size = reader.I32();
    size_t terminatorOffset = reader.pos;
    unsigned char hasPropertyGuid = reader.U8();
    property.guidRaw.push_back(hasPropertyGuid);
    if (hasPropertyGuid) {
        if (reader.pos + 16 > reader.buffer->size()) {
            throw std::runtime_error(InvalidPropertyTerminatorMessage(
                property.name,
                property.type,
                nameOffset,
                typeOffset,
                metaStart,
                sizeOffset,
                terminatorOffset,
                size,
                hasPropertyGuid,
                *reader.buffer));
        }
        ByteVec guid = reader.ReadBytes(16);
        property.guidRaw.insert(property.guidRaw.end(), guid.begin(), guid.end());
    }
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
        size_t nameOffset = reader.pos;
        std::string name = reader.FString();
        if (name == "None") break;
        props.push_back(ReadProperty(reader, name, nameOffset));
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
    if (!property.guidRaw.empty()) {
        writer.WriteBytes(property.guidRaw);
    } else {
        writer.WriteU8(0);
    }
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
    SetAllWeaponLevels(save, 100);

    for (size_t i = 0; i < sizeof(kBuildableWeapons) / sizeof(kBuildableWeapons[0]); ++i) {
        std::string prestigeName = WideToUtf8(kBuildableWeapons[i]) + "Prestige";
        Property* p = FindInventoryAmountProp(save, prestigeName);
        if (p) p->value->data = static_cast<std::int32_t>(10);
    }

    MapEntry* prestige = FindChallengeEntry(save, "Prestige");
    if (prestige) prestige->value->data = static_cast<std::int32_t>(10);

    MapEntry* heroLvl = FindChallengeEntry(save, "itemHeroLvl");
    if (heroLvl) heroLvl->value->data = static_cast<std::int32_t>(100);

    Property* soul = FindInventoryAmountProp(save, "moneySoul");
    if (soul) soul->value->data = static_cast<std::int32_t>(300000);

    Property* gold = FindInventoryAmountProp(save, "moneyGold");
    if (gold) gold->value->data = static_cast<std::int32_t>(9999999);
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
    IDC_TAB_OVERVIEW,
    IDC_TAB_INVENTORY,
    IDC_TAB_LEVELS,
    IDC_TAB_UPGRADES,
    IDC_TAB_JOKERS,
    IDC_TAB_REWARDS,
    IDC_TAB_OTHER,
    IDC_EDIT_SEARCH,
    IDC_LIST_ROWS,
    IDC_EDIT_DETAIL,
    IDC_EDIT_VALUE,
    IDC_BTN_APPLY,
    IDC_STATIC_SUMMARY,
    IDC_STATIC_SEARCH_LABEL,
    IDC_STATIC_LIST_LABEL,
    IDC_STATIC_EDITOR_LABEL,
    IDC_STATIC_VALUE_LABEL,
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
    std::wstring rawName;
    std::wstring noteText;
    std::wstring typeName;
    std::wstring valueText;
    bool editable = true;
    bool featured = false;
};

struct AsyncLoadResult {
    unsigned int token = 0;
    std::wstring path;
    ByteVec key;
    SaveFile save;
    std::string error;
    bool success = false;
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

static std::string JsonEscapeUtf8(const std::string& value) {
    std::ostringstream out;
    for (unsigned char ch : value) {
        switch (ch) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch) << std::dec;
            } else {
                out << static_cast<char>(ch);
            }
            break;
        }
    }
    return out.str();
}

static std::string JsonString(const std::wstring& value) {
    return "\"" + JsonEscapeUtf8(WideToUtf8(value)) + "\"";
}

static std::string JsonStringUtf8(const std::string& value) {
    return "\"" + JsonEscapeUtf8(value) + "\"";
}

static std::wstring UrlDecodeComponent(const std::wstring& value) {
    std::string bytes;
    bytes.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        wchar_t ch = value[i];
        if (ch == L'%' && i + 2 < value.size()) {
            wchar_t hex[3] = { value[i + 1], value[i + 2], 0 };
            bytes.push_back(static_cast<char>(wcstol(hex, NULL, 16)));
            i += 2;
        } else if (ch == L'+') {
            bytes.push_back(' ');
        } else {
            bytes.push_back(static_cast<char>(ch));
        }
    }
    return Utf8ToWide(bytes);
}

static bool StartsWithWide(const std::wstring& value, const wchar_t* prefix) {
    size_t prefixLen = lstrlenW(prefix);
    return value.size() >= prefixLen && value.compare(0, prefixLen, prefix) == 0;
}

static std::wstring ExecutableDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return path;
}

static std::wstring UiAssetDir() {
    return ExecutableDir() + L"\\ui";
}

static std::wstring WebView2UserDataDir() {
    wchar_t localAppData[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, localAppData))) {
        GetTempPathW(MAX_PATH, localAppData);
    }
    return std::wstring(localAppData) + L"\\FarFarWestUnlockAllTool\\WebView2Data";
}

static std::wstring HResultMessage(HRESULT hr) {
    LPWSTR buffer = NULL;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD size = FormatMessageW(flags, NULL, static_cast<DWORD>(hr), 0, reinterpret_cast<LPWSTR>(&buffer), 0, NULL);
    std::wstring message;
    if (size && buffer) {
        message.assign(buffer, size);
        LocalFree(buffer);
    } else {
        std::wostringstream out;
        out << L"HRESULT 0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr);
        message = out.str();
    }
    return Trim(message);
}

static std::wstring WebView2DownloadMessage() {
    return L"\n\nInstall the Microsoft Edge WebView2 Runtime and start the tool again:\n"
           L"https://developer.microsoft.com/microsoft-edge/webview2/";
}

static std::wstring DescribeWebView2Failure(HRESULT hr) {
    std::wstring message = HResultMessage(hr);
    message += L"\n\nError code: HRESULT 0x";
    std::wostringstream code;
    code << std::hex << std::uppercase << static_cast<unsigned long>(hr);
    message += code.str();

    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
        message += L"\n\nWebView2 Runtime was not found on this PC.";
        message += WebView2DownloadMessage();
    } else if (hr == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS)) {
        message += L"\n\nThe WebView2 data folder path points to an existing file instead of a folder.";
    } else if (hr == HRESULT_FROM_WIN32(ERROR_PRODUCT_UNINSTALLED)) {
        message += L"\n\nThe installed WebView2 Runtime appears to be broken or uninstalled.";
        message += WebView2DownloadMessage();
    } else if (hr == HRESULT_FROM_WIN32(ERROR_DISK_FULL)) {
        message += L"\n\nWebView2 could not start because the disk is full or old runtime versions could not be cleaned up.";
    } else if (hr == E_ACCESSDENIED) {
        message += L"\n\nWebView2 could not create or write to its user data folder.";
    } else if (hr == E_INVALIDARG || hr == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER)) {
        message += L"\n\nWebView2 rejected one of the startup parameters.";
    } else if (hr == E_FAIL) {
        message += L"\n\nThe WebView2 Runtime was found, but the browser process could not start.";
    }
    return message;
}

static bool IsWebView2RuntimeAvailable(std::wstring* version, HRESULT* failure) {
    LPWSTR versionInfo = NULL;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(NULL, &versionInfo);
    if (SUCCEEDED(hr) && versionInfo) {
        if (version) *version = versionInfo;
        CoTaskMemFree(versionInfo);
        return true;
    }
    if (versionInfo) CoTaskMemFree(versionInfo);
    if (failure) *failure = hr;
    return false;
}

template <typename Interface>
const IID& CallbackInterfaceId();

template <>
inline const IID& CallbackInterfaceId<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>() {
    return IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler;
}

template <>
inline const IID& CallbackInterfaceId<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>() {
    return IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler;
}

template <>
inline const IID& CallbackInterfaceId<ICoreWebView2WebMessageReceivedEventHandler>() {
    return IID_ICoreWebView2WebMessageReceivedEventHandler;
}

template <typename Interface>
class ComCallbackBase : public Interface {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (!ppvObject) return E_POINTER;
        *ppvObject = NULL;
        if (riid == IID_IUnknown || riid == CallbackInterfaceId<Interface>()) {
            *ppvObject = static_cast<Interface*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(++refCount_);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = static_cast<ULONG>(--refCount_);
        if (count == 0) delete this;
        return count;
    }

protected:
    virtual ~ComCallbackBase() = default;

private:
    std::atomic<ULONG> refCount_{ 1 };
};

class EnvironmentCompletedHandler : public ComCallbackBase<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> {
public:
    explicit EnvironmentCompletedHandler(std::function<HRESULT(HRESULT, ICoreWebView2Environment*)> callback)
        : callback_(std::move(callback)) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* result) override {
        return callback_(errorCode, result);
    }

private:
    std::function<HRESULT(HRESULT, ICoreWebView2Environment*)> callback_;
};

class ControllerCompletedHandler : public ComCallbackBase<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
public:
    explicit ControllerCompletedHandler(std::function<HRESULT(HRESULT, ICoreWebView2Controller*)> callback)
        : callback_(std::move(callback)) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* result) override {
        return callback_(errorCode, result);
    }

private:
    std::function<HRESULT(HRESULT, ICoreWebView2Controller*)> callback_;
};

class WebMessageReceivedHandler : public ComCallbackBase<ICoreWebView2WebMessageReceivedEventHandler> {
public:
    explicit WebMessageReceivedHandler(std::function<HRESULT(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*)> callback)
        : callback_(std::move(callback)) {}

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        return callback_(sender, args);
    }

private:
    std::function<HRESULT(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*)> callback_;
};

class WebView2EnvironmentOptions : public ICoreWebView2EnvironmentOptions {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (!ppvObject) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2EnvironmentOptions) {
            *ppvObject = static_cast<ICoreWebView2EnvironmentOptions*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount_; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = --refCount_;
        if (count == 0) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE get_AdditionalBrowserArguments(LPWSTR* value) override {
        return DupCoTaskW(kArgs_, value);
    }
    HRESULT STDMETHODCALLTYPE put_AdditionalBrowserArguments(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE get_Language(LPWSTR* value) override { return DupCoTaskW(L"", value); }
    HRESULT STDMETHODCALLTYPE put_Language(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE get_TargetCompatibleBrowserVersion(LPWSTR* value) override { return DupCoTaskW(L"", value); }
    HRESULT STDMETHODCALLTYPE put_TargetCompatibleBrowserVersion(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE get_AllowSingleSignOnUsingOSPrimaryAccount(BOOL* allow) override {
        *allow = FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE put_AllowSingleSignOnUsingOSPrimaryAccount(BOOL) override { return S_OK; }

private:
    static constexpr const wchar_t* kArgs_ =
        L"--disable-background-networking "
        L"--disable-client-side-phishing-detection "
        L"--no-pings "
        L"--metrics-recording-only "
        L"--disable-sync";

    static HRESULT DupCoTaskW(const wchar_t* src, LPWSTR* out) {
        size_t bytes = (wcslen(src) + 1) * sizeof(wchar_t);
        *out = static_cast<LPWSTR>(CoTaskMemAlloc(bytes));
        if (!*out) return E_OUTOFMEMORY;
        memcpy(*out, src, bytes);
        return S_OK;
    }

    std::atomic<ULONG> refCount_{ 1 };
};

class AppWindow {
public:
    AppWindow() = default;

    bool Create(HINSTANCE instance) {
        hInstance_ = instance;
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
        InitCommonControlsEx(&icc);

        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WindowProcSetup;
        wc.hInstance = instance;
        wc.lpszClassName = kWindowClass;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.hIcon   = LoadIconW(instance, MAKEINTRESOURCEW(1));
        wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
        RegisterClassExW(&wc);

        hwnd_ = CreateWindowExW(
            0, kWindowClass, kWindowTitle,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1440, 980,
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
    static constexpr const wchar_t* kTabTitles_[7] = {
        L"Overview",
        L"Inventory",
        L"Levels",
        L"Upgrades",
        L"Jokers",
        L"Rewards",
        L"Other"
    };

    HINSTANCE hInstance_ = NULL;
    HWND hwnd_ = NULL;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> webEnvironment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> webController_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView_;
    AppState state_;
    int currentTab_ = TAB_OVERVIEW;
    std::vector<UiRow> rows_;
    std::vector<int> filteredRows_;
    std::wstring searchText_;
    std::string selectedRowId_;
    bool isLoading_ = false;
    unsigned int nextLoadToken_ = 0;
    bool webViewReady_ = false;
    std::wstring paintMessage_ = L"Starting WebView2 UI...";

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

    void InitializeWebView() {
        paintMessage_ = L"Loading WebView2 environment...";
        InvalidateRect(hwnd_, NULL, TRUE);

        HRESULT runtimeHr = S_OK;
        if (!IsWebView2RuntimeAvailable(NULL, &runtimeHr)) {
            std::wstring message = L"WebView2 Runtime is missing or unavailable.\n\n";
            message += DescribeWebView2Failure(runtimeHr);
            MessageBoxW(hwnd_, message.c_str(), kWindowTitle, MB_ICONERROR);
            paintMessage_ = L"WebView2 Runtime is missing.";
            InvalidateRect(hwnd_, NULL, TRUE);
            return;
        }

        auto* environmentHandler = new EnvironmentCompletedHandler(
            [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || !environment) {
                    std::wstring message = L"WebView2 could not be initialized.\n\n";
                    message += DescribeWebView2Failure(result);
                    MessageBoxW(hwnd_, message.c_str(), kWindowTitle, MB_ICONERROR);
                    paintMessage_ = L"WebView2 initialization failed.";
                    InvalidateRect(hwnd_, NULL, TRUE);
                    return result;
                }

                webEnvironment_ = environment;
                auto* controllerHandler = new ControllerCompletedHandler(
                    [this](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                        if (FAILED(controllerResult) || !controller) {
                            std::wstring message = L"WebView2 controller could not be created.\n\n";
                            message += DescribeWebView2Failure(controllerResult);
                            MessageBoxW(hwnd_, message.c_str(), kWindowTitle, MB_ICONERROR);
                            paintMessage_ = L"WebView2 controller initialization failed.";
                            InvalidateRect(hwnd_, NULL, TRUE);
                            return controllerResult;
                        }

                        webController_ = controller;
                        webController_->get_CoreWebView2(webView_.GetAddressOf());
                        webController_->put_IsVisible(TRUE);
                        ResizeWebView();

                        auto* webMessageHandler = new WebMessageReceivedHandler(
                            [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                LPWSTR raw = NULL;
                                if (args && SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw) {
                                    std::wstring message = raw;
                                    CoTaskMemFree(raw);
                                    HandleWebMessage(message);
                                }
                                return S_OK;
                            });
                        webView_->add_WebMessageReceived(webMessageHandler, nullptr);
                        webMessageHandler->Release();

                        std::wstring uiDir = UiAssetDir();
                        std::wstring uiIndex = uiDir + L"\\index.html";
                        if (PathFileExistsW(uiIndex.c_str())) {
                            Microsoft::WRL::ComPtr<ICoreWebView2_3> webView3;
                            if (SUCCEEDED(webView_->QueryInterface(IID_ICoreWebView2_3, reinterpret_cast<void**>(webView3.GetAddressOf()))) && webView3) {
                                webView3->SetVirtualHostNameToFolderMapping(
                                    L"appassets.local",
                                    uiDir.c_str(),
                                    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                            }
                            webView_->Navigate(L"https://appassets.local/index.html");
                            paintMessage_ = L"Loading modern HTML/CSS UI...";
                        } else {
                            webView_->NavigateToString(
                                L"<html><body style='background:#101018;color:#f4f4f8;font-family:Segoe UI;padding:32px'>"
                                L"<h2>UI assets not found</h2><p>The WebView2 host started, but <code>ui/index.html</code> is missing next to the executable.</p>"
                                L"</body></html>");
                            paintMessage_ = L"UI assets missing.";
                        }
                        return S_OK;
                    });

                HRESULT controllerHr = webEnvironment_->CreateCoreWebView2Controller(hwnd_, controllerHandler);
                controllerHandler->Release();
                if (FAILED(controllerHr)) {
                    std::wstring message = L"WebView2 controller startup failed.\n\n";
                    message += DescribeWebView2Failure(controllerHr);
                    MessageBoxW(hwnd_, message.c_str(), kWindowTitle, MB_ICONERROR);
                    paintMessage_ = L"WebView2 controller startup failed.";
                    InvalidateRect(hwnd_, NULL, TRUE);
                }
                return controllerHr;
            });

        std::wstring userDataDir = WebView2UserDataDir();
        SHCreateDirectoryExW(hwnd_, userDataDir.c_str(), NULL);
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            NULL,
            userDataDir.c_str(),
            NULL,
            environmentHandler);
        environmentHandler->Release();

        if (FAILED(hr)) {
            std::wstring message = L"WebView2 startup call failed.\n\n";
            message += DescribeWebView2Failure(hr);
            MessageBoxW(hwnd_, message.c_str(), kWindowTitle, MB_ICONERROR);
            paintMessage_ = L"WebView2 startup call failed.";
            InvalidateRect(hwnd_, NULL, TRUE);
        }
    }

    void ResizeWebView() {
        if (!webController_) return;
        RECT client;
        GetClientRect(hwnd_, &client);
        webController_->put_Bounds(client);
    }

    const wchar_t* CurrentTabTitle() const {
        return kTabTitles_[currentTab_];
    }

    std::wstring CurrentSaveName() const {
        if (!state_.savePath.has_value()) return L"No save loaded";
        return PathFindFileNameW(state_.savePath.value().c_str());
    }

    std::wstring BuildSummaryText() const {
        std::wostringstream summary;
        if (isLoading_) {
            summary << L"Loading save in background. The UI stays responsive while data imports.";
        } else if (state_.loaded) {
            summary << L"Save: " << CurrentSaveName()
                    << L" | Tab: " << CurrentTabTitle()
                    << L" | Visible: " << filteredRows_.size()
                    << L" / " << rows_.size();
            if (!searchText_.empty()) {
                summary << L" | Filter: " << searchText_;
            }
        } else {
            summary << L"Open a save or use auto import to begin.";
        }
        return summary.str();
    }

    std::wstring BuildDetailText(const UiRow& row) const {
        std::wstring detail = row.selectedText;
        if (!row.rawName.empty() && row.rawName != row.selectedText) {
            detail += L"\nRaw key: " + row.rawName;
        }
        if (!row.noteText.empty()) {
            detail += L"\n" + row.noteText;
        }
        detail += L"\nType: " + row.typeName;
        detail += L"\nCurrent value: " + row.valueText;
        if (row.editable) {
            detail += L"\n\nApply updates the loaded save in memory. Use Save to write the file.";
        }
        return detail;
    }

    UiRow* FindRowById(const std::string& id) {
        for (UiRow& row : rows_) {
            if (row.id == id) return &row;
        }
        return NULL;
    }

    UiRow* SelectedRow() {
        return selectedRowId_.empty() ? NULL : FindRowById(selectedRowId_);
    }

    const UiRow* SelectedRow() const {
        if (selectedRowId_.empty()) return NULL;
        for (const UiRow& row : rows_) {
            if (row.id == selectedRowId_) return &row;
        }
        return NULL;
    }

    void PublishState() {
        if (!webView_ || !webViewReady_) return;
        std::wstring json = Utf8ToWide(BuildStateJson());
        webView_->PostWebMessageAsJson(json.c_str());
    }

    std::string BuildStateJson() const {
        std::ostringstream out;
        out << "{";
        out << "\"type\":\"state\",";
        out << "\"version\":" << JsonString(Utf8ToWide(kAppVersion)) << ",";
        out << "\"loaded\":" << (state_.loaded ? "true" : "false") << ",";
        out << "\"loading\":" << (isLoading_ ? "true" : "false") << ",";
        out << "\"canSave\":" << ((state_.loaded && !isLoading_) ? "true" : "false") << ",";
        out << "\"currentTab\":" << currentTab_ << ",";
        out << "\"currentTabTitle\":" << JsonString(CurrentTabTitle()) << ",";
        out << "\"saveName\":" << JsonString(CurrentSaveName()) << ",";
        out << "\"summary\":" << JsonString(BuildSummaryText()) << ",";
        out << "\"status\":" << JsonString(state_.status) << ",";
        out << "\"rowsVisible\":" << filteredRows_.size() << ",";
        out << "\"rowsTotal\":" << rows_.size() << ",";
        out << "\"search\":" << JsonString(searchText_) << ",";
        out << "\"selectedRowId\":";
        if (selectedRowId_.empty()) out << "null";
        else out << JsonStringUtf8(selectedRowId_);
        out << ",";
        out << "\"rows\":[";
        for (size_t i = 0; i < filteredRows_.size(); ++i) {
            const UiRow& row = rows_[filteredRows_[i]];
            if (i > 0) out << ",";
            out << "{";
            out << "\"id\":" << JsonStringUtf8(row.id) << ",";
            out << "\"label\":" << JsonString(row.selectedText) << ",";
            out << "\"line\":" << JsonString(row.line) << ",";
            out << "\"value\":" << JsonString(row.valueText) << ",";
            out << "\"type\":" << JsonString(row.typeName) << ",";
            out << "\"rawName\":" << JsonString(row.rawName) << ",";
            out << "\"note\":" << JsonString(row.noteText) << ",";
            out << "\"editable\":" << (row.editable ? "true" : "false") << ",";
            out << "\"featured\":" << (row.featured ? "true" : "false");
            out << "}";
        }
        out << "],";
        out << "\"selected\":";
        if (const UiRow* row = SelectedRow()) {
            out << "{";
            out << "\"id\":" << JsonStringUtf8(row->id) << ",";
            out << "\"label\":" << JsonString(row->selectedText) << ",";
            out << "\"line\":" << JsonString(row->line) << ",";
            out << "\"value\":" << JsonString(row->valueText) << ",";
            out << "\"type\":" << JsonString(row->typeName) << ",";
            out << "\"rawName\":" << JsonString(row->rawName) << ",";
            out << "\"note\":" << JsonString(row->noteText) << ",";
            out << "\"detail\":" << JsonString(BuildDetailText(*row)) << ",";
            out << "\"editable\":" << (row->editable ? "true" : "false");
            out << "}";
        } else {
            out << "null";
        }
        out << "}";
        return out.str();
    }

    void BeginAsyncLoad(const std::wstring& path) {
        if (isLoading_) return;

        isLoading_ = true;
        ++nextLoadToken_;
        unsigned int token = nextLoadToken_;
        state_.status = L"Loading " + path + L"...";
        PublishState();

        HWND target = hwnd_;
        std::thread([target, path, token]() {
            std::unique_ptr<AsyncLoadResult> result(new AsyncLoadResult());
            result->token = token;
            result->path = path;
            try {
                std::wstring seed = SeedForSavePath(path);
                ByteVec encrypted = ReadFileBytes(path);
                result->key = DeriveKey(seed);
                ByteVec plain = DecryptSaveBytes(encrypted, result->key);
                result->save = ParseGvas(plain);
                result->success = true;
            } catch (const std::exception& ex) {
                result->error = ex.what();
            }

            AsyncLoadResult* raw = result.release();
            if (!PostMessageW(target, WM_APP_LOAD_COMPLETE, static_cast<WPARAM>(token), reinterpret_cast<LPARAM>(raw))) {
                delete raw;
            }
        }).detach();
    }

    void FinishAsyncLoad(unsigned int token, AsyncLoadResult* rawResult) {
        std::unique_ptr<AsyncLoadResult> result(rawResult);
        if (!result || token != nextLoadToken_) return;

        isLoading_ = false;
        if (result->success) {
            state_.key = std::move(result->key);
            state_.save = std::move(result->save);
            state_.savePath = result->path;
            state_.loaded = true;
            state_.status = L"Loaded " + result->path;
            searchText_.clear();
            RefreshViews();
            return;
        }

        state_.status = L"Load failed.";
        PublishState();
        MessageBoxA(hwnd_, result->error.c_str(), "Load failed", MB_ICONERROR);
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
                row.selectedText = HumanizeIdentifier(shortName);
                row.rawName = Utf8ToWide(shortName);
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
                    row.selectedText = FriendlyItemLabel(itemName);
                    row.rawName = Utf8ToWide(itemName);
                    row.noteText = L"Changing inventory also updates the linked level when one exists.";
                    row.typeName = L"int";
                    row.valueText = std::to_wstring(std::get<std::int32_t>(amountProp->value->data));
                    row.line = row.selectedText + L" = " + row.valueText;
                    row.featured = (itemName == "moneyGold" || itemName == "moneySoul" || itemName == "itemHeroPrestige");
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
                    row.selectedText = FriendlyLevelLabel(key);
                    row.rawName = Utf8ToWide(key);
                    row.typeName = L"int";
                    row.valueText = std::to_wstring(std::get<std::int32_t>(entry.value->data));
                    std::wstring linked;
                    if (StartsWith(key, "item") && EndsWith(key, "Lvl")) {
                        Property* amountProp = FindInventoryAmountProp(state_.save, key.substr(0, key.size() - 3));
                        if (amountProp && IsIntValue(amountProp->value)) {
                            linked = L" | inventory " + std::to_wstring(std::get<std::int32_t>(amountProp->value->data));
                            row.noteText = L"Linked inventory amount: " + std::to_wstring(std::get<std::int32_t>(amountProp->value->data));
                        }
                    }
                    row.line = row.selectedText + linked + L" = " + row.valueText;
                    row.featured = (key == "itemHeroLvl");
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
                        row.selectedText = FriendlyItemLabel(itemName) + L" / " + HumanizeIdentifier(upgradeName);
                        row.rawName = Utf8ToWide(itemName) + L" / " + Utf8ToWide(upgradeName);
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
                            row.selectedText = FriendlyItemLabel(itemName) + L" / " + HumanizeIdentifier(shortName);
                            row.rawName = Utf8ToWide(itemName) + L" / " + Utf8ToWide(shortName);
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
                                row.selectedText = FriendlyItemLabel(itemName) + L" / " + HumanizeIdentifier(shortName) + L" [" + std::to_wstring(static_cast<int>(i) + 1) + L"]";
                                row.rawName = Utf8ToWide(itemName) + L" / " + Utf8ToWide(shortName) + L"[" + std::to_wstring(static_cast<int>(i)) + L"]";
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
                    row.selectedText = L"Reward " + std::to_wstring(static_cast<int>(i) + 1);
                    row.rawName = L"rewardedChallenges[" + std::to_wstring(static_cast<int>(i)) + L"]";
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
        std::string preferredId = preserveId.empty() ? selectedRowId_ : preserveId;
        bool foundPreferred = false;
        for (size_t i = 0; i < rows_.size(); ++i) {
            std::wstring haystack = rows_[i].line + L" " + rows_[i].valueText + L" " + rows_[i].rawName + L" " + rows_[i].noteText;
            if (!ContainsCaseInsensitive(haystack, searchText_)) continue;
            filteredRows_.push_back(static_cast<int>(i));
            if (!preferredId.empty() && rows_[i].id == preferredId) {
                foundPreferred = true;
            }
        }

        if (foundPreferred) {
            selectedRowId_ = preferredId;
        } else if (!filteredRows_.empty()) {
            selectedRowId_ = rows_[filteredRows_[0]].id;
        } else {
            selectedRowId_.clear();
        }
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

    bool ApplyCurrentEdit(const std::wstring& text) {
        UiRow* row = SelectedRow();
        if (!row || !row->editable) return false;
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
            PublishState();
        }
        return changed;
    }

    void RefreshViews() {
        PopulateList();
        PublishState();
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
            PublishState();
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
            action();
            state_.status = std::wstring(actionName) + L" applied. Save to write the file.";
            RefreshViews();
        } catch (const std::exception& ex) {
            MessageBoxA(hwnd_, ex.what(), "Action failed", MB_ICONERROR);
        }
    }

    void HandleWebMessage(const std::wstring& message) {
        if (message == L"ready") {
            webViewReady_ = true;
            RefreshViews();
            return;
        }
        if (message == L"open") {
            std::optional<std::wstring> path = PickOpenPath();
            if (path.has_value()) {
                BeginAsyncLoad(path.value());
            }
            return;
        }
        if (message == L"autoImport") {
            std::optional<std::wstring> path = FindLatestSave();
            if (!path.has_value()) {
                std::wstring message = L"No save found in:\n" + DefaultSaveDir();
                MessageBoxW(hwnd_, message.c_str(), L"Save not found", MB_ICONINFORMATION);
                return;
            }
            BeginAsyncLoad(path.value());
            return;
        }
        if (message == L"openFolder") {
            OpenDefaultSaveFolder();
            return;
        }
        if (message == L"save") {
            if (state_.savePath.has_value()) SaveTo(state_.savePath.value(), true);
            else {
                std::optional<std::wstring> path = PickSavePath();
                if (path.has_value()) SaveTo(path.value(), PathFileExistsW(path.value().c_str()) != FALSE);
            }
            return;
        }
        if (message == L"saveAs") {
            std::optional<std::wstring> path = PickSavePath();
            if (path.has_value()) SaveTo(path.value(), PathFileExistsW(path.value().c_str()) != FALSE);
            return;
        }
        if (message == L"action:weapon100") {
            DoBulkAction(L"Weapon levels 100", [this]() { SetAllWeaponLevels(state_.save, 100); });
            return;
        }
        if (message == L"action:spell100") {
            DoBulkAction(L"Spell levels 100", [this]() { SetAllSpellLevels(state_.save, 100); });
            return;
        }
        if (message == L"action:prestige10") {
            DoBulkAction(L"Weapon prestige 10", [this]() { EnsureWeaponPrestigeInventory(state_.save, 10); });
            return;
        }
        if (message == L"action:unlockAll") {
            DoBulkAction(L"Unlock All", [this]() { UnlockAll(state_.save); });
            return;
        }
        if (StartsWithWide(message, L"tab:")) {
            int tab = _wtoi(message.c_str() + 4);
            if (tab >= TAB_OVERVIEW && tab <= TAB_OTHER) {
                currentTab_ = tab;
                RefreshViews();
            }
            return;
        }
        if (StartsWithWide(message, L"filter:")) {
            searchText_ = UrlDecodeComponent(message.substr(7));
            RefreshViews();
            return;
        }
        if (StartsWithWide(message, L"select:")) {
            selectedRowId_ = WideToUtf8(UrlDecodeComponent(message.substr(7)));
            PublishState();
            return;
        }
        if (StartsWithWide(message, L"apply:")) {
            ApplyCurrentEdit(UrlDecodeComponent(message.substr(6)));
            return;
        }
    }

    void PaintBackground(HDC hdc) {
        RECT rect;
        GetClientRect(hwnd_, &rect);
        HBRUSH bg = CreateSolidBrush(RGB(14, 14, 20));
        FillRect(hdc, &rect, bg);
        DeleteObject(bg);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(244, 244, 248));
        DrawTextW(hdc, kWindowTitle, -1, &rect, DT_CENTER | DT_TOP | DT_SINGLELINE);
        RECT messageRect = rect;
        messageRect.top += 40;
        SetTextColor(hdc, RGB(168, 170, 184));
        DrawTextW(hdc, paintMessage_.c_str(), -1, &messageRect, DT_CENTER | DT_TOP | DT_SINGLELINE);
    }

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_CREATE:
            ApplyWindowTheme();
            InitializeWebView();
            RefreshViews();
            return 0;
        case WM_SIZE:
            ResizeWebView();
            return 0;
        case WM_APP_LOAD_COMPLETE:
            FinishAsyncLoad(static_cast<unsigned int>(wParam), reinterpret_cast<AsyncLoadResult*>(lParam));
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd_, &ps);
            PaintBackground(hdc);
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_DESTROY:
            webView_ = nullptr;
            webController_ = nullptr;
            webEnvironment_ = nullptr;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, msg, wParam, lParam);
        }
    }
};

} // namespace ffw

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

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
        MessageBoxW(NULL, L"Unable to create the main window.", L"Far Far West Unlock all tool", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }
    int code = app.Run();
    CoUninitialize();
    return code;
}
