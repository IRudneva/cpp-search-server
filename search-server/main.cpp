//#include "search_server.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
#include <sstream>
#include <iostream>
#include <numeric>

using namespace std;

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}
// Добавление документов. Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
void TestAddDocument() {
    const int doc_id_1 = 42;
    const string content_1 = "cat in the city"s;
    const vector<int> ratings_1 = { 1, 2, 3 };

    const int doc_id_2 = 5;
    const string content_2 = "some words cat"s;
    const vector<int> ratings_2 = { 3 };

    SearchServer server;
    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
    const auto found_docs_1 = server.FindTopDocuments("cat city"s);
    ASSERT_EQUAL(found_docs_1.size(), 1u);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
    const auto found_docs_2 = server.FindTopDocuments("cat city"s);
    ASSERT_EQUAL(found_docs_2.size(), 2u);
}
// Поддержка минус-слов. Документы, содержащие минус-слова из поискового запроса, не должны включаться в результаты поиска.
void TestExcludeMinusWordsFromDocument() {
    const int doc_id_1 = 42;
    const int doc_id_2 = 5;
    const string content_1 = "cat in the city"s;
    const string content_2 = "cat in the minus word"s;
    const vector<int> ratings_1 = { 1, 2, 3 };
    const vector<int> ratings_2 = { 1 };
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        const auto found_docs = server.FindTopDocuments("cat city -minus -word"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        ASSERT_HINT(server.FindTopDocuments("-minus -word"s).empty(), "Document containing minus words must be excluded from result"s);
    }
}
// Соответствие документов поисковому запросу. При этом должны быть возвращены все слова из поискового запроса, присутствующие в документе. 
// Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
void TestMatchDocumentSearchQuery() {
    const int doc_id_1 = 42;
    const int doc_id_2 = 5;
    const string content_1 = "cat in the city"s;
    const string content_2 = "cat in the minus"s;
    const vector<int> ratings_1 = { 1, 2, 3 };
    const vector<int> ratings_2 = { 2 };
    const auto query = "cat city -minus"s;

    SearchServer server;
    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
    const auto found_matched_documents_1 = server.MatchDocument(query, doc_id_1);
    tuple<vector<string>, DocumentStatus> expected_result_1 = { { "cat"s, "city"s } , DocumentStatus::ACTUAL };
    ASSERT(found_matched_documents_1 == expected_result_1);
    const auto found_matched_documents_2 = server.MatchDocument(query, doc_id_2);
    tuple<vector<string>, DocumentStatus> expected_result_2 = { {} , DocumentStatus::ACTUAL };
    ASSERT_HINT(found_matched_documents_2 == expected_result_2, "Document containing minus words must be excluded from result. Word list must be empty"s);
}
// Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
void TestSortFoundDocumentsRelevance()
{
    const int doc_id_1 = 4;
    const int doc_id_2 = 8;
    const string content_1 = "cat plus some words"s;
    const string content_2 = "plus some city"s;
    const vector<int> ratings_1 = { 0 };
    const vector<int> ratings_2 = { 0 };

    SearchServer server;
    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
    const auto found_docs = server.FindTopDocuments("plus some words"s);
    ASSERT(found_docs[0].id == doc_id_1);
    ASSERT_HINT(found_docs[1].id == doc_id_2, "Documents must be sorted in descending order of relevance"s);
}
// Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestCalculateRatingAddedDocument()
{
    const int doc_id = 1;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 8, -3 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    const auto expected_result = accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
    const auto found_docs = server.FindTopDocuments("cat in the city"s);
    ASSERT(expected_result == 2);
    ASSERT_EQUAL_HINT(found_docs[0].rating, expected_result, "Rating of the added document must be equal to the arithmetic mean"s);
}
// Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
void TestFilterSearchResultPredicate()
{
    const int doc_id_1 = 1;
    const int doc_id_2 = 4;
    const int doc_id_3 = 0;
    const string content_1 = "cat in the city"s;
    const string content_2 = "cat plus some words"s;
    const string content_3 = "some words"s;
    const vector<int> ratings_1 = { 1, 2, 3 };
    const vector<int> ratings_2 = { 0 };
    const vector<int> ratings_3 = { 3 };

    SearchServer server;
    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
    server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);

    const auto found_docs = server.FindTopDocuments("some words"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    ASSERT_EQUAL(found_docs.size(), 2u);
    ASSERT_EQUAL_HINT(found_docs[0].id % 2, 0, "Condition in predicate is not met"s);
    ASSERT_EQUAL_HINT(found_docs[1].id % 2, 0, "Condition in predicate is not met"s);
}
// Поиск документов, имеющих заданный статус.
void TestSearchDocumentWithStatus() {
    const int doc_id_1 = 1;
    const int doc_id_2 = 4;
    const int doc_id_3 = 0;
    const string content_1 = "cat in the city"s;
    const string content_2 = "cat plus some words"s;
    const string content_3 = "some words"s;
    const vector<int> ratings_1 = { 1, 2, 3 };
    const vector<int> ratings_2 = { 0 };
    const vector<int> ratings_3 = { 3 };

    SearchServer server;
    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::BANNED, ratings_2);
    server.AddDocument(doc_id_3, content_3, DocumentStatus::REMOVED, ratings_3);
    const auto found_docs_1 = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(found_docs_1.size(), 1u);
    ASSERT_EQUAL(found_docs_1[0].id, doc_id_1);
    const auto found_docs_2 = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(found_docs_2.size(), 1u);
    ASSERT_EQUAL(found_docs_2[0].id, doc_id_2);
    const auto found_docs_3 = server.FindTopDocuments("some words"s, DocumentStatus::REMOVED);
    ASSERT_EQUAL(found_docs_3.size(), 1u);
    ASSERT_EQUAL(found_docs_3[0].id, doc_id_3);
}
// Корректное вычисление релевантности найденных документов.
void TestCalculateRelevanceDocument() {
    const int doc_id_1 = 0;
    const int doc_id_2 = 1;
    const int doc_id_3 = 2;
    const string content_1 = "white cat fashion collar"s;
    const string content_2 = "fluffy cat fluffy tail"s;
    const string content_3 = "groomed dog expressive eyes"s;
    const vector<int> ratings_1 = { 1, 2, 3 };
    const vector<int> ratings_2 = { 5 };
    const vector<int> ratings_3 = { 7, -5 };

    SearchServer server;
    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
    server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
    const auto found_docs = server.FindTopDocuments("fluffy groomed cat"s);

    const double test_idf_fluffy = log(static_cast<double>(found_docs.size()) / 1);
    const double test_tf_fluffy_doc_id_1 = 0;
    const double test_idf_groomed = log(static_cast<double>(found_docs.size()) / 1);
    const double test_tf_groomed_doc_id_1 = 0;
    const double test_idf_cat = log(static_cast<double>(found_docs.size()) / 2);
    const double test_tf_cat_doc_id_1 = 0.25;
    const double expected_result = test_tf_fluffy_doc_id_1 * test_idf_fluffy + test_tf_groomed_doc_id_1 * test_idf_groomed + test_tf_cat_doc_id_1 * test_idf_cat;
    ASSERT_EQUAL_HINT(found_docs[2].relevance, expected_result, "Document relevance is calculated incorrectly"s);
}

void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestExcludeMinusWordsFromDocument);
    RUN_TEST(TestMatchDocumentSearchQuery);
    RUN_TEST(TestSortFoundDocumentsRelevance);
    RUN_TEST(TestCalculateRatingAddedDocument);
    RUN_TEST(TestFilterSearchResultPredicate);
    RUN_TEST(TestSearchDocumentWithStatus);
    RUN_TEST(TestCalculateRelevanceDocument);
}

// --------- Окончание модульных тестов поисковой системы -----------
