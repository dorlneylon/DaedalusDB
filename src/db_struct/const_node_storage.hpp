#pragma once

#include "node_storage.hpp"

namespace db {

class ConstantSizeNodeStorage : public NodeStorage {

public:
    class NodeIterator {
    private:
        mem::Magic magic_;
        std::shared_ptr<ts::Class> node_class_;
        std::shared_ptr<mem::File> file_;
        mem::PageList& page_list_;

        mem::PageOffset inner_offset_;
        mem::PageList::PageIterator current_page_;
        ObjectId id_;
        ObjectId end_;

        std::shared_ptr<Node> curr_;

    public:
        [[nodiscard]] mem::PageOffset InPageOffset() const noexcept {
            return inner_offset_;
        }
        [[nodiscard]] mem::PageList::PageIterator Page() const noexcept {
            return current_page_;
        }

        [[nodiscard]] size_t Size() const {
            return sizeof(mem::Magic) + sizeof(ObjectId) + node_class_->Size().value();
        }

        [[nodiscard]] size_t GetNodesInPage() const {
            return (mem::kPageSize - sizeof(mem::Page)) / Size();
        }

        [[nodiscard]] ObjectId GetInPageIndex() const {
            return (inner_offset_ - sizeof(mem::Page)) / Size();
        }

        NodeIterator& RegenerateId() {
            switch (curr_->State()) {
                case ObjectState::kFree: {
                    id_ = page_list_.GetPagesCount() * GetNodesInPage() + GetInPageIndex();
                    return *this;
                }
                case ObjectState::kValid: {
                    id_ = curr_->Id();
                    return *this;
                }
                case ObjectState::kInvalid:
                default:
                    throw error::BadArgument("No id");
            }
        }

        [[nodiscard]] ObjectId Id() const noexcept {
            return id_;
        }

    private:
        bool IsFree() {
            auto magic =
                file_->Read<mem::Magic>(mem::GetOffset(current_page_->index_, inner_offset_));
            if (magic == magic_) {
                return false;
            } else if (magic == ~magic_) {
                return true;
            } else {
                current_page_ = page_list_.End();
                return true;
                // throw error::RuntimeError("Invalid iterator");
            }
        }

        void Advance() {
            do {
                ++id_;
                if (id_ >= end_) {
                    return;
                }
                if (GetInPageIndex() + 1 <= GetNodesInPage()) {
                    inner_offset_ += Size();
                } else {
                    ++current_page_;
                    inner_offset_ = sizeof(mem::Page);
                }
            } while (IsFree());
        }

        void Retreat() {
            do {
                if (id_ == 0) {
                    return;
                }
                --id_;
                if (GetInPageIndex() >= 1) {
                    inner_offset_ -= Size();
                } else {
                    --current_page_;
                    inner_offset_ = Size() * (GetNodesInPage() - 1) + sizeof(mem::Page);
                }
            } while (IsFree());
        }

        void Read() {
            curr_ = std::make_shared<Node>(magic_, node_class_, file_,
                                           mem::GetOffset(current_page_->index_, inner_offset_));
        }

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = Node;
        using difference_type = size_t;
        using pointer = std::shared_ptr<Node>;
        using reference = Node&;

        NodeIterator(mem::Magic magic, std::shared_ptr<ts::Class>& node_class,
                     std::shared_ptr<mem::File>& file, mem::PageList& page_list, ObjectId id,
                     ObjectId end)
            : magic_(magic),
              node_class_(node_class),
              file_(file),
              page_list_(page_list),
              inner_offset_(sizeof(mem::Page)),
              current_page_(page_list.Begin()),
              id_(0),
              end_(end) {
            for (auto page_it = page_list_.Begin(); page_it != page_list.End(); ++page_it) {
                if (id_ + GetNodesInPage() <= id) {
                    current_page_ = page_it;
                    id_ += GetNodesInPage();
                } else {
                    break;
                }
            }

            while (id_ < id) {
                inner_offset_ += Size();
                ++id_;
            }
            Read();
        }

        NodeIterator(mem::Magic magic, std::shared_ptr<ts::Class>& node_class,
                     std::shared_ptr<mem::File>& file, mem::PageList& page_list,
                     mem::PageList::PageIterator it, mem::PageOffset offset, ObjectId end)
            : magic_(magic),
              node_class_(node_class),
              file_(file),
              page_list_(page_list),
              inner_offset_(offset),
              current_page_(it),
              id_(0),
              end_(end) {
            if (current_page_ != page_list.End()) {
                Read();
                RegenerateId();
            } else {
                id_ = page_list_.GetPagesCount() * GetNodesInPage() + (inner_offset_ / Size());
            }
        }
        NodeIterator& operator++() {
            Advance();
            return *this;
        }
        NodeIterator operator++(int) {
            auto temp = *this;
            Advance();
            return temp;
        }

        NodeIterator& operator--() {
            Retreat();
            return *this;
        }
        NodeIterator operator--(int) {
            auto temp = *this;
            Retreat();
            return temp;
        }

        reference operator*() {
            Read();
            return *curr_;
        }
        pointer operator->() {
            Read();
            return curr_;
        }

        bool operator==(const NodeIterator& other) const {
            return id_ == other.id_;
        }
        bool operator!=(const NodeIterator& other) const {
            return !(*this == other);
        }
    };

    template <ts::ClassLike C>
    ConstantSizeNodeStorage(std::shared_ptr<C> nodes_class,
                            std::shared_ptr<ClassStorage>& class_storage,
                            std::shared_ptr<mem::PageAllocator>& alloc, DEFAULT_LOGGER(logger))
        : NodeStorage(nodes_class, class_storage, alloc, logger) {
        DEBUG("Constant Node storage initialized with class: ",
              ts::ClassObject(nodes_class).ToString());
    }

    NodeIterator Begin() {
        return NodeIterator(GetHeader().magic_, nodes_class_, alloc_->GetFile(), data_page_list_, 0,
                            GetHeader().nodes_);
    }

    NodeIterator End() {
        return NodeIterator(GetHeader().magic_, nodes_class_, alloc_->GetFile(), data_page_list_,
                            GetHeader().nodes_, GetHeader().nodes_);
    }

    template <ts::ObjectLike O>
    requires(!std::is_same_v<O, ts::ClassObject>) void AddNode(std::shared_ptr<O>& node) {

        if (node->Size() + sizeof(mem::Magic) + sizeof(ObjectId) + sizeof(mem::Page) >
            mem::kPageSize) {
            throw error::NotImplemented("Too big Object");
        }
        INFO("Addding node: ", node->ToString());
        auto header = GetHeader();
        auto back = GetBack();
        auto next_free = Node(header.ReadMagic(alloc_->GetFile()).magic_, nodes_class_,
                              alloc_->GetFile(), mem::GetOffset(back.index_, back.free_offset_));
        DEBUG("Free: ", next_free.ToString());
        auto count = header.ReadNodeCount(alloc_->GetFile()).nodes_;

        switch (next_free.State()) {
            case ObjectState::kFree: {
                auto id =
                    NodeIterator(header.ReadMagic(alloc_->GetFile()).magic_, nodes_class_,
                                 alloc_->GetFile(), data_page_list_,
                                 data_page_list_.IteratorTo(back.index_), back.free_offset_, count)
                        .RegenerateId()
                        .Id();
                DEBUG("Rewrited id: ", id);
                DEBUG("Found free space: ", next_free.NextFree());
                Node(header.ReadMagic(alloc_->GetFile()).magic_, id, node)
                    .Write(alloc_->GetFile(), mem::GetOffset(back.index_, back.free_offset_));
                back.free_offset_ = next_free.NextFree();
                INFO("Successfully added node with id: ", id);
            } break;
            case ObjectState::kInvalid: {
                auto metaobject = Node(header.ReadMagic(alloc_->GetFile()).magic_, count, node);
                if (back.initialized_offset_ + metaobject.Size() > mem::kPageSize) {
                    DEBUG("Allocation");
                    AllocatePage();
                    back = GetBack();
                }
                DEBUG("Initializing new memory on id: ", count,
                      ", offset: ", mem::GetOffset(back.index_, back.initialized_offset_));

                metaobject.Write(alloc_->GetFile(), mem::GetOffset(back.index_, back.free_offset_));
                back.free_offset_ += metaobject.Size();
                back.initialized_offset_ += metaobject.Size();
                INFO("Successfully added node with id: ", metaobject.Id());
            } break;
            case ObjectState::kValid:
                throw error::RuntimeError("Already occupied memory");
            default:
                break;
        }
        mem::WritePage(back, alloc_->GetFile());
        DEBUG("Written offsets free: ", back.free_offset_, ", init: ", back.initialized_offset_);
        header.WriteNodeCount(alloc_->GetFile(), ++count);
    }

    template <typename Predicate, typename Functor>
    requires requires {
        requires std::is_invocable_r_v<bool, Predicate, NodeIterator>;
        requires std::is_invocable_v<Functor, Node>;
    }
    void VisitNodes(Predicate predicate, Functor functor) {
        DEBUG("Visiting nodes..");
        DEBUG("Begin page: ", *data_page_list_.Begin());
        auto end = End();
        for (auto node_it = Begin(); node_it != end; ++node_it) {
            if (predicate(node_it)) {
                DEBUG("Node: ", node_it->ToString());
                functor(*node_it);
            }
        }
    }

    template <typename Predicate>
    requires std::is_invocable_r_v<bool, Predicate, NodeIterator>
    void RemoveNodesIf(Predicate predicate) {
        DEBUG("Removing nodes..");
        auto end = End();
        size_t count = 0;
        for (auto node_it = Begin(); node_it != end; ++node_it) {
            if (predicate(node_it)) {
                DEBUG("Removing node ", node_it.Id());
                auto page = mem::ReadPage(*node_it.Page(), alloc_->GetFile());
                auto node = *node_it;
                node.Free(page.free_offset_);
                node.Write(alloc_->GetFile(),
                           mem::GetOffset(node_it.Page()->index_, node_it.InPageOffset()));
                DEBUG("Node: ", node.ToString());
                page.free_offset_ = node_it.InPageOffset();
                DEBUG("Page: ", page);
                mem::WritePage(page, alloc_->GetFile());
                ++count;
            }
        }
        auto header = GetHeader();
        header.WriteNodeCount(alloc_->GetFile(), header.nodes_ - count);
    }
};
}  // namespace db