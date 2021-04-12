#include "string_processing.h"

using namespace std;

std::vector<std::string_view> SplitIntoWords(string_view text) {
    return SplitIntoWords(execution::seq, text);
}

std::string ToString(const std::string_view text) {
    std::string result;
    for (auto it = text.begin(); it != text.end(); ++it) {
        result += *it;
    }
    return result;
}

int64_t PrefixOffset(std::string_view text) {
    return text.find_first_not_of(' ');
}
