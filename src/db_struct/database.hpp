#pragma once

#include <type_traits>
#include <vector>

#include "val_node_storage.hpp"
#include "var_node_storage.hpp"

namespace db {

constexpr auto kAll = [](auto) { return true; };

enum class OpenMode { kDefault, kRead, kWrite };

using ValNodeIterator = db::ValNodeStorage::NodeIterator;
using VarNodeIterator = db::VarNodeStorage::NodeIterator;

class Database {

    DECLARE_LOGGER;
    mem::Superblock superblock_;
    mem::File::Ptr file_;
    mem::PageAllocator::Ptr alloc_;
    ClassStorage::Ptr class_storage_;

    void InitializeSuperblock(OpenMode mode) {
        switch (mode) {
            case OpenMode::kRead: {
                DEBUG("OpenMode: Read");
                superblock_.ReadSuperblock(file_);
            } break;
            case OpenMode::kDefault: {
                DEBUG("OpenMode: Default");
                try {
                    superblock_.ReadSuperblock(file_);
                    break;
                } catch (const error::StructureError& e) {
                    ERROR("Can't open file in Read mode, rewriting..");
                } catch (const error::BadArgument& e) {
                    ERROR("Can't open file in Read mode, rewriting..");
                }
            }
            case OpenMode::kWrite: {
                DEBUG("OpenMode: Write");
                file_->Clear();
                superblock_.InitSuperblock(file_);
            } break;
        }
    }

public:
    using Ptr = util::Ptr<Database>;

    Database(const mem::File::Ptr& file, OpenMode mode = OpenMode::kDefault, DEFAULT_LOGGER(logger))
        : LOGGER(logger), file_(file) {

        InitializeSuperblock(mode);

        alloc_ = util::MakePtr<mem::PageAllocator>(file_, LOGGER);
        INFO("Allocator initialized");
        class_storage_ = util::MakePtr<ClassStorage>(alloc_, LOGGER);
    }

    ~Database() {
        INFO("Closing database");
    };

    template <ts::ClassLike C>
    void AddClass(util::Ptr<C>& new_class) {
        class_storage_->AddClass(new_class);
    }

    template <ts::ClassLike C>
    void RemoveClass(util::Ptr<C>& node_class) {
        NodeStorage(node_class, class_storage_, alloc_, LOGGER).Drop();
        class_storage_->RemoveClass(node_class);
    }

    template <ts::ClassLike C>
    bool Contains(util::Ptr<C>& node_class) {
        bool found = 0;
        std::string serialized = node_class->Serialize();
        class_storage_->VisitClasses([&found, &serialized](ts::Class::Ptr stored_class) {
            if (found) {
                return;
            }
            if (stored_class->Serialize() == serialized) {
                found = true;
            }
        });
        return found;
    }

    void PrintAllClasses(std::ostream& os = std::cout) {
        auto& alloc = alloc_;
        class_storage_->VisitClasses([alloc, &os](mem::ClassHeader class_header) -> void {
            ts::ClassObject class_object;
            class_object.Read(alloc->GetFile(),
                              mem::GetOffset(class_header.index_, class_header.free_offset_));
            os << " [ " << class_header.index_ << " ] " << class_object.ToString() << std::endl;
        });
    }

    template <ts::ObjectLike O>
    requires(!std::is_same_v<O, ts::ClassObject>) void AddNode(util::Ptr<O> node) {
        if (node->GetClass()->Size().has_value()) {
            ValNodeStorage(node->GetClass(), class_storage_, alloc_, LOGGER).AddNode(node);
        } else {
            VarNodeStorage(node->GetClass(), class_storage_, alloc_, LOGGER).AddNode(node);
        }
    }

    template <ts::ClassLike C, typename Predicate>
    void RemoveNodesIf(util::Ptr<C> node_class, Predicate predicate) {
        if (node_class->Size().has_value()) {
            if constexpr (std::is_invocable_r_v<bool, Predicate, ValNodeIterator>) {
                ValNodeStorage(node_class, class_storage_, alloc_, LOGGER).RemoveNodesIf(predicate);
            } else {
                ERROR("Bad predicate");
            }

        } else {
            if constexpr (std::is_invocable_r_v<bool, Predicate, VarNodeIterator>) {
                VarNodeStorage(node_class, class_storage_, alloc_, LOGGER).RemoveNodesIf(predicate);
            } else {
                ERROR("Bad predicate");
            }
        }
    }

    // TODO: I'm thinking about implementing some sort of Java StreamAPI-like API in future it
    // requires additional entity Stream or Sequence that will manage several Iterator's and some
    // constraints on them. It will introduce possibilities to chain predicates, zip iterators and
    // do other functional stuff but currently i'll focus on must-have functionality. Maybe somebody
    // or future me will implement this.

    template <ts::ClassLike C, typename Predicate, typename Functor>
    void VisitNodes(util::Ptr<C>& node_class, Predicate predicate, Functor functor) {
        if (node_class->Size().has_value()) {
            if constexpr (std::is_invocable_r_v<bool, Predicate, ValNodeIterator>) {
                ValNodeStorage(node_class, class_storage_, alloc_, LOGGER)
                    .VisitNodes(predicate, functor);
            } else {
                ERROR("Bad predicate");
            }

        } else {
            if constexpr (std::is_invocable_r_v<bool, Predicate, VarNodeIterator>) {
                VarNodeStorage(node_class, class_storage_, alloc_, LOGGER)
                    .VisitNodes(predicate, functor);
            } else {
                ERROR("Bad predicate");
            }
        }
    }

    template <ts::ClassLike C, typename Predicate>
    void PrintNodesIf(util::Ptr<C>& node_class, Predicate predicate, std::ostream& os = std::cout) {
        auto print = [&os](Node node) { os << node.ToString() << std::endl; };
        VisitNodes(node_class, predicate, print);
    }

    template <ts::ObjectLike O, typename Container = std::vector<util::Ptr<O>>, ts::ClassLike C,
              typename Predicate>
    Container CollectNodesIf(util::Ptr<C>& node_class, Predicate predicate) {
        Container result{};
        auto insert = [&result](Node node) { result.push_back(node.Data<O>()); };
        VisitNodes(node_class, predicate, insert);
        return result;
    }

    template <ts::ClassLike C>
    void PrintAllNodes(util::Ptr<C>& node_class) {
        PrintNodesIf(node_class, kAll);
    }

    void Join() {
    }
};

}  // namespace db
