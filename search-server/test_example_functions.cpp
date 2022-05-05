#include "test_example_functions.h"

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const exception& e) {
        cout << "Error in adding document "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    LOG_DURATION_STREAM("Operation time", cout);
    cout << "Results for request: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const exception& e) {
        cout << "Error is seaching: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    LOG_DURATION_STREAM("Operation time", cout);
    try {
        cout << "Matching for request: "s << query << endl;
        for (auto index = search_server.begin(); index != search_server.end(); ++index) {
            const int document_id = *index;
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception& e) {
        cout << "Error in matchig request "s << query << ": "s << e.what() << endl;
    }
}