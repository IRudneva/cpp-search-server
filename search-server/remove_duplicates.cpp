#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server)
{
    map<vector<string>, int> word_to_document_freqs;
    vector<int> documents_to_delete;

    for (const int document_id : search_server)
    {
        vector<string> words;
       
        for (auto& document : search_server.GetWordFrequencies(document_id))
        {
            words.push_back(document.first);
        }

        if (word_to_document_freqs.count(words) && document_id > word_to_document_freqs[words])
        {
            documents_to_delete.push_back(document_id);
        }
        else
        {
            word_to_document_freqs[words] = document_id;
        }
    }

    for (auto document : documents_to_delete)
    {
        cout << "Found duplicate document id " << document << endl;
        search_server.RemoveDocument(document);
    }
}