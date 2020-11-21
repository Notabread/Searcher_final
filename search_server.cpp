#include "search_server.h"

using namespace std;

void SearchServer::AddDocument(int document_id, const string& document, const DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0) {
        throw invalid_argument("Negative document id = "s + to_string(document_id) + "!"s);
    }
    if (document_parameters_.count(document_id) > 0) {
        throw invalid_argument("Document with id = "s + to_string(document_id) + " already exists!"s);
    }
    const vector<string> words = SplitIntoWordsNoStop(document);
    for (const string& word : words) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word \""s + word + "\" in adding document has an invalid entry!"s);
        }
    }
    const double inv_word_count = words.empty() ? 0.0 : 1.0 / static_cast<int>(words.size());
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    DocsParams params = {
            status,
            ComputeAverageRating(ratings),
    };
    document_parameters_.emplace(document_id, params);
    ids_.push_back(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(
            raw_query,
            [status](const int doc_id, const DocumentStatus doc_status, const int rating) {
                return doc_status == status;
            });
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return FindTopDocuments(
            raw_query,
            [](const int doc_id, const DocumentStatus doc_status, const int rating) {
                return doc_status == DocumentStatus::ACTUAL;
            });
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    Query query = ParseQuery(raw_query);
    vector<string> matched_words;
    if (document_parameters_.count(document_id) == 0) {
        return  make_tuple (matched_words, DocumentStatus::REMOVED);
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) > 0 && word_to_document_freqs_.at(word).count(document_id)) {
            return make_tuple (matched_words, document_parameters_.at(document_id).status);
        }
    }
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) > 0 && word_to_document_freqs_.at(word).count(document_id) > 0) {
            matched_words.push_back(word);
        }
    }
    return make_tuple (
        matched_words,
        document_parameters_.at(document_id).status
    );
}

int SearchServer::GetDocumentCount() const {
    return document_parameters_.size();
}

int SearchServer::GetDocumentId(int index) const {
    return ids_.at(index);
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

bool SearchServer::IsDoubleEqual(const double first, const double second) {
    const double EPSILON = 1e-6;
    return abs(first - second) < EPSILON;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string word) const {
    bool is_minus = false;
    if (word.size() > 0 && word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    return {
        word,
        is_minus,
        IsStopWord(word)
    };
}

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    Query query;
    for (const string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!IsValidWord(query_word.data)) {
            throw invalid_argument("Word \""s + word + "\" in query has an invalid entry!"s);
        }
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    if (word_to_document_freqs_.count(word) && word_to_document_freqs_.at(word).size() > 0) {
        return log(GetDocumentCount() * 1.0 / static_cast<double>(word_to_document_freqs_.at(word).size()));
    }
    return 0.0;
}

bool SearchServer::HasMinusWord(const int document_id, const set<string>& minus_words) const {
    //Проходимся по минус словам
    for (const string& word : minus_words) {
        //Если в словаре с этим словом находим document_id, возвращаем true
        if (word_to_document_freqs_.count(word) > 0 &&
            word_to_document_freqs_.at(word).count(document_id) > 0) {
            return true;
        }
    }
    return false;
}

bool SearchServer::IsValidWord(const string& word) {
    //Проверка на пустые слова и слова с двойными минусами
    if (word == ""s || word[0] == '-') {
        return false;
    }
    //Проверка на спецсимволы
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}
