#include "string_processing.h"

using namespace std;

std::vector<std::string> SplitIntoWords(const string& text) {
    vector<std::string> words;
    string word;
    for (const char c : text) {
        if (c == ' ' && word != ""s) {
            words.push_back(word);
            word = ""s;
        } else if (c != ' ') {
            word += c;
        }
    }
    if (word != ""s) {
        words.push_back(word);
    }
    return words;
}