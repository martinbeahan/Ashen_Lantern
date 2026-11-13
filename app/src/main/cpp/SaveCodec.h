#ifndef DND_SAVE_CODEC_H
#define DND_SAVE_CODEC_H

// Save / sync text safety (Code Auditor C1, H1).
//
// The save + online-sync format is a delimiter string (',' ';' '|' '~' '^' ':'). Free-text
// fields (hero / foe names, Lanternkeeper name, room text, last event, chat, journal) are
// percent-encoded on write since v2.23 (save marker "E2" at the end of the header), so a
// name like "Bob, Jr" or chat like "hi | there" can no longer shift fields. Older saves have
// no marker and are read raw, exactly as before.
//
// Also: strict, non-throwing integer parsing for save fields, and the name sanitizer used
// at name entry / remote join (mirrors TextSafety.kt — keep the two in sync).

#include <string>
#include <charconv>
#include <stdexcept>
#include <system_error>

namespace dnd {
namespace savecodec {

/** Header marker appended after the last header field (older readers ignore extra fields). */
constexpr const char* kEncodedMarker = "E2";

inline bool needsEscape(unsigned char c) {
    return c == '%' || c == ',' || c == ';' || c == '|' || c == '~' || c == '^' || c == ':'
        || c == '\n' || c == '\r' || c == '\0';
}

/** Percent-encode the delimiter set (and '%' itself). Every other byte passes through. */
inline std::string encode(const std::string& in) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in) {
        if (needsEscape(c)) {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

inline int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/** Inverse of encode(). Malformed escapes are kept literally (never throws). */
inline std::string decode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            int hi = hexVal(in[i + 1]);
            int lo = hexVal(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                char c = static_cast<char>((hi << 4) | lo);
                if (c != '\0') out.push_back(c);
                i += 2;
                continue;
            }
        }
        out.push_back(in[i]);
    }
    return out;
}

/** Thrown for a malformed numeric field; deserialize() turns it into a failed load. */
struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/** Strict int parse: optional leading/trailing spaces, whole field must be a number in range. */
inline bool tryParseInt(const std::string& s, int& out) {
    size_t b = 0, e = s.size();
    while (b < e && s[b] == ' ') ++b;
    while (e > b && s[e - 1] == ' ') --e;
    if (b == e) return false;
    const char* first = s.data() + b;
    const char* last = s.data() + e;
    if (*first == '+') ++first;
    int v = 0;
    auto res = std::from_chars(first, last, v);
    if (res.ec != std::errc() || res.ptr != last) return false;
    out = v;
    return true;
}

inline int parseInt(const std::string& s, const char* field) {
    int v = 0;
    if (!tryParseInt(s, v)) throw ParseError(std::string("bad number in ") + field + ": '" + s.substr(0, 24) + "'");
    return v;
}

inline int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/** Class ordinal 0..4 (FIGHTER..BARD); anything else is invalid. */
inline bool isValidClassId(int c) { return c >= 0 && c <= 4; }

/**
 * Player-entered / remote-join name sanitizer. Must match TextSafety.sanitizeName in Kotlin:
 *  1) control chars -> space  2) drop , ; | ~ ^ : %  3) drop "(NPC)" (reserved for companions)
 *  4) collapse spaces  5) trim spaces  6) cap at maxBytes on a UTF-8 boundary  7) fallback.
 */
inline std::string sanitizeName(const std::string& raw, const std::string& fallback = "Hero",
                                size_t maxBytes = 96) {
    std::string s;
    s.reserve(raw.size());
    for (unsigned char c : raw) {
        if (c < 0x20 || c == 0x7F) { s.push_back(' '); continue; }
        if (c == ',' || c == ';' || c == '|' || c == '~' || c == '^' || c == ':' || c == '%') continue;
        s.push_back(static_cast<char>(c));
    }
    for (size_t p; (p = s.find("(NPC)")) != std::string::npos;) s.erase(p, 5);
    std::string t;
    t.reserve(s.size());
    for (char c : s) {
        if (c == ' ' && (t.empty() || t.back() == ' ')) continue;
        t.push_back(c);
    }
    while (!t.empty() && t.back() == ' ') t.pop_back();
    if (t.size() > maxBytes) {
        size_t cut = maxBytes;
        while (cut > 0 && (static_cast<unsigned char>(t[cut]) & 0xC0) == 0x80) --cut;
        t.resize(cut);
        while (!t.empty() && t.back() == ' ') t.pop_back();
    }
    return t.empty() ? fallback : t;
}

/** Display-only strings handed to Kotlin in ',' ';' '|' lists (battle roster, party XP). */
inline std::string listSafe(const std::string& s) {
    std::string out = s;
    for (char& c : out) if (c == ',' || c == ';' || c == '|' || c == '\n') c = ' ';
    return out;
}

} // namespace savecodec
} // namespace dnd

#endif // DND_SAVE_CODEC_H
