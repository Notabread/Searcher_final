#pragma once

#include "search_server.h"
#include<string>
#include<map>

void RemoveDuplicates(SearchServer& search_server);

bool IsSameWords(const std::map<std::string, double>& word_to_freq_1, const std::map<std::string, double>& word_to_freq_2);
