#include "string_processing.h"

using namespace std;

std::vector<std::string> SplitIntoWords(const string& text) {
    return SplitIntoWords(execution::seq, text);
}
