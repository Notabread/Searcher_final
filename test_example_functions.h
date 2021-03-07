#pragma once

#include "search_server.h"
#include "string"
#include "vector"

void AddDocument(SearchServer& searchServer, int id, const std::string& query, DocumentStatus status,
        const std::vector<int>& marks);
