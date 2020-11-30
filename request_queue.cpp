#include "request_queue.h"

using namespace std;

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    const vector<Document> results = server_.FindTopDocuments(raw_query, status);
    Update(results);
    return results;
}

int RequestQueue::GetNoResultRequests() const {
    return empty_requests_;
}

void RequestQueue::Update(const vector<Document>& results) {
    ++time_;
    //Удаляем устаревшие результаты
    while (!requests_.empty() && sec_in_day_ <= time_ - requests_.front().time) {
        PopFrontRequest();
    }
    //Сохраняем новый результат
    QueryResult result = {
            time_,
            results.empty()
    };
    requests_.push_back(result);
    if (result.isEmpty) {
        ++empty_requests_;
    }
}

void RequestQueue::PopFrontRequest() {
    QueryResult result = requests_.front();
    if (result.isEmpty) {
        --empty_requests_;
    }
    requests_.pop_front();
}
