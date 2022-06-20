#pragma once
#include "document.h"
#include "read_input_functions.h"
#include "string_processing.h"
#include "concurrent_map.h"

#include <map>
#include <numeric>
#include <algorithm>
#include <utility>
#include <execution>
#include <stdexcept>
#include <string_view>
#include <cmath>
#include <future>
#include <tuple>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double STANDARD = 1e-6;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit  SearchServer(const std::string& stop_words_text);

    explicit SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy exec_policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy exec_policy, std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy exec_policy, std::string_view raw_query) const;

    int GetDocumentCount() const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);

    void RemoveDocument(const std::execution::parallel_policy& pol, int document_id);

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    std::map<std::string, std::pair<std::string, std::string_view>> words_in_docs_;
    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct QuerySet {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };
    struct QueryVector {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    QuerySet ParseQuerySet(const std::string_view& text) const;

    QueryVector ParseQueryVector(const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const ExecutionPolicy exec_policy, const QuerySet& query, DocumentPredicate document_predicate) const;

    template<typename WordCheckerPlus, typename WordCheckerMinus>
    void ForEachPar(const QuerySet& query, WordCheckerPlus plus_checker, WordCheckerMinus  minus_checker) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy exec_policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    QuerySet query = ParseQuerySet(raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(exec_policy, query, document_predicate);
    sort(exec_policy, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < STANDARD) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const  ExecutionPolicy exec_policy, std::string_view raw_query) const {
    return FindTopDocuments(exec_policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const  ExecutionPolicy exec_policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(exec_policy, raw_query, [status](int document_id, DocumentStatus statusp, int rating) { return statusp == status; });
}

template<typename WordCheckerPlus, typename WordCheckerMinus>
void SearchServer::ForEachPar(const QuerySet& query, WordCheckerPlus plus_checker, WordCheckerMinus minus_checker) const {

    const auto myForeach = [](auto& words, auto& checker) {
        static constexpr int PART_COUNT = 4;
        const auto part_length = std::size(words) / PART_COUNT;
        auto part_begin = words.begin();
        auto part_end = std::next(part_begin, part_length);

        std::vector<std::future<void>> futures;
        for (int i = 0;
            i < PART_COUNT;
            ++i,
            part_begin = part_end,
            part_end = (i == PART_COUNT - 1
                ? words.end()
                : std::next(part_begin, part_length))
            )
        {
            futures.push_back(std::async([checker, part_begin, part_end] {
                std::for_each(part_begin, part_end, checker);
                }));
        }
        return futures;
    };

    std::vector<std::future<void>> plusFutures = myForeach(query.plus_words, plus_checker);
    std::vector<std::future<void>> minusFutures = myForeach(query.minus_words, minus_checker);

    std::for_each(plusFutures.begin(), plusFutures.end(), [](auto& fut) {	fut.wait(); });
    std::for_each(minusFutures.begin(), minusFutures.end(), [](auto& fut) {	fut.wait(); });
}


template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const ExecutionPolicy exec_policy, const QuerySet& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(60);

    const auto plus_word_checker =
        [this, &document_predicate, &document_to_relevance](std::string_view word) {
        if (word_to_document_freqs_.count(word) == 0) {
            return;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id].ref_to_value += static_cast<double>(term_freq * inverse_document_freq);
            }
        }
    };

    const auto minus_word_checker =
        [this, &document_predicate, &document_to_relevance](std::string_view word) {
        if (word_to_document_freqs_.count(word) == 0) {
            return;
        }
        for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.Erase(document_id);
        }
    };

    if constexpr (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
        std::for_each(exec_policy, query.plus_words.begin(), query.plus_words.end(), plus_word_checker);
        std::for_each(exec_policy, query.minus_words.begin(), query.minus_words.end(), minus_word_checker);
    }
    else {
        ForEachPar(query, plus_word_checker, minus_word_checker);
    }

    std::map<int, double> m_doc_to_relevance = document_to_relevance.BuildOrdinaryMap();

    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : m_doc_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}
