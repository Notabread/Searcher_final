#pragma once

#include <string>
#include <iostream>
#include <utility>
#include <vector>
#include <map>
#include <set>

template <typename TestFunc>
void RunTestImpl(const TestFunc func, const std::string& funcName) {
    using namespace std;
    func();
    cerr << funcName << " OK"s << endl;
}
#define RUN_TEST(func) RunTestImpl(func, #func)

template <typename Key, typename Value>
std::ostream& operator<<(std::ostream& os, const std::pair<Key, Value>& item) {
    using namespace std;
    os << item.first << ": "s << item.second;
    return os;
}

template <typename Container>
std::ostream& Print(std::ostream& os, const Container& container) {
    using namespace std;
    bool first = true;
    for (const auto& item : container) {
        if (!first) {
            os << ", "s;
        } else {
            first = false;
        }
        os << item;
    }
    return os;
}

template <typename Item>
std::ostream& operator<<(std::ostream& os, const std::vector<Item>& container) {
    using namespace std;
    os << "["s;
    return Print(os, container) << "]"s;
}

template <typename Item>
std::ostream& operator<<(std::ostream& os, const std::set<Item>& container) {
    using namespace std;
    os << "{"s;
    return Print(os, container) << "}"s;
}

template <typename Key, typename Value>
std::ostream& operator<<(std::ostream& os, const std::map<Key, Value>& container) {
    using namespace std;
    os << "{"s;
    return Print(os, container) << "}"s;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint) {
    using namespace std;
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << std::endl;
        abort();
    }
}
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, "")
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint);
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
