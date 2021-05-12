#include "process_queries.h"
#include <algorithm>
#include <string>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server,
                                                  const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> result(queries.size());
    std::transform(
            std::execution::par,
            queries.begin(), queries.end(),
            result.begin(),
            [&search_server](const std::string& query){
                return search_server.FindTopDocuments(query);
            }
    );
    return result;

}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server,
                                           const std::vector<std::string>& queries) {
    auto processed = ProcessQueries(search_server, queries);

    int realSize = 0;
    for (const auto& item : processed) {
        realSize += item.size();
    }

    std::vector<Document> result;
    result.reserve(realSize);

    for (const auto& item : processed) {
        for (const Document& doc : item) {
            result.push_back( doc );
        }
    }

    return result;
}
