#pragma once

#include "mem.hpp"

namespace mem {

class PageAllocator : public std::enable_shared_from_this<PageAllocator> {

    Offset cr3_;
    size_t pages_count_;
    std::shared_ptr<mem::File> file_;

public:
    PageAllocator(std::shared_ptr<mem::File>& file, Offset cr3, size_t pages_count)
        : cr3_(cr3), pages_count_(pages_count), file_(file) {
    }

    [[nodiscard]] size_t GetPagesCount() const {
        return pages_count_;
    }

    [[nodiscard]] Offset GetCr3() const {
        return cr3_;
    }

    [[nodiscard]] const std::shared_ptr<mem::File>& GetFile() const {
        return file_;
    }

    size_t AllocatePage() {
    }

    void SwapPages(size_t first, size_t second) {
    }

    void FreePage(size_t index) {
    }
};

}  // namespace mem