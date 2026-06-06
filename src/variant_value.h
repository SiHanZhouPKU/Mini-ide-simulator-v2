#ifndef VARIANT_VALUE_H
#define VARIANT_VALUE_H

#include <string>
#include <variant>
#include <vector>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <list>
#include <set>
#include <deque>

// Forward declarations for object model
struct RuntimeClass;
struct RuntimeObject;

// --- Runtime types ---

using NullPtr = std::nullptr_t;
using IntList = std::vector<int>;
using FloatList = std::vector<float>;
using DoubleList = std::vector<double>;
using CharList = std::vector<char>;
using StringList = std::vector<std::string>;
using IntSet = std::set<int>;
using StringIntMap = std::map<std::string, int>;

struct RuntimeObject {
    std::string className;
    std::shared_ptr<RuntimeClass> classDef;
    std::map<std::string, struct VariantValue> members;
    bool destroyed = false;
};

struct VariantValue {
    enum Type {
        INT, FLOAT, DOUBLE, CHAR, BOOL, STRING, VOID,
        VECTOR_INT, VECTOR_FLOAT, VECTOR_DOUBLE, VECTOR_CHAR, VECTOR_BOOL, VECTOR_STRING,
        LIST_INT, LIST_FLOAT, LIST_DOUBLE, LIST_CHAR, LIST_BOOL, LIST_STRING,
        SET_INT,
        MAP_STRING_INT,
        VECTOR_OBJECT,
        OBJECT, POINTER, NULL_PTR
    };

    Type type;
    std::string elementClassName; // declared type for containers (e.g., "Person" for vector<Person>)
    std::variant<
        int, float, double, char, bool, std::string, std::nullptr_t,
        std::vector<int>, std::vector<float>, std::vector<double>,
        std::vector<char>, std::vector<bool>, std::vector<std::string>,
        std::list<int>, std::list<float>, std::list<double>,
        std::list<char>, std::list<bool>, std::list<std::string>,
        std::set<int>, std::map<std::string, int>,
        std::vector<std::shared_ptr<RuntimeObject>>,
        std::shared_ptr<RuntimeObject>
    > value;

    // --- Constructors ---
    VariantValue() : type(INT), value(0) {}

    VariantValue(int v)                        : type(INT),           value(v) {}
    VariantValue(float v)                      : type(FLOAT),         value(v) {}
    VariantValue(double v)                     : type(DOUBLE),        value(v) {}
    VariantValue(char v)                       : type(CHAR),          value(v) {}
    VariantValue(bool v)                       : type(BOOL),          value(v) {}
    VariantValue(const std::string& v)         : type(STRING),        value(v) {}
    VariantValue(std::nullptr_t)               : type(NULL_PTR),      value(nullptr) {}
    VariantValue(const std::vector<int>& v)    : type(VECTOR_INT),    value(v) {}
    VariantValue(const std::vector<float>& v)  : type(VECTOR_FLOAT),  value(v) {}
    VariantValue(const std::vector<double>& v) : type(VECTOR_DOUBLE), value(v) {}
    VariantValue(const std::vector<char>& v)   : type(VECTOR_CHAR),   value(v) {}
    VariantValue(const std::vector<bool>& v)   : type(VECTOR_BOOL),   value(v) {}
    VariantValue(const std::vector<std::string>& v) : type(VECTOR_STRING), value(v) {}
    VariantValue(const std::list<int>& v)      : type(LIST_INT),      value(v) {}
    VariantValue(const std::list<std::string>& v) : type(LIST_STRING), value(v) {}
    VariantValue(const std::set<int>& v)       : type(SET_INT),       value(v) {}
    VariantValue(const std::map<std::string, int>& v) : type(MAP_STRING_INT), value(v) {}
    VariantValue(const std::vector<std::shared_ptr<RuntimeObject>>& v) : type(VECTOR_OBJECT), value(v) {}
    VariantValue(std::shared_ptr<RuntimeObject> obj) : type(OBJECT), value(obj) {}

    // Type name string
    std::string typeName() const {
        switch (type) {
            case INT:        return "int";
            case FLOAT:      return "float";
            case DOUBLE:     return "double";
            case CHAR:       return "char";
            case BOOL:       return "bool";
            case STRING:     return "string";
            case VOID:       return "void";
            case VECTOR_INT:    return "vector<int>";
            case VECTOR_FLOAT:  return "vector<float>";
            case VECTOR_DOUBLE: return "vector<double>";
            case VECTOR_CHAR:   return "vector<char>";
            case VECTOR_BOOL:   return "vector<bool>";
            case VECTOR_STRING: return "vector<string>";
            case LIST_INT:      return "list<int>";
            case LIST_FLOAT:    return "list<float>";
            case LIST_DOUBLE:   return "list<double>";
            case LIST_CHAR:     return "list<char>";
            case LIST_BOOL:     return "list<bool>";
            case LIST_STRING:   return "list<string>";
            case SET_INT:       return "set<int>";
            case MAP_STRING_INT: return "map<string,int>";
            case VECTOR_OBJECT:
                if (!elementClassName.empty()) return "vector<" + elementClassName + ">";
                else {
                    const auto& v = std::get<std::vector<std::shared_ptr<RuntimeObject>>>(value);
                    if (v.empty()) return "vector<object>";
                    return "vector<" + v[0]->className + ">";
                }
            case OBJECT: {
                auto obj = std::get<std::shared_ptr<RuntimeObject>>(value);
                return obj ? obj->className : "object(null)";
            }
            case POINTER:      return "pointer";
            case NULL_PTR:     return "nullptr";
        }
        return "?";
    }

    /// Generic toString — used for display in variable table
    std::string toString() const {
        switch (type) {
        case INT:    return std::to_string(std::get<int>(value));
        case FLOAT:  return std::to_string(std::get<float>(value));
        case DOUBLE: return std::to_string(std::get<double>(value));
        case CHAR: {
            char c = std::get<char>(value);
            if (c == '\0') return "'\\0'";
            return std::string("'") + c + "'";
        }
        case BOOL:   return std::get<bool>(value) ? "true" : "false";
        case STRING: return std::get<std::string>(value);
        case VOID:   return "void";
        case NULL_PTR: return "nullptr";
        case VECTOR_INT: {
            const auto& v = std::get<std::vector<int>>(value);
            if (v.empty()) return "{}";
            std::string s = "{";
            for (size_t i = 0; i < v.size(); ++i) { if (i) s += ", "; s += std::to_string(v[i]); }
            return s + "}";
        }
        case VECTOR_FLOAT: {
            const auto& v = std::get<std::vector<float>>(value);
            if (v.empty()) return "{}";
            std::string s = "{";
            for (size_t i = 0; i < v.size(); ++i) { if (i) s += ", "; s += std::to_string(v[i]); }
            return s + "}";
        }
        case VECTOR_DOUBLE: {
            const auto& v = std::get<std::vector<double>>(value);
            if (v.empty()) return "{}";
            std::string s = "{";
            for (size_t i = 0; i < v.size(); ++i) { if (i) s += ", "; s += std::to_string(v[i]); }
            return s + "}";
        }
        case VECTOR_CHAR: {
            const auto& v = std::get<std::vector<char>>(value);
            if (v.empty()) return "{}";
            std::string s = "{";
            for (size_t i = 0; i < v.size(); ++i) { if (i) s += ", "; s += std::string("'") + v[i] + "'"; }
            return s + "}";
        }
        case VECTOR_BOOL: {
            const auto& v = std::get<std::vector<bool>>(value);
            if (v.empty()) return "{}";
            std::string s = "{";
            for (size_t i = 0; i < v.size(); ++i) { if (i) s += ", "; s += v[i] ? "true" : "false"; }
            return s + "}";
        }
        case VECTOR_STRING: {
            const auto& v = std::get<std::vector<std::string>>(value);
            if (v.empty()) return "{}";
            std::string s = "{";
            for (size_t i = 0; i < v.size(); ++i) { if (i) s += ", "; s += "\"" + v[i] + "\""; }
            return s + "}";
        }
        case LIST_INT: {
            const auto& v = std::get<std::list<int>>(value);
            if (v.empty()) return "[]";
            std::string s = "[";
            bool first = true;
            for (int x : v) { if (!first) s += ", "; first = false; s += std::to_string(x); }
            return s + "]";
        }
        case LIST_STRING: {
            const auto& v = std::get<std::list<std::string>>(value);
            if (v.empty()) return "[]";
            std::string s = "[";
            bool first = true;
            for (auto& x : v) { if (!first) s += ", "; first = false; s += "\"" + x + "\""; }
            return s + "]";
        }
        case SET_INT: {
            const auto& v = std::get<std::set<int>>(value);
            if (v.empty()) return "{}";
            std::string s = "{";
            bool first = true;
            for (int x : v) { if (!first) s += ", "; first = false; s += std::to_string(x); }
            return s + "}";
        }
        case MAP_STRING_INT: {
            const auto& m = std::get<std::map<std::string, int>>(value);
            if (m.empty()) return "{}";
            std::string s = "{";
            bool first = true;
            for (auto& kv : m) {
                if (!first) s += ", ";
                first = false;
                s += "\"" + kv.first + "\": " + std::to_string(kv.second);
            }
            return s + "}";
        }
        case VECTOR_OBJECT: {
            const auto& v = std::get<std::vector<std::shared_ptr<RuntimeObject>>>(value);
            if (v.empty()) return "[]";
            std::string s = "[";
            for (size_t i = 0; i < v.size(); ++i) {
                if (i) s += ", ";
                if (v[i]) {
                    s += v[i]->className + "{";
                    bool first = true;
                    for (auto& m : v[i]->members) {
                        if (!first) s += ", ";
                        first = false;
                        s += m.first + "=" + m.second.toString();
                    }
                    s += "}";
                } else s += "nullptr";
            }
            return s + "]";
        }
        case OBJECT: {
            auto obj = std::get<std::shared_ptr<RuntimeObject>>(value);
            if (!obj) return "null";
            std::string s = obj->className + "{";
            bool first = true;
            for (auto& m : obj->members) {
                if (!first) s += ", ";
                first = false;
                s += m.first + "=" + m.second.toString();
            }
            return s + "}";
        }
        case POINTER: {
            // Pointer re-uses OBJECT variant to point to an object
            auto obj = std::get<std::shared_ptr<RuntimeObject>>(value);
            if (!obj) return "nullptr";
            return "&" + obj->className + "{...}";
        }
        }
        return "?";
    }

    /// Get "memory size" for the educational memory view
    int byteSize() const {
        switch (type) {
        case INT:    return 4;
        case FLOAT:  return 4;
        case DOUBLE: return 8;
        case CHAR:   return 1;
        case BOOL:   return 1;
        case STRING: return static_cast<int>(std::get<std::string>(value).size()) + 1;
        case VOID:   return 0;
        case NULL_PTR: return 8;
        case VECTOR_INT: return static_cast<int>(std::get<std::vector<int>>(value).size()) * 4 + 16;
        case VECTOR_FLOAT: return static_cast<int>(std::get<std::vector<float>>(value).size()) * 4 + 16;
        case VECTOR_DOUBLE: return static_cast<int>(std::get<std::vector<double>>(value).size()) * 8 + 16;
        case VECTOR_CHAR: return static_cast<int>(std::get<std::vector<char>>(value).size()) + 16;
        case VECTOR_BOOL: return static_cast<int>(std::get<std::vector<bool>>(value).size()) + 16;
        case VECTOR_STRING: {
            int s = 16;
            for (auto& x : std::get<std::vector<std::string>>(value)) s += static_cast<int>(x.size()) + 1;
            return s;
        }
        case LIST_INT: return static_cast<int>(std::get<std::list<int>>(value).size()) * 16 + 16;
        case LIST_STRING: {
            int s = 16;
            for (auto& x : std::get<std::list<std::string>>(value)) s += static_cast<int>(x.size()) + 1;
            return s;
        }
        case SET_INT: return static_cast<int>(std::get<std::set<int>>(value).size()) * 20 + 16;
        case MAP_STRING_INT: return static_cast<int>(std::get<std::map<std::string,int>>(value).size()) * 32 + 16;
        case VECTOR_OBJECT: return static_cast<int>(std::get<std::vector<std::shared_ptr<RuntimeObject>>>(value).size()) * 8 + 16;
        case OBJECT: {
            auto obj = std::get<std::shared_ptr<RuntimeObject>>(value);
            if (!obj) return 8;
            int total = 8; // vtable ptr + overhead
            for (auto& m : obj->members) total += m.second.byteSize();
            return total;
        }
        case POINTER: return 8;
        }
        return 4;
    }
};

#endif // VARIANT_VALUE_H
