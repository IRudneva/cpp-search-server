#pragma once
#include <iostream>
#include <mutex>
#include <vector>
#include <map>
#include <string>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            : guard(bucket.mutex)
            , ref_to_value(bucket.map[key]){
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count) {
    }

    Access operator[](const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return { key, bucket };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        std::lock_guard g(mutex_erase_);
        for (auto& [mutex, map] : buckets_) {
            std::lock_guard g(mutex);
            result.insert(map.begin(), map.end());
        }
        return result;
    }

    size_t Erase(const Key& key) {
        std::lock_guard g1(mutex_erase_);
        size_t mapId = static_cast<uint64_t>(key) % buckets_.size();
        auto& map_for_erase = buckets_.at(mapId);
        std::lock_guard g2(map_for_erase.mutex);
        size_t result = map_for_erase.map.erase(key);
        return result;
    }

private:
    std::vector<Bucket> buckets_;
    std::mutex mutex_erase_;
};

