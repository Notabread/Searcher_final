#include "unit_tests.h"

using namespace std;

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