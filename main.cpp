#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

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
    Document() = default;

    Document(int document_id, double document_relevance, int document_rating)
    :id(document_id),
    relevance(document_relevance),
    rating(document_rating) {}

    int id;
    double relevance;
    int rating;
};

class SearchServer {
public:

    SearchServer() = default;

    SearchServer(const string& stop_text) {
        vector<string> stop_words = SplitIntoWords(stop_text);
        for (const string& word : stop_words) {
            stop_words_.insert(word);
        }
    }

    template <typename Container>
    SearchServer(const Container& stop_words) {
        for(const string& word : stop_words) {
            if (word == ""s) {
                continue;
            }
            stop_words_.insert(word);
        }
    }

    [[nodiscard]] bool AddDocument(int document_id, const string& document, const DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0) {
            return false;
        }
        if (document_parameters_.count(document_id) > 0) {
            return false;
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / static_cast<int>(words.size());
        for (const string& word : words) {
            if (!IsValidWord(word)) {
                return false;
            }
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        DocsParams params = {
                status,
                ComputeAverageRating(ratings),
        };
        document_parameters_.emplace(document_id, params);
        ids_.push_back(document_id);
        return true;
    }

    /*
     * Основная функция поиска самых подходящих документов по запросу.
     * Для уточнения поиска используется функция предикат.
     */
    template <typename Predicate>
    [[nodiscard]] bool FindTopDocuments(const string& raw_query, const Predicate predicate, vector<Document>& result) const {
        Query query;
        if (!ParseQuery(query, raw_query)) {
            return false;
        }
        auto matched_documents = FindAllDocuments(query, predicate);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return (!IsDoubleEqual(lhs.relevance, rhs.relevance) && lhs.relevance > rhs.relevance)
                    || (lhs.rating > rhs.rating && IsDoubleEqual(lhs.relevance, rhs.relevance));
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        result = matched_documents;
        return true;
    }

    /*
     * Перегрузка функции поиска с использованием статуса в качестве второго парметра
     * вместо функции предиката.
     */
    [[nodiscard]] bool FindTopDocuments(const string& raw_query, vector<Document>& result) const {
        return FindTopDocuments(
                raw_query,
                [](const int doc_id, const DocumentStatus doc_status, const int rating) { return doc_status == DocumentStatus::ACTUAL; },
                result);
    }

    [[nodiscard]] bool FindTopDocuments(const string& raw_query, const DocumentStatus status, vector<Document>& result) const {
        return FindTopDocuments(
                raw_query,
                [status](const int doc_id, const DocumentStatus doc_status, const int rating) { return doc_status == status; },
                result);
    }

    /*
     * Функция, которая возвращает кортеж из вектора совпавших слов из raw_query в документе document_id.
     * Если таких нет или совпало хоть одно минус слово, кортеж возвращается с пустым вектором слов
     * и статусом документа.
     */
    [[nodiscard]] bool MatchDocument(const string& raw_query, int document_id, tuple<vector<string>, DocumentStatus>& result) const {
        Query query;
        if (!ParseQuery(query, raw_query)) {
            return false;
        }
        vector<string> matched_words;
        if (document_parameters_.count(document_id) == 0) {
            result = { {}, DocumentStatus::REMOVED };
            return true;
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) > 0 && word_to_document_freqs_.at(word).count(document_id)) {
                result = { {}, document_parameters_.at(document_id).status };
                return true;
            }
        }
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) > 0 && word_to_document_freqs_.at(word).count(document_id) > 0) {
                matched_words.push_back(word);
            }
        }
        result = { matched_words, document_parameters_.at(document_id).status };
        return true;
    }

    int GetDocumentCount() const {
        return document_parameters_.size();
    }

    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    int GetDocumentId(int index) const {
        if (index >= ids_.size() || index < 0) {
            return INVALID_DOCUMENT_ID;
        }
        return ids_.at(index);
    }

private:
    struct DocsParams {
        DocumentStatus status;
        int rating;
    };
    vector<int> ids_;
    map<int, DocsParams> document_parameters_;
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;

    bool IsStopWord(const string& word) const {
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
    static bool IsDoubleEqual(const double first, const double second) {
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
        bool is_minus;
        bool is_stop;
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
    bool ParseQuery(Query& result, const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!IsValidWord(query_word.data)) {
                return false;
            }
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        result = query;
        return true;
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
    bool IsDocumentAllowed(const int document_id, const set<string>& minus_words, const Predicate predicate) const {
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
void TestWordsValidate() {
    {
        //Проверка add функции
        SearchServer server;
        ASSERT(!server.AddDocument(1, "alpha --beta"s, DocumentStatus::ACTUAL, {}));
        ASSERT(!server.AddDocument(1, "alpha bet\1a"s, DocumentStatus::ACTUAL, {}));
        ASSERT(!server.AddDocument(1, "alpha -"s, DocumentStatus::ACTUAL, {}));

        ASSERT(server.AddDocument(1, "alpha betta-gamma"s, DocumentStatus::ACTUAL, {}));
    }
    {
        //Проверка Find функции
        SearchServer server;
        vector<Document> docs_found;
        ASSERT(!server.FindTopDocuments("alpha --beta"s, docs_found));
        ASSERT(!server.FindTopDocuments("alpha bet\1a"s, docs_found));
        ASSERT(!server.FindTopDocuments("alpha -"s, docs_found));

        ASSERT(server.FindTopDocuments("alpha betta-gamma"s, docs_found));
    }
    {
        //Проверка match функции
        SearchServer server;
        tuple<vector<string>, DocumentStatus> match;
        ASSERT(!server.MatchDocument("alpha --beta"s, 1, match));
        ASSERT(!server.MatchDocument("alpha bet\02a"s, 1, match));
        ASSERT(!server.MatchDocument("alpha -"s, 1, match));

        ASSERT(server.MatchDocument("alpha betta-gamma"s, 1, match));
    }
}

void TestIdGetting() {
    {
        SearchServer server;
        server.AddDocument(6, "alpha"s, DocumentStatus::ACTUAL, {});
        server.AddDocument(1, "beta"s, DocumentStatus::ACTUAL, {});
        server.AddDocument(3, "gamma"s, DocumentStatus::ACTUAL, {});

        ASSERT_EQUAL(server.GetDocumentId(0), 6);
        ASSERT_EQUAL(server.GetDocumentId(1), 1);
        ASSERT_EQUAL(server.GetDocumentId(2), 3);
        ASSERT_EQUAL(server.GetDocumentId(3), SearchServer::INVALID_DOCUMENT_ID);
        ASSERT_EQUAL(server.GetDocumentId(-1), SearchServer::INVALID_DOCUMENT_ID);
    }
    {
        SearchServer server;
        ASSERT_EQUAL(server.GetDocumentId(0), SearchServer::INVALID_DOCUMENT_ID);
    }
}

void TestAddingDocument() {
    SearchServer server;
    vector<Document> found_docs_pre;
    server.FindTopDocuments("in city"s, found_docs_pre);
    ASSERT_EQUAL(found_docs_pre.size(), 0);

    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "dog at home"s, DocumentStatus::ACTUAL, {1, 15, 3});

    vector<Document> found_docs;
    server.FindTopDocuments("in city"s, found_docs);
    ASSERT_EQUAL(found_docs.size(), 1);
    const Document& doc = found_docs[0];
    ASSERT_EQUAL(doc.id, 1);

    vector<Document> found_docs2;
    server.FindTopDocuments("at home"s, found_docs2);
    ASSERT_EQUAL(found_docs2.size(), 1);
    const Document& doc2 = found_docs2[0];
    ASSERT_EQUAL(doc2.id, 2);

    vector<Document> found_docs3;
    server.FindTopDocuments("cat at the home"s, found_docs3);
    ASSERT_EQUAL(found_docs3.size(), 2);
}

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<Document> found_docs;
        server.FindTopDocuments("in"s, found_docs);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<Document> found_docs;
        server.FindTopDocuments("in"s,found_docs);
        ASSERT_EQUAL(found_docs.size(), 0);
    }
}

void TestMinusWordsExcludeDocs() {
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(44, "fat dog at home"s, DocumentStatus::ACTUAL, {1, 5, 3});
    server.AddDocument(45, "fat rat beat the cat"s, DocumentStatus::ACTUAL, {1, 2, -3});

    vector<Document> found_docs;
    server.FindTopDocuments("cat in home -cat -fat"s, found_docs);
    ASSERT_EQUAL(found_docs.size(), 0);
    vector<Document> found_docs2;
    server.FindTopDocuments("cat in home"s, found_docs2);
    ASSERT_EQUAL(found_docs2.size(), 3);
    vector<Document> found_docs3;
    server.FindTopDocuments("cat in home -fat"s, found_docs3);
    ASSERT_EQUAL(found_docs3[0].id, 42);
    vector<Document> found_docs4;
    server.FindTopDocuments("cat in home -bag"s, found_docs4);
    ASSERT_EQUAL(found_docs4.size(), 3);
}

void TestMatchDocument() {
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::REMOVED, {1, 3, 3});
    server.AddDocument(56, "fat rat in the house"s, DocumentStatus::ACTUAL, {1, 3, 3});

    tuple<vector<string>, DocumentStatus> match;
    server.MatchDocument("cat in home"s, 42, match);
    vector<string>& result_words = get<0>(match);
    sort(result_words.begin(), result_words.end());
    vector<string> words = {"cat"s, "in"s};
    ASSERT_EQUAL(result_words, words);
    ASSERT(get<1>(match) == DocumentStatus::REMOVED);

    tuple<vector<string>, DocumentStatus> match2;
    server.MatchDocument("cat at the city"s, 42, match2);
    ASSERT_EQUAL(get<0>(match2).size(), 3);
    ASSERT(get<1>(match2) == DocumentStatus::REMOVED);

    tuple<vector<string>, DocumentStatus> match3;
    server.MatchDocument("cat the city -at"s, 42, match3);
    ASSERT_EQUAL(get<0>(match3).size(), 3);
    ASSERT(get<1>(match3) == DocumentStatus::REMOVED);

    tuple<vector<string>, DocumentStatus> match4;
    server.MatchDocument("fat cat in city"s, 56, match4);
    ASSERT_EQUAL(get<0>(match4).size(), 2);
    ASSERT(get<1>(match4) == DocumentStatus::ACTUAL);

    tuple<vector<string>, DocumentStatus> match5;
    server.MatchDocument("fat cat in city -rat"s, 56, match5);
    ASSERT_EQUAL(get<0>(match5).size(), 0);
    ASSERT(get<1>(match5) == DocumentStatus::ACTUAL);
}

void TestRelevanceSort() {
    SearchServer server;
    server.AddDocument(1, "fat rat in the house"s, DocumentStatus::ACTUAL, {1, 5, 24});
    server.AddDocument(2, "cat in the city"s, DocumentStatus::ACTUAL, {1, 34, 3});
    server.AddDocument(3, "fat cat in the house"s, DocumentStatus::ACTUAL, {1, 2, 1});

    vector<Document> docs;
    server.FindTopDocuments("cat in city"s, docs);
    ASSERT(docs[0].relevance >= docs[1].relevance);
    ASSERT(docs[1].relevance >= docs[2].relevance);
}

void TestRatingCompute() {
    {
        SearchServer server;
        server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 5, 3});
        vector<Document> docs;
        server.FindTopDocuments("cat in city"s, docs);
        ASSERT_EQUAL(docs[0].rating, 3);
    }
    {
        SearchServer server;
        server.AddDocument(1, "cat"s, DocumentStatus::ACTUAL, {2, -5, -3});
        vector<Document> docs;
        server.FindTopDocuments("cat in city"s, docs);
        ASSERT_EQUAL(docs[0].rating, -2);
    }
    {
        SearchServer server;
        server.AddDocument(1, "fat cat in the house"s, DocumentStatus::ACTUAL, {});
        vector<Document> docs;
        server.FindTopDocuments("cat in house"s, docs);
        ASSERT_EQUAL(docs[0].rating, 0);
    }
}

void TestPredicateFiltering() {
    SearchServer server;
    server.AddDocument(1, "fat rat in the house"s, DocumentStatus::ACTUAL, {1, 5, 3});
    server.AddDocument(2, "cat in the city"s, DocumentStatus::BANNED, {1, 2, 3});
    server.AddDocument(3, "fat cat in the house"s, DocumentStatus::REMOVED, {});

    vector<Document> docs;
    server.FindTopDocuments("cat in city"s, [](const int id, const DocumentStatus status, const int rating) {
        return status == DocumentStatus::REMOVED;
    }, docs);
    ASSERT_EQUAL(docs.size(), 1);
    ASSERT_EQUAL(docs[0].id, 3);

    vector<Document> docs2;
    server.FindTopDocuments("cat in city"s, [](const int id, const DocumentStatus status, const int rating) {
        return id == 1 || id == 2;
    }, docs2);
    ASSERT_EQUAL(docs2.size(), 2);

    vector<Document> docs3;
    server.FindTopDocuments("cat in city"s, [](const int id, const DocumentStatus status, const int rating) {
        return rating == 3;
    }, docs3);
    ASSERT_EQUAL(docs3.size(), 1);
    ASSERT_EQUAL(docs3[0].id, 1);
}

void TestStatusFiltering() {
    SearchServer server;
    server.AddDocument(1, "fat rat in the house"s, DocumentStatus::ACTUAL, {1, 5, 24, 14});
    server.AddDocument(2, "cat in the city"s, DocumentStatus::BANNED, {1, -34, 3});
    server.AddDocument(3, "fat cat in the house"s, DocumentStatus::REMOVED, {0});
    server.AddDocument(4, "fat cat in the house"s, DocumentStatus::IRRELEVANT, {});

    vector<Document> docs;
    server.FindTopDocuments("in the house"s, DocumentStatus::ACTUAL, docs);
    ASSERT_EQUAL(docs.size(), 1);
    ASSERT_EQUAL(docs[0].id, 1);

    vector<Document> docs2;
    server.FindTopDocuments("in the house"s, DocumentStatus::IRRELEVANT, docs2);
    ASSERT_EQUAL(docs2.size(), 1);
    ASSERT_EQUAL(docs2[0].id, 4);

    vector<Document> docs3;
    server.FindTopDocuments("in the house"s, DocumentStatus::REMOVED, docs3);
    ASSERT_EQUAL(docs3.size(), 1);
    ASSERT_EQUAL(docs3[0].id, 3);

    vector<Document> docs4;
    server.FindTopDocuments("in the house"s, DocumentStatus::BANNED, docs4);
    ASSERT_EQUAL(docs4.size(), 1);
    ASSERT_EQUAL(docs4[0].id, 2);

}

void TestRelevanceComputing() {
    SearchServer search_server;
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    vector<Document> docs;
    search_server.FindTopDocuments("пушистый ухоженный кот"s, docs);
    const double EPSILON = 1e-6;
    ASSERT(abs(docs[0].relevance - 0.866434) < EPSILON);
    ASSERT(abs(docs[1].relevance - 0.173287) < EPSILON);
    ASSERT(abs(docs[2].relevance - 0.138629) < EPSILON);
}

void TestSearchServer() {
    RUN_TEST(TestWordsValidate);
    RUN_TEST(TestIdGetting);
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

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;
    return 0;
}