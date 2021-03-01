#include "remove_duplicates.h"

using namespace std;

bool IsSameWords(const map<string, double>& word_to_freq_1, const map<string, double>& word_to_freq_2) {
    if (word_to_freq_1.size() != word_to_freq_2.size()) {
        return false;
    }
    auto it_1 = word_to_freq_1.begin();
    auto it_2 = word_to_freq_2.begin();

    for(; it_1 != word_to_freq_1.end(); ++it_1) {
        if ((*it_1).first != (*it_2).first) {
            return false;
        }
        ++it_2;
    }

    return true;
}

void RemoveDuplicates(SearchServer& search_server) {
    auto begin = search_server.begin();
    auto end = search_server.end();
    set<int> documents_to_remove;
    for (auto it = begin; it != end; ++it) {
        int document_id = *it;
        if (documents_to_remove.count(document_id) > 0) {
            continue;
        }
        map<string, double> word_to_freq = search_server.GetWordFrequencies(document_id);

        for (auto it_2 = next(it); it_2 != end; ++it_2) {
            int document_id_2 = *it_2;
            if (documents_to_remove.count(document_id_2) > 0) {
                continue;
            }
            map<string, double> word_to_freq_2 = search_server.GetWordFrequencies(document_id_2);
            if (IsSameWords(word_to_freq, word_to_freq_2)) {
                documents_to_remove.insert(document_id_2);
            }
        }

    }
    for (const int id : documents_to_remove) {
        search_server.RemoveDocument(id);
        cout << "Found duplicate document id "s << id << endl;
    }
}
