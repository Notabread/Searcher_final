#pragma once

#include "search_server.h"
#include<string>
#include<map>

std::set<std::string> MakeWordsSet(const std::map<std::string, double>& word_to_freq);

void RemoveDuplicates(SearchServer& search_server);
