#include "src/storage/buffer_pool_manager.h"

BufferPoolManager::BufferPoolManager(size_t pool_size) : pool_size_(pool_size) {
    pages_ = new Page[pool_size_];
    for (size_t i = 0; i < pool_size_; ++i) {
        free_list_.push_back(static_cast<frame_id_t>(i));
    }
}

BufferPoolManager::~BufferPoolManager() {
    delete[] pages_;
    pages_ = nullptr;
}

Page *BufferPoolManager::NewPage(page_id_t *page_id) {
    std::lock_guard<std::mutex> lock(latch_);
    frame_id_t frame_id = -1;

    if (!free_list_.empty()) {
        frame_id = free_list_.front();
        free_list_.pop_front();
    } else if (FindVictim(&frame_id)) {
        Page *victim_page = &pages_[frame_id];
        if (victim_page->IsDirty()) {
            FlushPageInternal(victim_page->GetPageId());
        }
        page_table_.erase(victim_page->GetPageId());
    } else {
        return nullptr;
    }

    *page_id = next_page_id_++;
    Page *page = &pages_[frame_id];
    page->ResetMemory();
    page->page_id_ = *page_id;
    page->pin_count_ = 1;
    page->is_dirty_ = false;

    page_table_[*page_id] = frame_id;
    replacer_list_.remove(frame_id);

    return page;
}

Page *BufferPoolManager::FetchPage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(latch_);
    auto iter = page_table_.find(page_id);
    if (iter != page_table_.end()) {
        frame_id_t frame_id = iter->second;
        Page *page = &pages_[frame_id];
        page->pin_count_++;
        replacer_list_.remove(frame_id);
        return page;
    }

    frame_id_t frame_id = -1;
    if (!free_list_.empty()) {
        frame_id = free_list_.front();
        free_list_.pop_front();
    } else if (FindVictim(&frame_id)) {
        Page *victim_page = &pages_[frame_id];
        if (victim_page->IsDirty()) {
            FlushPageInternal(victim_page->GetPageId());
        }
        page_table_.erase(victim_page->GetPageId());
    } else {
        return nullptr;
    }

    Page *page = &pages_[frame_id];
    page->ResetMemory();
    page->page_id_ = page_id;
    page->pin_count_ = 1;
    page->is_dirty_ = false;

    page_table_[page_id] = frame_id;
    replacer_list_.remove(frame_id);

    return page;
}

bool BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) {
    std::lock_guard<std::mutex> lock(latch_);
    auto iter = page_table_.find(page_id);
    if (iter == page_table_.end()) {
        return false;
    }

    frame_id_t frame_id = iter->second;
    Page *page = &pages_[frame_id];

    if (page->GetPinCount() <= 0) {
        return false;
    }

    if (is_dirty) {
        page->SetDirty(true);
    }

    page->pin_count_--;
    if (page->GetPinCount() == 0) {
        replacer_list_.remove(frame_id);
        replacer_list_.push_back(frame_id);
    }

    return true;
}

bool BufferPoolManager::FlushPage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(latch_);
    return FlushPageInternal(page_id);
}

void BufferPoolManager::FlushAllPages() {
    std::lock_guard<std::mutex> lock(latch_);
    for (auto const &[page_id, frame_id] : page_table_) {
        FlushPageInternal(page_id);
    }
}

bool BufferPoolManager::DeletePage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(latch_);
    auto iter = page_table_.find(page_id);
    if (iter == page_table_.end()) {
        return true;
    }

    frame_id_t frame_id = iter->second;
    Page *page = &pages_[frame_id];

    if (page->GetPinCount() > 0) {
        return false;
    }

    page_table_.erase(iter);
    replacer_list_.remove(frame_id);
    page->ResetMemory();
    free_list_.push_back(frame_id);

    return true;
}

bool BufferPoolManager::FindVictim(frame_id_t *frame_id) {
    if (replacer_list_.empty()) {
        return false;
    }
    *frame_id = replacer_list_.front();
    replacer_list_.pop_front();
    return true;
}

bool BufferPoolManager::FlushPageInternal(page_id_t page_id) {
    auto iter = page_table_.find(page_id);
    if (iter == page_table_.end()) {
        return false;
    }
    frame_id_t frame_id = iter->second;
    pages_[frame_id].SetDirty(false);
    return true;
}
