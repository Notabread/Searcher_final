#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <deque>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

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

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ' && word != ""s) {
            words.push_back(word);
            word = ""s;
        } else if (c != ' ') {
            word += c;
        }
    }
    if (word != ""s) {
        words.push_back(word);
    }
    return words;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

struct Document {
    Document() = default;

    Document(int document_id, double document_relevance, int document_rating)
    :id(document_id),
    relevance(document_relevance),
    rating(document_rating) {}

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

ostream& operator<<(ostream& os, const Document& document) {
    os << "{ "s;
    os << "document_id = "  << document.id << ", "s;
    os << "relevance = "    << document.relevance << ", "s;
    os << "rating = "       << document.rating;
    os << " }"s;
    return os;
}

class SearchServer {
public:

    SearchServer() = default;

    explicit SearchServer(const string& stop_text) : SearchServer(SplitIntoWords(stop_text)) {}

    template <typename Container>
    explicit SearchServer(const Container& stop_words) {
        for(const string& word : stop_words) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Stop word \""s + word + "\" has an invalid entry!"s);
            }
        }
        for(const string& word : stop_words) {
            stop_words_.emplace(word);
        }
    }

    void AddDocument(int document_id, const string& document, const DocumentStatus status, const vector<int>& ratings) {
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

    /*
     * Основная функция поиска самых подходящих документов по запросу.
     * Для уточнения поиска используется функция предикат.
     */
    template <typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query, const Predicate predicate) const {
        Query query = ParseQuery(raw_query);
        vector<Document> matched_documents = FindAllDocuments(query, predicate);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return (!IsDoubleEqual(lhs.relevance, rhs.relevance) && lhs.relevance > rhs.relevance)
                    || (lhs.rating > rhs.rating && IsDoubleEqual(lhs.relevance, rhs.relevance));
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    /*
     * Перегрузка функции поиска с использованием статуса в качестве второго парметра
     * вместо функции предиката.
     */

    vector<Document>  FindTopDocuments(const string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(
                raw_query,
                [status](const int doc_id, const DocumentStatus doc_status, const int rating) {
                    return doc_status == status;
                });
    }

    /*
     * Функция, которая возвращает кортеж из вектора совпавших слов из raw_query в документе document_id.
     * Если таких нет или совпало хоть одно минус слово, кортеж возвращается с пустым вектором слов
     * и статусом документа.
     */
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        if (document_parameters_.count(document_id) == 0) {
            return  tuple (matched_words, DocumentStatus::REMOVED);
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) > 0 && word_to_document_freqs_.at(word).count(document_id)) {
                return tuple (matched_words, document_parameters_.at(document_id).status);
            }
        }
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) > 0 && word_to_document_freqs_.at(word).count(document_id) > 0) {
                matched_words.push_back(word);
            }
        }
        return tuple (
                matched_words,
                document_parameters_.at(document_id).status
        );
    }

    int GetDocumentCount() const {
        return document_parameters_.size();
    }

    int GetDocumentId(int index) const {
        return ids_.at(index);
    }

private:
    struct DocsParams {
        DocumentStatus status = DocumentStatus::ACTUAL;
        int rating = 0;
    };
    vector<int> ids_;
    map<int, DocsParams> document_parameters_;
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;

    [[nodiscard]] bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

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
    [[nodiscard]] static bool IsDoubleEqual(const double first, const double second) {
        const double EPSILON = 1e-6;
        return abs(first - second) < EPSILON;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus = false;
        bool is_stop = false;
    };

    /*
     * Анализ слова.
     * Возвращает структуру со свойствами слова word.
     */
    QueryWord ParseQueryWord(string word) const {
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

    /*
     * Вычисление IDF слова.
     */
    double ComputeWordInverseDocumentFreq(const string& word) const {
        if (word_to_document_freqs_.count(word) && word_to_document_freqs_.at(word).size() > 0) {
            return log(GetDocumentCount() * 1.0 / static_cast<double>(word_to_document_freqs_.at(word).size()));
        }
        return 0.0;
    }

    /*
     * Проверяет, есть ли в документе с id = document_id минус слово.
     */
    bool HasMinusWord(const int document_id, const set<string>& minus_words) const {
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

    /*
     * Проверяет, удовлетворяет ли документ требованиям запроса.
     * Сначала идёт проверка через функцию предикат, а потом проверка на минус слово.
     */
    template <typename Predicate>
    [[nodiscard]] bool IsDocumentAllowed(const int document_id, const set<string>& minus_words,
            const Predicate predicate) const {
        return predicate(
                document_id,
                document_parameters_.at(document_id).status,
                document_parameters_.at(document_id).rating
            ) && !HasMinusWord(document_id, minus_words);
    }

    /*
     * Поиск всех документов, удовлетворяющих запросу.
     */
    template <typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, const Predicate predicate) const {
        map<int, double> document_to_relevance;

        //Проходим по плюс словам и заполняем словарь document_to_relevance
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0 || query.minus_words.count(word) > 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (IsDocumentAllowed(document_id, query.minus_words, predicate)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        //Объявляем и заполняем вектор документов
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                document_parameters_.at(document_id).rating
            });
        }
        return matched_documents;
    }

    [[nodiscard]] static bool IsValidWord(const string& word) {
        //Проверка на пустые слова и слова с двойными минусами
        if (word == ""s || word[0] == '-') {
            return false;
        }
        //Проверка на спецсимволы
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

};

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end) : begin_(begin), end_(end) {}

    Iterator begin() const {
        return begin_;
    }

    Iterator end() const {
        return end_;
    }

    size_t size() const {
        return  distance(begin_, end_);
    }
    
private:
    Iterator begin_;
    Iterator end_;

};

template <typename Iterator>
ostream& operator<<(ostream& os, const IteratorRange<Iterator>& range) {
    for (auto it = range.begin(); it != range.end(); ++it) {
        os << *it;
    }
    return os;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator container_begin, Iterator container_end, size_t size) {
        size_t container_size = distance(container_begin, container_end);
        for (size_t i = 0; i < container_size; i += size) {
            Iterator current_begin = container_begin;
            advance(current_begin, i);
            Iterator current_end = current_begin;
            if (i + size < container_size) {
                advance(current_end, size);
            } else {
                current_end = container_end;
            }
            pages_.push_back({
                current_begin,
                current_end
            });
        }
    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

private:
    vector<IteratorRange<Iterator>> pages_;

};

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) : server_(search_server), time_(0), empty_requests_(0) {}
    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
        const vector<Document> results = server_.FindTopDocuments(raw_query, document_predicate);
        Update(results);
        return results;
    }

    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status) {
        const vector<Document> results = server_.FindTopDocuments(raw_query, status);
        Update(results);
        return results;
    }

    vector<Document> AddFindRequest(const string& raw_query) {
        const vector<Document> results = server_.FindTopDocuments(raw_query);
        Update(results);
        return results;
    }

    int GetNoResultRequests() const {
        return empty_requests_;
    }
private:
    struct QueryResult {
        size_t time = 0;
        bool isEmpty = false;
    };
    deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    const SearchServer& server_;
    size_t time_;
    int empty_requests_;
    void Update(const vector<Document>& results) {
        ++time_;
        //Удаляем устаревшие результаты
        while (!requests_.empty() && sec_in_day_ <= time_ - requests_.front().time) {
            RemoveRequest();
        }
        //Сохраняем новый результат
        QueryResult result = {
                time_,
                results.empty()
        };
        requests_.push_back(result);
        if (result.isEmpty) {
            ++empty_requests_;
        }
    }
    void RemoveRequest() {
        QueryResult result = requests_.front();
        if (result.isEmpty) {
            --empty_requests_;
        }
        requests_.pop_front();
    }
};

// -------- Реализация тестирующего фреймворка ----------

template <typename TestFunc>
void RunTestImpl(const TestFunc func, const string& funcName) {
    func();
    cerr << funcName << " OK" << endl;
}
#define RUN_TEST(func) RunTestImpl(func, #func)

template <typename Key, typename Value>
ostream& operator<<(ostream& os, const pair<Key, Value>& item) {
    os << item.first << ": " << item.second;
    return os;
}

template <typename Container>
ostream& Print(ostream& os, const Container& container) {
    bool first = true;
    for (const auto& item : container) {
        if (!first) {
            os << ", "s;
        } else {
            first = false;
        }
        os << item;
    }
    return os;
}

template <typename Item>
ostream& operator<<(ostream& os, const vector<Item>& container) {
    os << "["s;
    return Print(os, container) << "]"s;
}

template <typename Item>
ostream& operator<<(ostream& os, const set<Item>& container) {
    os << "{"s;
    return Print(os, container) << "}"s;
}

template <typename Key, typename Value>
ostream& operator<<(ostream& os, const map<Key, Value>& container) {
    os << "{"s;
    return Print(os, container) << "}"s;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
// -------- Реализация тестирующего фреймворка ----------

// -------- Начало модульных тестов поисковой системы ----------

void TestAddingDocument() {
    SearchServer server;
    auto found_docs_pre = server.FindTopDocuments("in city"s);
    ASSERT_EQUAL(found_docs_pre.size(), 0);

    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "dog at home"s, DocumentStatus::ACTUAL, {1, 15, 3});

    auto found_docs = server.FindTopDocuments("in city"s);
    ASSERT_EQUAL(found_docs.size(), 1);
    const Document& doc = found_docs[0];
    ASSERT_EQUAL(doc.id, 1);

    auto found_docs2 = server.FindTopDocuments("at home"s);
    ASSERT_EQUAL(found_docs2.size(), 1);
    const Document& doc2 = found_docs2[0];
    ASSERT_EQUAL(doc2.id, 2);

    auto found_docs3 = server.FindTopDocuments("cat at the home"s);
    ASSERT_EQUAL(found_docs3.size(), 2);
}

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 0);
    }
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto found_docs = server.FindTopDocuments("city"s);
        ASSERT_EQUAL(found_docs.size(), 1);
    }
}

void TestMinusWordsExcludeDocs() {
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(44, "fat dog at home"s, DocumentStatus::ACTUAL, {1, 5, 3});
    server.AddDocument(45, "fat rat beat the cat"s, DocumentStatus::ACTUAL, {1, 2, -3});

    auto found_docs = server.FindTopDocuments("cat in home -cat -fat"s);
    ASSERT_EQUAL(found_docs.size(), 0);
    auto found_docs2 = server.FindTopDocuments("cat in home"s);
    ASSERT_EQUAL(found_docs2.size(), 3);
    auto found_docs3 = server.FindTopDocuments("cat in home -fat"s);
    ASSERT_EQUAL(found_docs3[0].id, 42);
    auto found_docs4 = server.FindTopDocuments("cat in home -bag"s);
    ASSERT_EQUAL(found_docs4.size(), 3);
    auto found_docs5 = server.FindTopDocuments("cat in home -rat"s);
    ASSERT_EQUAL(found_docs5.size(), 2);
}

void TestMatchDocument() {
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::REMOVED, {1, 3, 3});
    server.AddDocument(56, "fat rat in the house"s, DocumentStatus::ACTUAL, {1, 3, 3});

    auto match = server.MatchDocument("cat in home"s, 42);
    vector<string>& result_words = get<0>(match);
    sort(result_words.begin(), result_words.end());
    vector<string> words = {"cat"s, "in"s};
    ASSERT_EQUAL(result_words, words);
    ASSERT(get<1>(match) == DocumentStatus::REMOVED);

    auto match2 = server.MatchDocument("cat at the city"s, 42);
    ASSERT_EQUAL(get<0>(match2).size(), 3);
    ASSERT(get<1>(match2) == DocumentStatus::REMOVED);

    auto match3 = server.MatchDocument("cat the city -at"s, 42);
    ASSERT_EQUAL(get<0>(match3).size(), 3);
    ASSERT(get<1>(match3) == DocumentStatus::REMOVED);

    auto match4 = server.MatchDocument("fat cat in city"s, 56);
    ASSERT_EQUAL(get<0>(match4).size(), 2);
    ASSERT(get<1>(match4) == DocumentStatus::ACTUAL);

    auto match5 = server.MatchDocument("fat cat in city -rat"s, 56);
    ASSERT_EQUAL(get<0>(match5).size(), 0);
    ASSERT(get<1>(match5) == DocumentStatus::ACTUAL);
}

void TestRelevanceSort() {
    SearchServer server;
    server.AddDocument(1, "fat rat in the house"s, DocumentStatus::ACTUAL, {1, 5, 24});
    server.AddDocument(2, "cat in the city"s, DocumentStatus::ACTUAL, {1, 34, 3});
    server.AddDocument(3, "fat cat in the house"s, DocumentStatus::ACTUAL, {1, 2, 1});

    auto docs = server.FindTopDocuments("cat in city"s);
    ASSERT(docs[0].relevance >= docs[1].relevance);
    ASSERT(docs[1].relevance >= docs[2].relevance);
}

void TestRatingCompute() {
    {
        SearchServer server;
        server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 5, 3});
        auto docs = server.FindTopDocuments("cat in city"s);
        ASSERT_EQUAL(docs[0].rating, 3);
    }
    {
        SearchServer server;
        server.AddDocument(1, "cat"s, DocumentStatus::ACTUAL, {2, -5, -3});
        auto docs = server.FindTopDocuments("cat in city"s);
        ASSERT_EQUAL(docs[0].rating, -2);
    }
    {
        SearchServer server;
        server.AddDocument(1, "fat cat in the house"s, DocumentStatus::ACTUAL, {});
        auto docs = server.FindTopDocuments("cat in house"s);
        ASSERT_EQUAL(docs[0].rating, 0);
    }
}

void TestPredicateFiltering() {
    SearchServer server;
    server.AddDocument(1, "fat rat in the house"s, DocumentStatus::ACTUAL, {1, 5, 3});
    server.AddDocument(2, "cat in the city"s, DocumentStatus::BANNED, {1, 2, 3});
    server.AddDocument(3, "fat cat in the house"s, DocumentStatus::REMOVED, {});

    auto docs = server.FindTopDocuments("cat in city"s, [](const int id, const DocumentStatus status,
            const int rating) {
        return status == DocumentStatus::REMOVED;
    });
    ASSERT_EQUAL(docs.size(), 1);
    ASSERT_EQUAL(docs[0].id, 3);

    auto docs2 = server.FindTopDocuments("cat in city"s, [](const int id, const DocumentStatus status,
            const int rating) {
        return id == 1 || id == 2;
    });
    ASSERT_EQUAL(docs2.size(), 2);

    auto docs3 = server.FindTopDocuments("cat in city"s, [](const int id, const DocumentStatus status,
            const int rating) {
        return rating == 3;
    });
    ASSERT_EQUAL(docs3.size(), 1);
    ASSERT_EQUAL(docs3[0].id, 1);
}

void TestStatusFiltering() {
    SearchServer server;
    server.AddDocument(1, "fat rat in the house"s, DocumentStatus::ACTUAL, {1, 5, 24, 14});
    server.AddDocument(2, "cat in the city"s, DocumentStatus::BANNED, {1, -34, 3});
    server.AddDocument(3, "fat cat in the house"s, DocumentStatus::REMOVED, {0});
    server.AddDocument(4, "fat cat in the house"s, DocumentStatus::IRRELEVANT, {});

    auto docs = server.FindTopDocuments("in the house"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(docs.size(), 1);
    ASSERT_EQUAL(docs[0].id, 1);

    auto docs2 = server.FindTopDocuments("in the house"s, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(docs2.size(), 1);
    ASSERT_EQUAL(docs2[0].id, 4);

    auto docs3 = server.FindTopDocuments("in the house"s, DocumentStatus::REMOVED);
    ASSERT_EQUAL(docs3.size(), 1);
    ASSERT_EQUAL(docs3[0].id, 3);

    auto docs4 = server.FindTopDocuments("in the house"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(docs4.size(), 1);
    ASSERT_EQUAL(docs4[0].id, 2);

}

void TestRelevanceComputing() {
    SearchServer search_server;
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    const double EPSILON = 1e-6;
    ASSERT(abs(docs[0].relevance - 0.866434) < EPSILON);
    ASSERT(abs(docs[1].relevance - 0.173287) < EPSILON);
    ASSERT(abs(docs[2].relevance - 0.138629) < EPSILON);
}

void TestSearchServer() {
    RUN_TEST(TestAddingDocument);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWordsExcludeDocs);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestRelevanceComputing);
    RUN_TEST(TestRelevanceSort);
    RUN_TEST(TestRatingCompute);
    RUN_TEST(TestPredicateFiltering);
    RUN_TEST(TestStatusFiltering);
}
// --------- Окончание модульных тестов поисковой системы -----------

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

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
    cout << "Запросов, по которым ничего не нашлось "s << request_queue.GetNoResultRequests();
    return 0;
}
