#pragma once

#include <string>
#include <tuple>
#include <map>
#include <set>
#include <algorithm>
#include <vector>
#include <cmath>
#include <execution>
#include <exception>
#include <string_view>

#include "string_processing.h"
#include "document.h"

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:

    SearchServer() = default;

    explicit SearchServer(const std::string_view stop_text) : SearchServer(SplitIntoWords(stop_text)) {}
    explicit SearchServer(const std::string& stop_text) : SearchServer(SplitIntoWords(stop_text)) {}

    template <typename Container>
    explicit SearchServer(const Container& stop_words) {
        using namespace std::literals;
        for(const std::string_view word : stop_words) {
            if (!IsValidWord(word)) {
                throw std::invalid_argument("Stop word has an invalid entry!"s);
            }
        }
        for(const std::string_view word : stop_words) {
            stop_words_.emplace(word);
        }
    }

    void AddDocument(int document_id, const std::string_view document, const DocumentStatus status, const std::vector<int>& ratings);

    /*
     * Основная функция поиска самых подходящих документов по запросу.
     * Для уточнения поиска используется функция предикат.
     */
    template <typename Predicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, const Predicate predicate) const {
        Query query = ParseQuery(raw_query);
        std::vector<Document> matched_documents = FindAllDocuments(query, predicate);
        std::sort(matched_documents.begin(), matched_documents.end(),
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
    std::vector<Document>  FindTopDocuments(const std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    /*
     * Функция, которая возвращает кортеж из вектора совпавших слов из raw_query в документе document_id.
     * Если таких нет или совпало хоть одно минус слово, кортеж возвращается с пустым вектором слов
     * и статусом документа.
     */
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;

    template<typename ExPo>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExPo&& policy, const std::string_view raw_query, int document_id) const {
        Query query = ParseQuery(policy, raw_query);
        std::vector<std::string_view> matched_words;

        if (document_parameters_.count(document_id) == 0) {
            return  make_tuple (matched_words, DocumentStatus::REMOVED);
        }
        for (const std::string_view word : query.minus_words) {
            if (IsWordInDocument(word, document_id)) {
                return make_tuple (matched_words, document_parameters_.at(document_id).status);
            }
        }
        std::for_each(
                policy,
                query.plus_words.begin(), query.plus_words.end(),
                [this, document_id, &matched_words](const std::string_view word){
                    if (IsWordInDocument(word, document_id)) {
                        matched_words.push_back(GetSourceView(word));
                    }
                }
        );
        return make_tuple(
                matched_words,
                document_parameters_.at(document_id).status
        );
    }

    int GetDocumentCount() const;

    std::set<int>::iterator begin();

    std::set<int>::iterator end();

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    //Общая часть удаления документа, которую нельзя сделать параллельной
    void SeqPartRemove(int document_id);

    void RemoveDocument(int document_id);

    template<typename ExPo>
    void RemoveDocument(ExPo&& policy, const int document_id) {
        if (ids_.count(document_id) == 0) {
            return;
        }

        auto& words = id_to_word_freq_[document_id];
        std::for_each(
                policy,
                words.begin(), words.end(),
                [this, document_id](const std::pair<std::string_view, double>& word_to_freq) {
                    std::string_view word = word_to_freq.first;
                    auto word_to_docs_it = word_to_documents_.find(word);

                    std::set<int>& docs_list = word_to_docs_it->second;
                    docs_list.erase(docs_list.find(document_id));
                    if (docs_list.empty()) {
                        word_to_documents_.erase(word_to_docs_it);
                    }
                }
        );

        SeqPartRemove(document_id);
    }

    bool IsWordInDocument(const std::string_view word, const int document_id) const;

    const std::set<int>& DocumentsWithWord(const std::string_view word) const;

private:
    struct DocsParams {
        DocumentStatus status = DocumentStatus::ACTUAL;
        int rating = 0;
    };
    std::set<int> ids_;
    std::map<int, DocsParams> document_parameters_;
    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string, std::set<int>, std::less<>> word_to_documents_;

    std::map<int, std::map<std::string_view, double>> id_to_word_freq_;

    [[nodiscard]] bool IsStopWord(const std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    /*
     * Определяет, являются ли два double числа равными с погрешностью 1e-6.
     */
    [[nodiscard]] static bool IsDoubleEqual(const double first, const double second);

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus = false;
        bool is_stop = false;
    };

    /*
     * Анализ слова.
     * Возвращает структуру со свойствами слова word.
     */
    QueryWord ParseQueryWord(std::string_view word) const;

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    /*
     * Разбивает строку-запрос на плюс и минус слова, исключая стоп слова.
     * Возвращает структуру с двумя множествами этих слов.
     */
    Query ParseQuery(const std::string_view text) const;

    template<typename ExPo>
    Query ParseQuery(ExPo&& policy, const std::string_view text) const {
        using namespace std::literals;
        Query query;
        std::vector<std::string_view> words = SplitIntoWords(policy, text);

        bool error = false;
        std::for_each(
                policy,
                words.begin(), words.end(),
                [&query, this, &error](const std::string_view word){
                    if (error) {
                        return;
                    }

                    const QueryWord query_word = ParseQueryWord(word);
                    if (!IsValidWord(query_word.data)) {
                        error = true;
                        return;
                    }

                    if (!query_word.is_stop) {
                        if (query_word.is_minus) {
                            query.minus_words.insert(query_word.data);
                        } else {
                            query.plus_words.insert(query_word.data);
                        }
                    }
                }
        );
        if (error) throw std::invalid_argument(__FUNCTION__ + " invalid word error!"s);

        return query;
    }

    /*
     * Вычисление IDF слова.
     */
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    /*
     * Проверяет, есть ли в документе с id = document_id минус слово.
     */
    bool HasMinusWord(const int document_id, const std::set<std::string_view>& minus_words) const;

    /*
     * Проверяет, удовлетворяет ли документ требованиям запроса.
     * Сначала идёт проверка через функцию предикат, а потом проверка на минус слово.
     */
    template <typename Predicate>
    [[nodiscard]] bool IsDocumentAllowed(const int document_id, const std::set<std::string_view>& minus_words,
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
    std::vector<Document> FindAllDocuments(const Query& query, const Predicate predicate) const {
        std::map<int, double> document_to_relevance;
        //Проходим по плюс словам и заполняем словарь document_to_relevance
        for (const std::string_view word : query.plus_words) {
            const std::set<int>& documents_with_word = DocumentsWithWord(word);
            if (documents_with_word.empty() || query.minus_words.count(word) > 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

            for (const int document_id : documents_with_word) {
                if (IsDocumentAllowed(document_id, query.minus_words, predicate)) {
                    document_to_relevance[document_id] += id_to_word_freq_.at(document_id)
                            .at(word) * inverse_document_freq;
                }
            }
        }
        //Объявляем и заполняем вектор документов
        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                document_parameters_.at(document_id).rating
            });
        }
        return matched_documents;
    }

    [[nodiscard]] static bool IsValidWord(const std::string_view word);

    std::string_view GetSourceView(std::string_view word) const;

};
