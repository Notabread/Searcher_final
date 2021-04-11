#pragma once

#include <string>
#include <tuple>
#include <map>
#include <set>
#include <algorithm>
#include <vector>
#include <cmath>
#include <execution>

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

    explicit SearchServer(const std::string& stop_text) : SearchServer(SplitIntoWords(stop_text)) {}

    template <typename Container>
    explicit SearchServer(const Container& stop_words) {
        using namespace std;
        for(const std::string& word : stop_words) {
            if (!IsValidWord(word)) {
                throw std::invalid_argument("Stop word \""s + word + "\" has an invalid entry!"s);
            }
        }
        for(const std::string& word : stop_words) {
            stop_words_.emplace(word);
        }
    }

    void AddDocument(int document_id, const std::string& document, const DocumentStatus status, const std::vector<int>& ratings);

    /*
     * Основная функция поиска самых подходящих документов по запросу.
     * Для уточнения поиска используется функция предикат.
     */
    template <typename Predicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, const Predicate predicate) const {
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

    std::vector<Document>  FindTopDocuments(const std::string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    /*
     * Функция, которая возвращает кортеж из вектора совпавших слов из raw_query в документе document_id.
     * Если таких нет или совпало хоть одно минус слово, кортеж возвращается с пустым вектором слов
     * и статусом документа.
     */
    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

    int GetDocumentCount() const;

    std::set<int>::iterator begin();

    std::set<int>::iterator end();

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    //Общая часть удаления документа, которую нельзя сделать параллельно
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
                [this, document_id](const std::pair<std::string, double>& word_to_freq){

                    std::set<int>& docs_list = word_to_documents_[word_to_freq.first];
                    auto docs_iterator = docs_list.find(document_id);
                    docs_list.erase(docs_iterator);

                }
        );

        SeqPartRemove(document_id);
    }

    bool IsWordInDocument(const std::string& word, const int document_id) const;

    const std::set<int>& DocumentsWithWord(const std::string& word) const;

private:
    struct DocsParams {
        DocumentStatus status = DocumentStatus::ACTUAL;
        int rating = 0;
    };
    std::set<int> ids_;
    std::map<int, DocsParams> document_parameters_;
    std::set<std::string> stop_words_;
    std::map<std::string, std::set<int>> word_to_documents_;

    std::map<int, std::map<std::string, double>> id_to_word_freq_;

    [[nodiscard]] bool IsStopWord(const std::string& word) const;

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    /*
     * Определяет, являются ли два double числа равными с погрешностью 1e-6.
     */
    [[nodiscard]] static bool IsDoubleEqual(const double first, const double second);

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string data;
        bool is_minus = false;
        bool is_stop = false;
    };

    /*
     * Анализ слова.
     * Возвращает структуру со свойствами слова word.
     */
    QueryWord ParseQueryWord(std::string word) const;

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    /*
     * Разбивает строку-запрос на плюс и минус слова, исключая стоп слова.
     * Возвращает структуру с двумя множествами этих слов.
     */
    Query ParseQuery(const std::string& text) const;

    /*
     * Вычисление IDF слова.
     */
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    /*
     * Проверяет, есть ли в документе с id = document_id минус слово.
     */
    bool HasMinusWord(const int document_id, const std::set<std::string>& minus_words) const;

    /*
     * Проверяет, удовлетворяет ли документ требованиям запроса.
     * Сначала идёт проверка через функцию предикат, а потом проверка на минус слово.
     */
    template <typename Predicate>
    [[nodiscard]] bool IsDocumentAllowed(const int document_id, const std::set<std::string>& minus_words,
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
        for (const std::string& word : query.plus_words) {
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

    [[nodiscard]] static bool IsValidWord(const std::string& word);
};
