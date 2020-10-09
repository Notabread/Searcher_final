#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;


const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

/*
 * Просто разбиение строки слов в вектор слов с пробелом в качестве разделителя.
 */
vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            words.push_back(word);
            word = "";
        } else {
            word += c;
        }
    }
    words.push_back(word);
    return words;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

struct Document {
    int id;
    double relevance;
    int rating;
    DocumentStatus status;
};

class SearchServer {
public:

    /*
     * Установка стоп слов. То есть слов, которые будут удаляться из запроса.
     */
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    /*
     * Функция добавления нового документа.
     */
    void AddDocument(
            int document_id,
            const string& document,
            DocumentStatus status,
            const vector<int>& ratings
        )
    {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        document_ratings_.emplace(document_id, ComputeAverageRating(ratings));
        document_status_.emplace(document_id, status);
        ++document_count_;
    }

    /*
     * Основная функция поиска самых подходящих документов по запросу.
     * Для уточнения поиска используется функция предикат.
     */
    template <typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query, Predicate predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return (!IsDoubleEqual(lhs.relevance, rhs.relevance) && lhs.relevance > rhs.relevance) || (lhs.rating > rhs.rating && IsDoubleEqual(lhs.relevance, rhs.relevance));
             });
        PredicateFiltering(matched_documents, predicate);
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    /*
     * Перегрузка функции поиска с использованием статуса в качестве второго парметра
     * вместо функции предиката.
     */
    vector<Document> FindTopDocuments(const string& raw_query, const DocumentStatus status = DocumentStatus::ACTUAL) const
    {
        return FindTopDocuments(raw_query, [status](const int doc_id, const DocumentStatus doc_status, const int rating) { return doc_status == status; });
    }

    /*
     * Функция, которая возвращает кортеж из вектора совпавших слов из raw_query в документе document_id.
     * Если таких нет или совпало хоть одно минус слово, кортеж возвращается с пустым вектором слов
     * и статусом документа.
     */
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) > 0 && word_to_document_freqs_.at(word).count(document_id)) {
                return { {}, document_status_.at(document_id) };
            }
        }
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) > 0 && word_to_document_freqs_.at(word).count(document_id) > 0) {
                matched_words.push_back(word);
            }
        }
        return { matched_words, document_status_.at(document_id) };
    }

    /*
     * Получить общее количество добавленных документов.
     */
    int GetDocumentCount() const {
        return document_count_;
    }

private:
    int document_count_ = 0;
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, int> document_ratings_;
    map<int, DocumentStatus> document_status_;

    /*
     * Фильтрация документов documents по ссылке через функцию предикат.
     */
    template <typename Predicate>
    void PredicateFiltering(vector<Document>& documents, Predicate predicate) const {
        vector<Document> temp;
        for (const Document& doc : documents) {
            if (predicate(doc.id, doc.status, doc.rating)) {
                temp.push_back(doc);
            }
        }
        documents = temp;
    }

    /*
     * Определить, является ли слово стоп словом.
     */
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    /*
     * Разбить строку на слова, исключая стоп слова.
     */
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    /*
     * Определяет, являются ли два double числа равными с погрешностью 1e-6.
     */
    static bool IsDoubleEqual(const double& first, const double& second) {
        const double EPSILON = 1e-6;
        return abs(first - second) < EPSILON;
    }

    /*
     * Вычисление среднего рейтинга документа.
     */
    static int ComputeAverageRating(const vector<int>& ratings) {
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    /*
     * Анализ слова.
     * Возвращает структуру со свойствами слова word.
     */
    QueryWord ParseQueryWord(string word) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (word[0] == '-') {
            is_minus = true;
            word = word.substr(1);
        }
        return {
                word,
                is_minus,
                IsStopWord(word)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    /*
     * Разбивает строку-запрос на плюс и минус слова, исключая стоп слова.
     * Возвращает структуру с двумя множествами этих слов.
     */
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
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

    /*
     * Вычисление IDF слова.
     */
    double ComputeWordInverseDocumentFreq(const string& word) const {
        if (word_to_document_freqs_.count(word) && static_cast<int>(word_to_document_freqs_.at(word).size()) > 0) {
            return log(document_count_ * 1.0 / static_cast<double>(word_to_document_freqs_.at(word).size()));
        }
        return 0.0;
    }

    /*
     * Поиск всех документов, удовлетворяющих запросу query.
     */
    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> document_to_relevance;

        //Проходим по плюс словам и заполняем словарь document_to_relevance
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }

        //Проходимся по минус словам и удаляем из найденных выше документов те, в которых есть минус слово
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        //Объявляем и заполняем вектор структур документов
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                document_ratings_.at(document_id),
                document_status_.at(document_id)
            });
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}