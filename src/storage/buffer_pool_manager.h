#pragma once

#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "src/storage/page.h"

class BufferPoolManager {
public:
    explicit BufferPoolManager(size_t pool_size);
    ~BufferPoolManager();

    size_t GetPoolSize() const { return pool_size_; }

    Page *NewPage(page_id_t *page_id);
    Page *FetchPage(page_id_t page_id);
    bool UnpinPage(page_id_t page_id, bool is_dirty);
    bool FlushPage(page_id_t page_id);
    void FlushAllPages();
    bool DeletePage(page_id_t page_id);

private:
    bool FindVictim(frame_id_t *frame_id);
    bool FlushPageInternal(page_id_t page_id);

    size_t pool_size_;
    page_id_t next_page_id_{0};
    Page *pages_{nullptr};
    std::unordered_map<page_id_t, frame_id_t> page_table_;
    std::list<frame_id_t> replacer_list_;
    std::list<frame_id_t> free_list_;
    std::mutex latch_;
};
