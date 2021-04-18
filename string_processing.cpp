#include "string_processing.h"

using namespace std;

std::vector<std::string_view> SplitIntoWords(std::string_view text) {
    std::vector<std::string_view> words;

    int64_t end = text.npos;
    while(true) {
        int64_t offset = text.find_first_not_of(' ');
        if (offset == end) break;

        text.remove_prefix(offset);
        int64_t space = text.find(' ', 0);
        words.push_back(space == end ? text : text.substr(0, space));

        if (space == end) break;
        text.remove_prefix(space);
    }

    return words;
}
