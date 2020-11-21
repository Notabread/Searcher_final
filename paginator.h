#pragma once

#include <iostream>
#include <vector>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end) : begin_(begin), end_(end) {}
    Iterator begin() const {
        return begin_;
    }
    Iterator end() const {
        return end_;
    }
    size_t size() const {
        return  std::distance(begin_, end_);
    }
private:
    Iterator begin_;
    Iterator end_;
};

template <typename Iterator>
std::ostream& operator<<(std::ostream& os, const IteratorRange<Iterator>& range) {
    for (auto it = range.begin(); it != range.end(); ++it) {
        os << *it;
    }
    return os;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator container_begin, Iterator container_end, size_t size) {
        size_t container_size = std::distance(container_begin, container_end);
        for (size_t i = 0; i < container_size; i += size) {
            Iterator current_begin = container_begin;
            std::advance(current_begin, i);
            Iterator current_end = current_begin;
            if (i + size < container_size) {
                std::advance(current_end, size);
            } else {
                current_end = container_end;
            }
            pages_.push_back({
                current_begin,
                current_end
            });
        }
    }
    auto begin() const {
        return pages_.begin();
    }
    auto end() const {
        return pages_.end();
    }
private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(std::begin(c), std::end(c), page_size);
}