#pragma once

#include <cstdint>
#include <cstring>

using page_id_t = int32_t;
using frame_id_t = int32_t;

static constexpr size_t PAGE_SIZE = 4096;
static constexpr page_id_t INVALID_PAGE_ID = -1;

class Page {
    friend class BufferPoolManager;
public:
    Page() {
        data_ = new char[PAGE_SIZE]();
        ResetMemory();
    }

    ~Page() {
        delete[] data_;
        data_ = nullptr;
    }

    char *GetData() { return data_; }
    page_id_t GetPageId() const { return page_id_; }
    int GetPinCount() const { return pin_count_; }
    bool IsDirty() const { return is_dirty_; }
    void SetDirty(bool is_dirty) { is_dirty_ = is_dirty; }

private:
    void ResetMemory() {
        if (data_ != nullptr) {
            std::memset(data_, 0, PAGE_SIZE);
        }
        page_id_ = INVALID_PAGE_ID;
        pin_count_ = 0;
        is_dirty_ = false;
    }

    char *data_{nullptr};
    page_id_t page_id_{INVALID_PAGE_ID};
    int pin_count_{0};
    bool is_dirty_{false};
};
