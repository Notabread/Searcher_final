#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <utility>
#include <execution>

std::vector<std::string_view> SplitIntoWords(std::string_view text);

int64_t PrefixOffset(std::string_view text);


template<typename ExPo>
std::vector<std::string_view> SplitIntoWords(ExPo&& policy, std::string_view text) {
    std::vector<std::string_view> words;

    int64_t end = text.npos;
    while(true) {
        int64_t offset = PrefixOffset(text);
        if (offset == end) break;

        text.remove_prefix(offset);
        int64_t space = text.find(' ', 0);
        words.push_back(space == end ? text.substr(0) : text.substr(0, space));

        if (space == end) break;
        text.remove_prefix(space);
    }

    return words;
}

std::string ToString(const std::string_view text);
