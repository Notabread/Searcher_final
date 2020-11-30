#include "document.h"

using namespace std;

ostream& operator<<(ostream& os, const Document& document) {
    os << "{ "s;
    os << "document_id = "  << document.id << ", "s;
    os << "relevance = "    << document.relevance << ", "s;
    os << "rating = "       << document.rating;
    os << " }"s;
    return os;
}
