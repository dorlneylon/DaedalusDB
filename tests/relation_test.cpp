
#include "relation.hpp"

#include <ostream>

#include "test.hpp"

TEST(Relation, RelationClass) {
    auto file = util::MakePtr<mem::File>("test.data");

    auto name = ts::NewClass<ts::StringClass>("name");
    auto age = ts::NewClass<ts::PrimitiveClass<int>>("age");

    auto name_to_age = util::MakePtr<ts::RelationClass>("name-to-age", name, age);

    auto holder = util::MakePtr<ts::ClassObject>(name_to_age);
    std::cerr << holder->ToString() << std::endl;
    auto old = holder->ToString();
    holder->Write(file, 0);
    holder->Read(file, 0);
    std::cerr << holder->ToString() << std::endl;
    ASSERT_EQ(old, holder->ToString());

    auto name_to_age_aged = util::MakePtr<ts::RelationClass>("name-to-age-aged", name, age, age);
    holder = util::MakePtr<ts::ClassObject>(name_to_age_aged);
    old = holder->ToString();
    std::cerr << old << std::endl;
    holder->Write(file, 0);
    holder->Read(file, 0);
    std::cerr << holder->ToString() << std::endl;
    ASSERT_EQ(old, holder->ToString());
}

TEST(Relation, RelationObject) {
    auto file = util::MakePtr<mem::File>("test.data");

    auto name = ts::NewClass<ts::StringClass>("name");
    auto age = ts::NewClass<ts::PrimitiveClass<int>>("age");

    auto name_to_age = util::MakePtr<ts::RelationClass>("name-to-age", name, age);

    auto object = util::MakePtr<ts::Relation>(name_to_age, 1, 1);
    std::cerr << object->ToString() << std::endl;
    auto old = object->ToString();
    object->Write(file, 0);
    object->Read(file, 0);
    std::cerr << object->ToString() << std::endl;
    ASSERT_EQ(old, object->ToString());

    auto name_to_age_aged = util::MakePtr<ts::RelationClass>("name-to-age-aged", name, age, age);
    object = util::MakePtr<ts::Relation>(name_to_age_aged, 2, 2,
                                         ts::DefaultNew<ts::Primitive<int>>(age));
    std::cerr << object->ToString() << std::endl;
    old = object->ToString();
    object->Write(file, 0);
    object->Read(file, 0);
    std::cerr << object->ToString() << std::endl;
    ASSERT_EQ(old, object->ToString());
}