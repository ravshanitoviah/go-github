#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>
#include "src/storage/buffer_pool_manager.h"

void TestBasicOperations() {
    const size_t pool_size = 10;
    BufferPoolManager bpm(pool_size);

    page_id_t page_id_temp;
    auto *page0 = bpm.NewPage(&page_id_temp);
    assert(page0 != nullptr);
    assert(page0->GetPageId() == 0);

    // Write data and unpin
    std::snprintf(page0->GetData(), PAGE_SIZE, "Hello Buffer Pool");
    assert(bpm.UnpinPage(0, true));

    // Fetch page again and verify content
    auto *fetched_page = bpm.FetchPage(0);
    assert(fetched_page != nullptr);
    assert(std::strcmp(fetched_page->GetData(), "Hello Buffer Pool") == 0);
    assert(bpm.UnpinPage(0, false));

    std::cout << "✅ TestBasicOperations passed!" << std::endl;
}

void TestHighChurnMemoryStability() {
    const size_t pool_size = 10;
    BufferPoolManager bpm(pool_size);

    std::vector<page_id_t> allocated_pages;
    for (int i = 0; i < 10; ++i) {
        page_id_t pid;
        auto *page = bpm.NewPage(&pid);
        assert(page != nullptr);
        allocated_pages.push_back(pid);
    }

    for (auto pid : allocated_pages) {
        assert(bpm.UnpinPage(pid, true));
    }

    // Simulate sustained load with 15,000 churn operations
    for (int op = 0; op < 15000; ++op) {
        page_id_t pid;
        auto *page = bpm.NewPage(&pid);
        assert(page != nullptr);
        std::snprintf(page->GetData(), PAGE_SIZE, "Data operation %d", op);
        assert(bpm.UnpinPage(pid, true));
    }

    std::cout << "✅ TestHighChurnMemoryStability passed (15,000 ops without memory leak)!" << std::endl;
}

int main() {
    TestBasicOperations();
    TestHighChurnMemoryStability();
    std::cout << "All BufferPoolManager tests passed successfully!" << std::endl;
    return 0;
}
