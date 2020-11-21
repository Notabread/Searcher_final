#pragma once

#include <vector>
#include <string>
#include <deque>
#include <utility>

#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) : server_(search_server), time_(0), empty_requests_(0) {}
    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        const std::vector<Document> results = server_.FindTopDocuments(raw_query, document_predicate);
        Update(results);
        return results;
    }
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;
private:
    struct QueryResult {
        size_t time = 0;
        bool isEmpty = false;
    };
    std::deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    const SearchServer& server_;
    size_t time_;
    int empty_requests_;
    void Update(const std::vector<Document>& results);
    void RemoveRequest();
};
