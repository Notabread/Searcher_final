#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <utility>
#include <execution>

std::vector<std::string> SplitIntoWords(const std::string& text);

template<typename ExPo>
std::vector<std::string> SplitIntoWords(ExPo&& policy, const std::string& text) {
    using namespace std::literals;
    std::vector<std::string> words;
    std::string acc;

    std::for_each(
            policy,
            text.begin(), text.end(),
            [&words, &acc](const char c){
                if (c == ' ' && acc != ""s) {
                    words.push_back(std::move(acc));
                } else if (c != ' ') {
                    acc += c;
                }
            }
    );
    if (acc != ""s) {
        words.push_back(std::move(acc));
    }

    return words;
}
