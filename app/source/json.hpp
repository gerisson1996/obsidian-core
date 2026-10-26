#ifndef SIMPLE_JSON_HPP
#define SIMPLE_JSON_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cctype>

enum JsonType { JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT };

class JsonValue {
public:
    JsonType type = JSON_NULL;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::map<std::string, JsonValue> obj_val;

    bool is_null() const { return type == JSON_NULL; }
    bool is_bool() const { return type == JSON_BOOL; }
    bool is_number() const { return type == JSON_NUMBER; }
    bool is_string() const { return type == JSON_STRING; }
    bool is_array() const { return type == JSON_ARRAY; }
    bool is_object() const { return type == JSON_OBJECT; }

    const std::string& as_string(const std::string& def = "") const {
        return (type == JSON_STRING) ? str_val : def;
    }

    double as_number(double def = 0.0) const {
        return (type == JSON_NUMBER) ? num_val : def;
    }

    int64_t as_int64(int64_t def = 0) const {
        return (type == JSON_NUMBER) ? static_cast<int64_t>(num_val) : def;
    }

    bool as_bool(bool def = false) const {
        return (type == JSON_BOOL) ? bool_val : def;
    }

    const JsonValue& operator[](const std::string& key) const {
        static const JsonValue null_instance;
        if (type != JSON_OBJECT) return null_instance;
        auto it = obj_val.find(key);
        return (it != obj_val.end()) ? it->second : null_instance;
    }

    const JsonValue& operator[](size_t index) const {
        static const JsonValue null_instance;
        if (type != JSON_ARRAY || index >= arr_val.size()) return null_instance;
        return arr_val[index];
    }

    bool contains(const std::string& key) const {
        return type == JSON_OBJECT && obj_val.find(key) != obj_val.end();
    }

    static JsonValue parse(const std::string& text) {
        size_t idx = 0;
        skip_ws(text, idx);
        return parse_value(text, idx);
    }

private:
    static void skip_ws(const std::string& text, size_t& idx) {
        while (idx < text.size() && (std::isspace(static_cast<unsigned char>(text[idx])))) {
            idx++;
        }
    }

    static JsonValue parse_value(const std::string& text, size_t& idx) {
        skip_ws(text, idx);
        if (idx >= text.size()) return JsonValue();

        char c = text[idx];
        if (c == '{') return parse_object(text, idx);
        if (c == '[') return parse_array(text, idx);
        if (c == '"') return parse_string(text, idx);
        if (c == 't' || c == 'f') return parse_bool(text, idx);
        if (c == 'n') return parse_null(text, idx);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number(text, idx);

        return JsonValue();
    }

    static JsonValue parse_object(const std::string& text, size_t& idx) {
        JsonValue val;
        val.type = JSON_OBJECT;
        idx++; // skip '{'

        while (idx < text.size()) {
            skip_ws(text, idx);
            if (idx < text.size() && text[idx] == '}') {
                idx++;
                return val;
            }

            JsonValue key_val = parse_string(text, idx);
            skip_ws(text, idx);
            if (idx >= text.size() || text[idx] != ':') break;
            idx++; // skip ':'

            JsonValue item = parse_value(text, idx);
            val.obj_val[key_val.str_val] = item;

            skip_ws(text, idx);
            if (idx < text.size() && text[idx] == ',') {
                idx++;
            } else if (idx < text.size() && text[idx] == '}') {
                idx++;
                return val;
            } else {
                break;
            }
        }
        return val;
    }

    static JsonValue parse_array(const std::string& text, size_t& idx) {
        JsonValue val;
        val.type = JSON_ARRAY;
        idx++; // skip '['

        while (idx < text.size()) {
            skip_ws(text, idx);
            if (idx < text.size() && text[idx] == ']') {
                idx++;
                return val;
            }

            JsonValue item = parse_value(text, idx);
            val.arr_val.push_back(item);

            skip_ws(text, idx);
            if (idx < text.size() && text[idx] == ',') {
                idx++;
            } else if (idx < text.size() && text[idx] == ']') {
                idx++;
                return val;
            } else {
                break;
            }
        }
        return val;
    }

    static JsonValue parse_string(const std::string& text, size_t& idx) {
        JsonValue val;
        val.type = JSON_STRING;
        if (idx >= text.size() || text[idx] != '"') return val;
        idx++; // skip opening '"'

        std::string s;
        while (idx < text.size()) {
            char c = text[idx++];
            if (c == '"') {
                val.str_val = s;
                return val;
            }
            if (c == '\\' && idx < text.size()) {
                char esc = text[idx++];
                if (esc == '"') s += '"';
                else if (esc == '\\') s += '\\';
                else if (esc == '/') s += '/';
                else if (esc == 'n') s += '\n';
                else if (esc == 'r') s += '\r';
                else if (esc == 't') s += '\t';
                else s += esc;
            } else {
                s += c;
            }
        }
        val.str_val = s;
        return val;
    }

    static JsonValue parse_number(const std::string& text, size_t& idx) {
        JsonValue val;
        val.type = JSON_NUMBER;
        size_t start = idx;
        if (text[idx] == '-') idx++;
        while (idx < text.size() && (std::isdigit(static_cast<unsigned char>(text[idx])) || text[idx] == '.' || text[idx] == 'e' || text[idx] == 'E' || text[idx] == '+' || text[idx] == '-')) {
            idx++;
        }
        std::string num_str = text.substr(start, idx - start);
        val.num_val = std::strtod(num_str.c_str(), nullptr);
        return val;
    }

    static JsonValue parse_bool(const std::string& text, size_t& idx) {
        JsonValue val;
        val.type = JSON_BOOL;
        if (text.compare(idx, 4, "true") == 0) {
            val.bool_val = true;
            idx += 4;
        } else if (text.compare(idx, 5, "false") == 0) {
            val.bool_val = false;
            idx += 5;
        }
        return val;
    }

    static JsonValue parse_null(const std::string& text, size_t& idx) {
        JsonValue val;
        val.type = JSON_NULL;
        if (text.compare(idx, 4, "null") == 0) {
            idx += 4;
        }
        return val;
    }
};

#endif // SIMPLE_JSON_HPP
