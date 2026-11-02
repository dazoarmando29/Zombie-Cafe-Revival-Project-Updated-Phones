/* Companion save logic + JSON. See companion_state.h. */
#include "companion_state.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace zcr_comp {
namespace {

/* ---------------- minimal JSON reader ----------------
 * Recursive-descent parser for the controlled schema of the companion
 * save and the achievement definitions (objects / arrays / strings /
 * integers / true / false / null). Numbers are read as int64 (the schema
 * never stores fractions). */

struct JsonVal;
using JsonObj = std::map<std::string, JsonVal>;
using JsonArr = std::vector<JsonVal>;

struct JsonVal {
    enum Type { Null, Bool, Int, Str, Arr, Obj } type = Null;
    bool b = false;
    int64_t i = 0;
    std::string s;
    JsonArr a;
    JsonObj o;

    const JsonVal *find(const std::string &k) const {
        if (type != Obj) return nullptr;
        auto it = o.find(k);
        return it == o.end() ? nullptr : &it->second;
    }
    std::string str_or(const std::string &k, const std::string &dflt) const {
        const JsonVal *v = find(k);
        return (v && v->type == Str) ? v->s : dflt;
    }
    int64_t int_or(const std::string &k, int64_t dflt) const {
        const JsonVal *v = find(k);
        return (v && v->type == Int) ? v->i : dflt;
    }
    bool bool_or(const std::string &k, bool dflt) const {
        const JsonVal *v = find(k);
        if (!v) return dflt;
        if (v->type == Bool) return v->b;
        if (v->type == Int) return v->i != 0; /* accept 0/1 too */
        return dflt;
    }
};

struct JsonParser {
    const std::string &in;
    size_t pos = 0;
    std::string err;

    explicit JsonParser(const std::string &s) : in(s) {}

    void skip_ws() {
        while (pos < in.size() &&
               (in[pos] == ' ' || in[pos] == '\t' || in[pos] == '\n' ||
                in[pos] == '\r'))
            pos++;
    }
    bool fail(const std::string &m) {
        char buf[64];
        snprintf(buf, sizeof(buf), " at offset %zu", pos);
        err = m + buf;
        return false;
    }
    bool lit(const char *s) {
        size_t n = strlen(s);
        if (in.compare(pos, n, s) != 0) return false;
        pos += n;
        return true;
    }
    bool parse_string(std::string *out) {
        if (pos >= in.size() || in[pos] != '"') return fail("expected string");
        pos++;
        out->clear();
        while (pos < in.size()) {
            char c = in[pos];
            if (c == '"') {
                pos++;
                return true;
            }
            if (c == '\\') {
                pos++;
                if (pos >= in.size()) return fail("bad escape");
                char e = in[pos++];
                switch (e) {
                    case '"': *out += '"'; break;
                    case '\\': *out += '\\'; break;
                    case '/': *out += '/'; break;
                    case 'b': *out += '\b'; break;
                    case 'f': *out += '\f'; break;
                    case 'n': *out += '\n'; break;
                    case 'r': *out += '\r'; break;
                    case 't': *out += '\t'; break;
                    case 'u': {
                        if (pos + 4 > in.size()) return fail("bad \\u");
                        unsigned cp = 0;
                        for (int k = 0; k < 4; k++) {
                            char h = in[pos++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                            else return fail("bad hex in \\u");
                        }
                        if (cp < 0x80) {
                            *out += (char)cp;
                        } else if (cp < 0x800) {
                            *out += (char)(0xC0 | (cp >> 6));
                            *out += (char)(0x80 | (cp & 0x3F));
                        } else {
                            *out += (char)(0xE0 | (cp >> 12));
                            *out += (char)(0x80 | ((cp >> 6) & 0x3F));
                            *out += (char)(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default:
                        return fail("bad escape char");
                }
            } else {
                *out += c;
                pos++;
            }
        }
        return fail("unterminated string");
    }
    bool parse_value(JsonVal *out) {
        skip_ws();
        if (pos >= in.size()) return fail("unexpected end");
        char c = in[pos];
        if (c == '{') {
            pos++;
            out->type = JsonVal::Obj;
            skip_ws();
            if (pos < in.size() && in[pos] == '}') {
                pos++;
                return true;
            }
            for (;;) {
                skip_ws();
                std::string key;
                if (!parse_string(&key)) return false;
                skip_ws();
                if (pos >= in.size() || in[pos] != ':') return fail("expected ':'");
                pos++;
                JsonVal v;
                if (!parse_value(&v)) return false;
                out->o[key] = v;
                skip_ws();
                if (pos < in.size() && in[pos] == ',') {
                    pos++;
                    continue;
                }
                if (pos < in.size() && in[pos] == '}') {
                    pos++;
                    return true;
                }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            pos++;
            out->type = JsonVal::Arr;
            skip_ws();
            if (pos < in.size() && in[pos] == ']') {
                pos++;
                return true;
            }
            for (;;) {
                JsonVal v;
                if (!parse_value(&v)) return false;
                out->a.push_back(v);
                skip_ws();
                if (pos < in.size() && in[pos] == ',') {
                    pos++;
                    continue;
                }
                if (pos < in.size() && in[pos] == ']') {
                    pos++;
                    return true;
                }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') {
            out->type = JsonVal::Str;
            return parse_string(&out->s);
        }
        if (c == 't') {
            if (!lit("true")) return fail("bad literal");
            out->type = JsonVal::Bool;
            out->b = true;
            return true;
        }
        if (c == 'f') {
            if (!lit("false")) return fail("bad literal");
            out->type = JsonVal::Bool;
            return true;
        }
        if (c == 'n') {
            if (!lit("null")) return fail("bad literal");
            out->type = JsonVal::Null;
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            size_t start = pos;
            if (in[pos] == '-') pos++;
            while (pos < in.size() && in[pos] >= '0' && in[pos] <= '9') pos++;
            if (pos < in.size() && (in[pos] == '.' || in[pos] == 'e' ||
                                   in[pos] == 'E'))
                return fail("fractional numbers not supported");
            out->type = JsonVal::Int;
            out->i = strtoll(in.substr(start, pos - start).c_str(), nullptr, 10);
            return true;
        }
        return fail("unexpected character");
    }
    bool parse(JsonVal *out) {
        if (!parse_value(out)) return false;
        skip_ws();
        if (pos != in.size()) return fail("trailing data");
        return true;
    }
};

/* ---------------- minimal JSON writer ---------------- */

void json_escape(const std::string &s, std::string *out) {
    *out += '"';
    for (char c : s) {
        switch (c) {
            case '"': *out += "\\\""; break;
            case '\\': *out += "\\\\"; break;
            case '\b': *out += "\\b"; break;
            case '\f': *out += "\\f"; break;
            case '\n': *out += "\\n"; break;
            case '\r': *out += "\\r"; break;
            case '\t': *out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)(unsigned char)c);
                    *out += buf;
                } else {
                    *out += c;
                }
        }
    }
    *out += '"';
}

void write_int(const char *key, int64_t v, std::string *out, bool *first) {
    if (!*first) *out += ",";
    *first = false;
    *out += '"';
    *out += key;
    *out += "\":";
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)v);
    *out += buf;
}

void write_str(const char *key, const std::string &v, std::string *out,
               bool *first) {
    if (!*first) *out += ",";
    *first = false;
    *out += '"';
    *out += key;
    *out += "\":";
    json_escape(v, out);
}

void write_bool(const char *key, bool v, std::string *out, bool *first) {
    if (!*first) *out += ",";
    *first = false;
    *out += '"';
    *out += key;
    *out += "\":";
    *out += v ? "true" : "false";
}

/* ---------------- helpers ---------------- */

bool parse_type(const std::string &s, AchType *out) {
    if (s == "cook_total") { *out = AchType::CookTotal; return true; }
    if (s == "cook_recipe") { *out = AchType::CookRecipe; return true; }
    if (s == "distinct_recipes") { *out = AchType::DistinctRecipes; return true; }
    if (s == "mastery_recipe") { *out = AchType::MasteryRecipe; return true; }
    if (s == "mastery_count") { *out = AchType::MasteryCount; return true; }
    if (s == "toxin_spent") { *out = AchType::ToxinSpent; return true; }
    if (s == "toxin_earned") { *out = AchType::ToxinEarned; return true; }
    return false;
}

int64_t recipe_count(const State &s, const std::string &key) {
    auto it = s.recipes.find(key);
    return it == s.recipes.end() ? 0 : it->second.cooked;
}

/* Progress for one achievement against the current state. */
int64_t progress_of(const State &s, const Defs &d, const AchDef &def) {
    switch (def.type) {
        case AchType::CookTotal:
            return s.total_cooked;
        case AchType::CookRecipe:
            return recipe_count(s, def.param);
        case AchType::DistinctRecipes: {
            int64_t n = 0;
            for (const auto &kv : s.recipes)
                if (kv.second.cooked > 0) n++;
            return n;
        }
        case AchType::MasteryRecipe:
            return mastery_level(d, recipe_count(s, def.param));
        case AchType::MasteryCount: {
            int64_t n = 0;
            for (const auto &kv : s.recipes)
                if (mastery_level(d, kv.second.cooked) >= def.param2) n++;
            return n;
        }
        case AchType::ToxinSpent:
            return s.toxin_spent;
        case AchType::ToxinEarned:
            return s.toxin_earned;
    }
    return 0;
}

}  // namespace

int mastery_level(const Defs &d, int64_t cooked) {
    int level = 0;
    for (int64_t t : d.mastery_levels) {
        if (cooked >= t) level++;
    }
    return level;
}

std::string recipe_key(const std::string &zcr_id, int32_t row) {
    if (!zcr_id.empty()) return zcr_id;
    char buf[24];
    snprintf(buf, sizeof(buf), "row:%d", (int)row);
    return buf;
}

std::vector<std::string> evaluate(State &s, const Defs &d, int64_t now) {
    std::vector<std::string> unlocked_now;
    for (const AchDef &def : d.achievements) {
        AchState &st = s.achievements[def.id]; /* creates if missing */
        if (st.unlocked) continue;
        int64_t p = progress_of(s, d, def);
        if (p > st.progress) st.progress = p;
        if (st.progress >= def.threshold) {
            st.unlocked = true;
            st.unlocked_ts = now;
            unlocked_now.push_back(def.id);
        }
    }
    return unlocked_now;
}

std::vector<std::string> on_dish_cooked(State &s, const Defs &d,
                                        const std::string &key, int32_t row,
                                        int64_t now) {
    RecipeStat &r = s.recipes[key];
    if (r.cooked == 0) r.first_ts = now;
    r.cooked++;
    r.last_ts = now;
    if (r.zcr_id.empty() && key.rfind("row:", 0) != 0) r.zcr_id = key;
    if (r.row < 0) r.row = row;
    s.total_cooked++;
    if (row >= 0) s.last_cooked_row = row;
    s.updated_ts = now;
    return evaluate(s, d, now);
}

std::vector<std::string> on_toxin_spent(State &s, const Defs &d, int64_t amount,
                                        int64_t now, const std::string &flow) {
    if (amount <= 0) return {};
    s.toxin_spent += amount;
    SpendEvent ev;
    ev.ts = now;
    ev.amount = (int32_t)amount;
    ev.flow = flow.empty() ? "unknown" : flow;
    s.toxin_log.push_back(ev);
    if (s.toxin_log.size() > kToxinLogCap)
        s.toxin_log.erase(s.toxin_log.begin(),
                          s.toxin_log.end() - (ptrdiff_t)kToxinLogCap);
    s.updated_ts = now;
    return evaluate(s, d, now);
}

std::vector<std::string> on_toxin_earned(State &s, const Defs &d,
                                         int64_t amount, int64_t now) {
    if (amount <= 0) return {};
    s.toxin_earned += amount;
    s.updated_ts = now;
    return evaluate(s, d, now);
}

void set_favorite(State &s, const std::string &key, int32_t row, bool fav,
                  int64_t now) {
    if (fav) {
        s.favorites[key] = row;
    } else {
        s.favorites.erase(key);
    }
    s.updated_ts = now;
}

void mark_claimed(State &s, const std::string &id, int64_t now) {
    AchState &st = s.achievements[id];
    st.claimed = true;
    st.claimed_ts = now;
    s.updated_ts = now;
}

int32_t achievement_reward(const Defs &d, const std::string &id) {
    for (const AchDef &def : d.achievements)
        if (def.id == id) return def.reward_toxin;
    return 0;
}

std::string serialize(const State &s) {
    std::string out;
    out.reserve(1024);
    out += "{";
    bool f = true;
    write_int("version", s.version, &out, &f);
    write_int("created", s.created_ts, &out, &f);
    write_int("updated", s.updated_ts, &out, &f);
    /* totals */
    if (!f) out += ",";
    f = false;
    out += "\"totals\":{";
    bool tf = true;
    write_int("cooked", s.total_cooked, &out, &tf);
    write_int("toxin_spent", s.toxin_spent, &out, &tf);
    write_int("toxin_earned", s.toxin_earned, &out, &tf);
    write_int("last_cooked_row", s.last_cooked_row, &out, &tf);
    out += "}";
    /* per-recipe cook add-on (row -> variant) */
    out += ",\"cookVariant\":{";
    bool vf = true;
    for (const auto &kv : s.cook_variant) {
        if (!vf) out += ",";
        vf = false;
        char kbuf[16];
        snprintf(kbuf, sizeof(kbuf), "\"%d\":", (int)kv.first);
        out += kbuf;
        char vbuf[16];
        snprintf(vbuf, sizeof(vbuf), "%d", (int)kv.second);
        out += vbuf;
    }
    out += "}";
    /* recipes (std::map = sorted keys = deterministic) */
    out += ",\"recipes\":{";
    bool rf = true;
    for (const auto &kv : s.recipes) {
        if (!rf) out += ",";
        rf = false;
        json_escape(kv.first, &out);
        out += ":{";
        bool ef = true;
        write_str("zcr", kv.second.zcr_id, &out, &ef);
        write_int("row", kv.second.row, &out, &ef);
        write_int("cooked", kv.second.cooked, &out, &ef);
        write_int("first", kv.second.first_ts, &out, &ef);
        write_int("last", kv.second.last_ts, &out, &ef);
        out += "}";
    }
    out += "}";
    /* favorites (key -> foodData row; presence = starred) */
    out += ",\"favorites\":{";
    bool ff = true;
    for (const auto &kv : s.favorites) {
        if (!ff) out += ",";
        ff = false;
        json_escape(kv.first, &out);
        out += ":";
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", (int)kv.second);
        out += buf;
    }
    out += "}";
    /* toxin spend ledger (newest last) */
    out += ",\"toxin_log\":[";
    for (size_t i = 0; i < s.toxin_log.size(); i++) {
        const SpendEvent &e = s.toxin_log[i];
        if (i) out += ",";
        out += "{\"t\":";
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", (long long)e.ts);
        out += buf;
        out += ",\"n\":";
        snprintf(buf, sizeof(buf), "%d", (int)e.amount);
        out += buf;
        out += ",\"f\":";
        json_escape(e.flow, &out);
        out += "}";
    }
    out += "]";
    /* achievements */
    out += ",\"achievements\":{";
    bool af = true;
    for (const auto &kv : s.achievements) {
        if (!af) out += ",";
        af = false;
        json_escape(kv.first, &out);
        out += ":{";
        bool ef = true;
        write_int("progress", kv.second.progress, &out, &ef);
        write_bool("unlocked", kv.second.unlocked, &out, &ef);
        write_int("unlocked_ts", kv.second.unlocked_ts, &out, &ef);
        write_bool("claimed", kv.second.claimed, &out, &ef);
        write_int("claimed_ts", kv.second.claimed_ts, &out, &ef);
        out += "}";
    }
    out += "}";
    out += "}";
    return out;
}

bool parse_state(const std::string &json, State *out, std::string *err) {
    JsonParser p(json);
    JsonVal root;
    if (!p.parse(&root)) {
        if (err) *err = p.err;
        return false;
    }
    if (root.type != JsonVal::Obj) {
        if (err) *err = "root is not an object";
        return false;
    }
    State st;
    st.version = (int32_t)root.int_or("version", 1);
    st.created_ts = root.int_or("created", 0);
    st.updated_ts = root.int_or("updated", 0);
    if (const JsonVal *t = root.find("totals")) {
        st.total_cooked = t->int_or("cooked", 0);
        st.toxin_spent = t->int_or("toxin_spent", 0);
        st.toxin_earned = t->int_or("toxin_earned", 0);
        st.last_cooked_row = (int32_t)t->int_or("last_cooked_row", -1);
    }
    if (const JsonVal *r = root.find("recipes")) {
        for (const auto &kv : r->o) {
            RecipeStat rs;
            rs.zcr_id = kv.second.str_or("zcr", "");
            rs.row = (int32_t)kv.second.int_or("row", -1);
            rs.cooked = kv.second.int_or("cooked", 0);
            rs.first_ts = kv.second.int_or("first", 0);
            rs.last_ts = kv.second.int_or("last", 0);
            if (rs.cooked < 0) rs.cooked = 0; /* monotonic guard */
            st.recipes[kv.first] = rs;
        }
    }
    if (const JsonVal *fv = root.find("favorites")) {
        for (const auto &kv : fv->o) {
            if (kv.second.type != JsonVal::Int) continue;
            if (kv.second.i < 0 || kv.second.i > 255) continue;
            st.favorites[kv.first] = (int32_t)kv.second.i;
        }
    }
    if (const JsonVal *cv = root.find("cookVariant")) {
        for (const auto &kv : cv->o) {
            if (kv.second.type != JsonVal::Int) continue;
            int32_t row = (int32_t)atoi(kv.first.c_str());
            if (row < 0 || row >= 250) continue;
            if (kv.second.i < 0 || kv.second.i > 255) continue;
            st.cook_variant[row] = (int32_t)kv.second.i;
        }
    }
    if (const JsonVal *tl = root.find("toxin_log")) {
        if (tl->type == JsonVal::Arr) {
            for (const JsonVal &e : tl->a) {
                if (e.type != JsonVal::Obj) continue;
                SpendEvent ev;
                ev.ts = e.int_or("t", 0);
                ev.amount = (int32_t)e.int_or("n", 0);
                ev.flow = e.str_or("f", "unknown");
                if (ev.amount <= 0) continue;
                st.toxin_log.push_back(ev);
            }
            /* cap: keep the most recent */
            if (st.toxin_log.size() > kToxinLogCap)
                st.toxin_log.erase(st.toxin_log.begin(),
                                   st.toxin_log.end() - (ptrdiff_t)kToxinLogCap);
        }
    }
    if (const JsonVal *a = root.find("achievements")) {
        for (const auto &kv : a->o) {
            AchState as;
            as.progress = kv.second.int_or("progress", 0);
            if (as.progress < 0) as.progress = 0; /* tamper guard */
            as.unlocked = kv.second.bool_or("unlocked", false);
            as.unlocked_ts = kv.second.int_or("unlocked_ts", 0);
            as.claimed = kv.second.bool_or("claimed", false);
            as.claimed_ts = kv.second.int_or("claimed_ts", 0);
            if (as.unlocked && !as.claimed) {
                /* persisted claim state wins: keep unlocked-but-unclaimed so
                 * the reward path can complete (persist-then-grant order
                 * means a crash between the two leaves it unclaimed here). */
            }
            st.achievements[kv.first] = as;
        }
    }
    /* consistency: totals never lower than the per-recipe sum */
    int64_t sum = 0;
    for (const auto &kv : st.recipes) sum += kv.second.cooked;
    if (st.total_cooked < sum) st.total_cooked = sum;
    *out = std::move(st);
    return true;
}

bool parse_defs(const std::string &json, Defs *out, std::string *err) {
    JsonParser p(json);
    JsonVal root;
    if (!p.parse(&root)) {
        if (err) *err = p.err;
        return false;
    }
    if (root.type != JsonVal::Obj) {
        if (err) *err = "root is not an object";
        return false;
    }
    Defs d;
    if (const JsonVal *m = root.find("mastery")) {
        if (const JsonVal *lv = m->find("levels")) {
            for (const JsonVal &v : lv->a)
                if (v.type == JsonVal::Int && v.i > 0) d.mastery_levels.push_back(v.i);
        }
    }
    if (d.mastery_levels.empty()) {
        d.mastery_levels = {10, 50, 200};
    }
    /* ascending, unique */
    std::vector<int64_t> lv = d.mastery_levels;
    std::sort(lv.begin(), lv.end());
    lv.erase(std::unique(lv.begin(), lv.end()), lv.end());
    d.mastery_levels = lv;

    if (const JsonVal *list = root.find("achievements")) {
        for (const JsonVal &v : list->a) {
            if (v.type != JsonVal::Obj) continue;
            AchDef def;
            def.id = v.str_or("id", "");
            if (def.id.empty()) {
                if (err) *err = "achievement without id";
                return false;
            }
            def.title = v.str_or("title", def.id);
            def.desc = v.str_or("desc", "");
            def.param = v.str_or("param", "");
            def.param2 = (int32_t)v.int_or("param2", 0);
            def.threshold = v.int_or("threshold", 0);
            def.reward_toxin = (int32_t)v.int_or("reward_toxin", 0);
            std::string ty = v.str_or("type", "cook_total");
            if (!parse_type(ty, &def.type)) {
                if (err) *err = "unknown achievement type: " + ty;
                return false;
            }
            if (def.reward_toxin < 0) def.reward_toxin = 0;
            d.achievements.push_back(def);
        }
    }
    *out = std::move(d);
    return true;
}

bool parse_registry_rows(const std::string &json, const std::string &file,
                         std::map<int32_t, std::string> *row_to_id,
                         std::string *err) {
    JsonParser p(json);
    JsonVal root;
    if (!p.parse(&root)) {
        if (err) *err = p.err;
        return false;
    }
    const JsonVal *entries = root.find("entries");
    if (!entries || entries->type != JsonVal::Arr) {
        if (err) *err = "no entries array";
        return false;
    }
    for (const JsonVal &e : entries->a) {
        if (e.type != JsonVal::Obj) continue;
        if (e.str_or("file", "") != file) continue;
        const JsonVal *row = e.find("row");
        if (!row || row->type != JsonVal::Int) continue;
        std::string id = e.str_or("id", "");
        if (id.empty()) continue;
        (*row_to_id)[(int32_t)row->i] = id;
    }
    return true;
}

}  // namespace zcr_comp
