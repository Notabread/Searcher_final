#include <iostream>
#include <string>

#include "request_queue.h"
#include "unit_tests.h"
#include "paginator.h"

using namespace std;

int main() {
    TestSearchServer();
    SearchServer search_server("и в на"s);
    RequestQueue request_queue(search_server);

    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "большой кот модный ошейник "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "большой пёс скворец василий"s, DocumentStatus::ACTUAL, {1, 1, 1});

    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("пустой запрос"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("пушистый пёс"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("большой ошейник"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("скворец"s);
    cout << "Запросов, по которым ничего не нашлось "s << request_queue.GetNoResultRequests() << endl;

    auto pages = Paginate(search_server.FindTopDocuments("большой пушыстый кот евгений хвост пёс модный скворец и ошейник и хвост ещё"s), 2);
    for (auto it = pages.begin(); it != pages.end(); ++it) {
        cout << *it << endl << "---page break---" << endl;
    }

    return 0;
}
