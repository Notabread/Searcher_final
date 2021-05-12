#pragma once

#include <mutex>
#include <map>

template <typename Key, typename Value>
class ConcurrentMap {

    struct Bucket {
        std::mutex guard;
        std::map<Key, Value> data;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
    private:
        std::lock_guard<std::mutex> guard_;
    public:
        Access(Bucket& bucket, uint64_t key) : guard_(bucket.guard), ref_to_value(bucket.data[key]) {};
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) : buckets_(bucket_count), size_(bucket_count) {};

    Access operator[](const Key& key) {
        uint64_t k = key;
        size_t bucket = key % size_;

        return {buckets_[bucket], k};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;

        for (Bucket& bucket : buckets_) {
            std::lock_guard lock(bucket.guard);

            for (auto it = bucket.data.begin(); it != bucket.data.end(); ++it) {
                result[it->first] = it->second;
            }
        }

        return result;
    }

private:

    std::vector<Bucket> buckets_;
    size_t size_ = 0;

};
