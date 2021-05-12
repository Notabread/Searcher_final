#include "remove_duplicates.h"

using namespace std;

set<string_view> MakeWordsSet(const map<string_view, double>& word_to_freq) {
    set<string_view> words;
    for (const auto& [word, _] : word_to_freq) {
        words.insert(word);
    }
    return words;
}

void RemoveDuplicates(SearchServer& search_server) {
    auto begin = search_server.begin();
    auto end = search_server.end();
    set<int> documents_to_remove;
    set<set<string_view>> existing_sets;
    for (auto it = begin; it != end; ++it) {
        int document_id = *it;
        if (documents_to_remove.count(document_id) > 0) {
            continue;
        }
        const map<string_view, double>& word_to_freq = search_server.GetWordFrequencies(document_id);
        set<string_view> words = MakeWordsSet(word_to_freq);
        if (existing_sets.count(words) > 0) {
            documents_to_remove.insert(document_id);
            continue;
        }
        existing_sets.insert(words);
    }
    for (const int id : documents_to_remove) {
        search_server.RemoveDocument(id);
        cout << "Found duplicate document id "s << id << endl;
    }
}
