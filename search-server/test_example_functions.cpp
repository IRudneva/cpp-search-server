#include "test_example_functions.h"

using namespace std::string_literals;

void AddDocument(SearchServer& search_server, int document_id, std::string_view document, DocumentStatus status,
    const std::vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const std::exception& e) {
        std::cout << "Error in adding document "s << document_id << ": "s << e.what() << std::endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, std::string_view raw_query) {
    LOG_DURATION_STREAM("Operation time", std::cout);
    std::cout << "Results for request: "s << raw_query << std::endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const std::exception& e) {
        std::cout << "Error is seaching: "s << e.what() << std::endl;
    }
}

void MatchDocuments(const SearchServer& search_server, std::string_view query) {

    LOG_DURATION_STREAM("Operation time", std::cout);
    try {
        std::cout << "Matching for request: "s << query << std::endl;

        for (int document_id : search_server) {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const std::invalid_argument& e) {
        std::cout << "Error in matchig request "s << query << ": "s << e.what() << std::endl;
    }
}