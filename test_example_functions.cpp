#include "test_example_functions.h"

using namespace std;

void AddDocument(SearchServer& searchServer, int id, const string& query, DocumentStatus status, const vector<int>& marks) {
    searchServer.AddDocument(id, query, status, marks);
}
