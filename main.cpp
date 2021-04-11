#include "search_server.h"

#include <iostream>
#include <string>
#include <vector>
#include "process_queries.h"

using namespace std;

int main() {
    SearchServer search_server("and with"s);

    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
    }
            ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }


    const string query = "--curly and funny -not"s;

    {

        try {

            const auto [words, status] = search_server.MatchDocument(query, 1);
            cout << words.size() << " words for document 1"s << endl;
            // 1 words for document 1
        } catch (std::invalid_argument e) {
            cout << e.what() << endl;
        }
    }


    return 0;
}
