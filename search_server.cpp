#include "search_server.h"
#include <execution>

using namespace std;

void SearchServer::AddDocument(int document_id, const string_view document, const DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0) {
        throw invalid_argument("Negative document id = "s + to_string(document_id) + "!"s);
    }
    if (document_parameters_.count(document_id) > 0) {
        throw invalid_argument("Document with id = "s + to_string(document_id) + " already exists!"s);
    }
    vector<string_view> words = SplitIntoWordsNoStop(document);
    for (const string_view word : words) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word in adding document has an invalid entry!"s);
        }
    }

    //Добавляем оригиналы слов в word_to_documents_
    for (std::string_view word : words) {
        word_to_documents_[std::string(word)].insert(document_id);
    }

    //А тут уже используем string_view
    const double inv_word_count = words.empty() ? 0.0 : 1.0 / static_cast<int>(words.size());
    for (const string_view word : words) {
        id_to_word_freq_[document_id][GetSourceView(word)] += inv_word_count;
    }
    DocsParams params = {
            status,
            ComputeAverageRating(ratings),
    };
    document_parameters_.emplace(document_id, params);
    ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(
            raw_query,
            [status](const int doc_id, const DocumentStatus doc_status, const int rating) {
                return doc_status == status;
            });
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

int SearchServer::GetDocumentCount() const {
    return ids_.size();
}

std::set<int>::iterator SearchServer::begin() {
    return ids_.begin();
}

std::set<int>::iterator SearchServer::end() {
    return ids_.end();
}

const map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static map <std::string_view, double> empty_map;
    return id_to_word_freq_.count(document_id) > 0 ? id_to_word_freq_.at(document_id) : empty_map;
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

bool SearchServer::IsWordInDocument(const string_view word, const int document_id) const {
    if (id_to_word_freq_.count(document_id) == 0 || id_to_word_freq_.at(document_id).count(word) == 0) {
        return false;
    }
    return true;
}

bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(word) > 0;
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWords(text)) {
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

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view word) const {
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

SearchServer::Query SearchServer::ParseQuery(const std::string_view text) const {
    return ParseQuery(std::execution::seq, text);
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    int word_count = DocumentsWithWord(word).size();
    if (word_count > 0) {
        return log(GetDocumentCount() * 1.0 / static_cast<double>(word_count));
    }
    return 0.0;
}

const set<int>& SearchServer::DocumentsWithWord(const string_view word) const {
    static set<int> empty;
    return word_to_documents_.count(word) > 0 ? word_to_documents_.find(word)->second : empty;
}

bool SearchServer::HasMinusWord(const int document_id, const set<string_view>& minus_words) const {
    //Проходимся по минус словам
    for (const string_view word : minus_words) {
        //Если в словаре с этим словом находим document_id, возвращаем true
        if (IsWordInDocument(word, document_id)) {
            return true;
        }
    }
    return false;
}

bool SearchServer::IsValidWord(const string_view word) {
    //Проверка на пустые слова и слова с двойными минусами
    if (word == ""s || word[0] == '-') {
        return false;
    }
    //Проверка на спецсимволы
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

//Возвращает string_view именно из хранилища самого сервера
std::string_view SearchServer::GetSourceView(const string_view word) const {
    if (word_to_documents_.count(word) == 0) {
        return {};
    }
    return word_to_documents_.find(word)->first;
}
