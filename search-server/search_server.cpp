#include "search_server.h"

#include <cassert>

using namespace std::string_literals;

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor
                                                     // from string container
{
}

SearchServer::SearchServer(std::string_view stop_words_text) 
    : SearchServer(SplitIntoWords(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("document contains wrong id"s);
    }
    std::vector<std::string_view> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (std::string_view& word : words) {
        std::string s_word{ word };
        if (words_in_docs_.count(s_word) == 0) {
            words_in_docs_[s_word].first = s_word;
            std::string_view sv_word{ words_in_docs_.at(s_word).first };
            words_in_docs_.at(s_word).second = sv_word;
        }
        word_to_document_freqs_[words_in_docs_.at(s_word).second][document_id] += inv_word_count;
        document_word_freqs_[document_id][words_in_docs_.at(std::string(s_word)).second] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, [status](int document_id, DocumentStatus statusp, int rating) { return statusp == status; });
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static std::map<std::string_view, double> words_freqs_empty;

    if (!documents_.count(document_id))
    {
        return words_freqs_empty;
    }

    return document_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
    const auto id_found = find(std::execution::seq, document_ids_.begin(), document_ids_.end(), document_id);
    if (id_found == document_ids_.end()) {
        return;
    }

    for (auto& [word, freqs] : document_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(std::string(word)).erase(document_id);
    }

    document_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(id_found);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id) {

    auto id_found = find(std::execution::par, document_ids_.begin(), document_ids_.end(), document_id);
    if (id_found == document_ids_.end()) {
        return;
    }

    const auto& word_freqs = document_word_freqs_.at(document_id);
    std::vector<std::string_view> words(word_freqs.size(), "");
    transform(std::execution::par,
        word_freqs.begin(), word_freqs.end(),
        words.begin(),
        [](const auto& item)
        { return std::string_view(item.first); }
    );

    for_each(std::execution::par, words.begin(), words.end(),
        [this, document_id](std::string_view word) {
            word_to_document_freqs_.at(std::string(word)).erase(document_id);
        }
    );

    document_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(id_found);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const {
if (document_ids_.count(document_id) == 0) {
        return { {}, {} };
    }
    const QuerySet query = ParseQuerySet(raw_query);

    const auto word_checker =
        [this, document_id](std::string_view word) {
        const auto it = word_to_document_freqs_.find(word);
        return it != word_to_document_freqs_.end() && it->second.count(document_id);
    };

    if (any_of(std::execution::seq,
        query.minus_words.begin(), query.minus_words.end(),
        word_checker)) {
        return { {}, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words(query.plus_words.size());
    auto words_end = copy_if(std::execution::seq,
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        word_checker
    );
    sort(matched_words.begin(), words_end);
    words_end = unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());

    return make_tuple(matched_words, documents_.at(document_id).status);
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const {

    if (document_word_freqs_.count(document_id) == 0) {
        throw std::out_of_range("");
    }
    const auto query = ParseQueryVector(raw_query);
    const auto& word_freqs = document_word_freqs_.at(document_id);

    std::vector<std::string_view> matched_words(query.plus_words.size());

    if (any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [&word_freqs, document_id](const std::string_view word) {return word_freqs.count(word) > 0;}))
    {
        return { {}, documents_.at(document_id).status };
    }

    auto words_end = copy_if(
        std::execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(), [&word_freqs](const std::string_view word) {return word_freqs.count(word) > 0;}
    );
    sort(std::execution::par, matched_words.begin(), words_end);
    words_end = unique(std::execution::par, matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());

    return { matched_words, documents_.at(document_id).status };

}
bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words;
    for (const std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word "s + std::string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    assert(!ratings.empty());
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw std::invalid_argument("Query word "s + std::string(text) + " is invalid"s);
    }
    return { text, is_minus, IsStopWord(text) };
}

SearchServer::QuerySet SearchServer::ParseQuerySet(const std::string_view& text) const {
    QuerySet result;
    for (const std::string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            }
            else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}

SearchServer::QueryVector SearchServer::ParseQueryVector(const std::string_view text) const {
    QueryVector result;
    for (const std::string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}