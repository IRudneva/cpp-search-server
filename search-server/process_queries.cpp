#include "process_queries.h"

#include <execution>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](std::string i) {return search_server.FindTopDocuments(i);});
    return result;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<Document> result;
    for (const auto& document : ProcessQueries(search_server, queries))
    {
        transform(document.begin(), document.end(), back_inserter(result), [](const Document& i) { return i;});
    }
    return result;
}